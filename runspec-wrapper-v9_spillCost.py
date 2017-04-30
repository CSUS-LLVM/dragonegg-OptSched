#!/usr/bin/python2

from __future__ import division
import optparse
import re
import subprocess
import sys

# Configuration.
INT_BENCHMARKS = [
  'perlbench',
  'bzip2',
  'gcc',
  'mcf',
  'gobmk',
  'hmmer',
  'sjeng',
  'libquantum',
  'h264ref',
  'omnetpp',
  'astar',
  'xalancbmk'
]
FP_BENCHMARKS = [
  'bwaves',
  'gamess',
  'milc',
  'zeusmp',
  'gromacs',
  'cactus',
  'leslie',
  'namd',
  'dealII',
  'soplex',
  'povray',
  'calculix',
  'Gems',
  'tonto',
  'lbm',
  'wrf',
  'sphinx'
]
ALL_BENCHMARKS = INT_BENCHMARKS + FP_BENCHMARKS
COMMAND = "runspec --loose -size=ref -iterations=1 -config=Intel_llvm_3.3.cfg --tune=base -r 1 -I -a build %s"

# Regular expressions.
SETTING_REGEX = re.compile(r'\bUSE_OPT_SCHED\b.*')
SPILLS_REGEX = re.compile(r'GREEDY RA: Number of spilled live ranges: (\d+)')
#SPILLS_REGEX = re.compile(r'Spill Cost: (\d+)')
TIMES_REGEX = re.compile(r'(\d+) total seconds elapsed')
BLOCK_NAME_AND_SIZE_REGEX = re.compile(r'Processing DAG (.*) with (\d+) insts')
#BLOCK_NOT_ENUMERATED_REGEX = re.compile(r'The list schedule .* is optimal')
BLOCK_NOT_ENUMERATED_REGEX = re.compile(r'Bypassing optimal scheduling due to zero time limit')
BLOCK_ENUMERATED_OPTIMAL_REGEX = re.compile(r'DAG solved optimally')
BLOCK_COST_REGEX = re.compile(r'list schedule is of length \d+ and spill cost \d+. Tot cost = (\d+)')
BLOCK_IMPROVEMENT_REGEX = re.compile(r'cost imp=(\d+)')
BLOCK_START_TIME_REGEX = re.compile(r'-{20} \(Time = (\d+) ms\)')
BLOCK_END_TIME_REGEX = re.compile(r'verified successfully \(Time = (\d+) ms\)')
BLOCK_LIST_FAILED_REGEX = re.compile(r'List scheduling failed')

def writeStats(stats, spills, times, blocks):
  # Write times.
  with open(times, 'w') as times_file:
    total_time = 0
    for benchName in stats:
      time = stats[benchName]['time']
      total_time += time
      times_file.write('%10s:%5d seconds\n' % (benchName, time))
    times_file.write('---------------------------\n')
    times_file.write('     Total:%5d seconds\n' % total_time)

  # Write spill stats.
  with open(spills, 'w') as spills_file:
    total_spills = 0
    for benchName in stats:
      spills = stats[benchName]['spills']
      total_spills += sum(spills)
      spills_file.write('%s:\n' % benchName)
      for spill in spills:
        spills_file.write('      %5d\n' % spill)
      spills_file.write('  ---------\n')
      spills_file.write('  Sum:%5d\n\n' % sum(spills))
    spills_file.write('------------\n')
    spills_file.write('Total:%5d\n' % total_spills)

  # Write block stats.
  with open(blocks, 'w') as blocks_file:
    totalCount = 0
    totalSuccessful = 0
    totalEnumerated = 0
    totalOptimalImproved = 0
    totalOptimalNotImproved = 0
    totalTimedOutImproved = 0
    totalTimedOutNotImproved = 0
    totalCost = 0
    totalImprovement = 0
    sizes = []
    enumeratedSizes = []
    optimalSizes = []
    improvedSizes = []
    timedOutSizes = []
    optimalTimes = []

    for benchName in stats:
      blocks = stats[benchName]['blocks']
      count = 0
      successful = 0
      enumerated = 0
      optimalImproved = 0
      optimalNotImproved = 0
      timedOutImproved = 0
      timedOutNotImproved = 0
      cost = improvement = 0

      for block in blocks:
        count += 1
        if block['success']:
          successful += 1
          cost += block['listCost']
          sizes.append(block['size'])
          if block['isEnumerated']:
            enumerated += 1
            improvement += block['improvement']
            enumeratedSizes.append(block['size'])
            if block['isOptimal']:
              optimalTimes.append(block['time'])
              optimalSizes.append(block['size'])
              if block['improvement'] > 0:
                improvedSizes.append(block['size'])
                optimalImproved += 1
              else:
                optimalNotImproved += 1
            else:
              timedOutSizes.append(block['size'])
              if block['improvement'] > 0:
                improvedSizes.append(block['size'])
                timedOutImproved += 1
              else:
                timedOutNotImproved += 1

      blocks_file.write('%s:\n' % benchName)
      blocks_file.write('  Blocks: %d\n' %
                        count)
      blocks_file.write('  Successful: %d (%.2f%%)\n' %
                        (successful, (100 * successful / count) if count else 0))
      blocks_file.write('  Enumerated: %d (%.2f%%)\n' %
                        (enumerated, (100 * enumerated / successful) if successful else 0))
      blocks_file.write('  Optimal and Improved: %d (%.2f%%)\n' %
                        (optimalImproved, (100 * optimalImproved / enumerated) if enumerated else 0))
      blocks_file.write('  Optimal but not Improved: %d (%.2f%%)\n' %
                        (optimalNotImproved, (100 * optimalNotImproved / enumerated) if enumerated else 0))
      blocks_file.write('  Non-Optimal and Improved: %d (%.2f%%)\n' %
                        (timedOutImproved, (100 * timedOutImproved / enumerated) if enumerated else 0))
      blocks_file.write('  Non-Optimal and not Improved: %d (%.2f%%)\n' %
                        (timedOutNotImproved, (100 * timedOutNotImproved / enumerated) if enumerated else 0))
      blocks_file.write('  Cost improvement: %d (%.2f%%)\n' %
                        (improvement, (100 * improvement / cost) if cost else 0))

      totalCount += count
      totalSuccessful += successful
      totalEnumerated += enumerated
      totalOptimalImproved += optimalImproved
      totalOptimalNotImproved += optimalNotImproved
      totalTimedOutImproved += timedOutImproved
      totalTimedOutNotImproved += timedOutNotImproved
      totalCost += cost
      totalImprovement += improvement

    blocks_file.write('-' * 50 + '\n')
    blocks_file.write('Total:\n')
    blocks_file.write('  Blocks: %d\n' %
                      totalCount)
    blocks_file.write('  Successful: %d (%.2f%%)\n' %
                      (totalSuccessful, (100 * totalSuccessful / totalCount) if totalCount else 0))
    blocks_file.write('  Enumerated: %d (%.2f%%)\n' %
                      (totalEnumerated, (100 * totalEnumerated / totalSuccessful) if totalSuccessful else 0))
    blocks_file.write('  Optimal and Improved: %d (%.2f%%)\n' %
                      (totalOptimalImproved, (100 * totalOptimalImproved / totalEnumerated) if totalEnumerated else 0))
    blocks_file.write('  Optimal but not Improved: %d (%.2f%%)\n' %
                      (totalOptimalNotImproved, (100 * totalOptimalNotImproved / totalEnumerated) if totalEnumerated else 0))
    blocks_file.write('  Non-Optimal and Improved: %d (%.2f%%)\n' %
                      (totalTimedOutImproved, (100 * totalTimedOutImproved / totalEnumerated) if totalEnumerated else 0))
    blocks_file.write('  Non-Optimal and not Improved: %d (%.2f%%)\n' %
                      (totalTimedOutNotImproved, (100 * totalTimedOutNotImproved / totalEnumerated) if totalEnumerated else 0))
    blocks_file.write('  Cost improvement: %d (%.2f%%)\n' %
                      (totalImprovement, (100 * totalImprovement / totalCost) if totalCost else 0))

    blocks_file.write('  Smallest block size: %s\n' %
                      (min(sizes) if sizes else 'none'))
    blocks_file.write('  Largest block size: %s\n' %
                      (max(sizes) if sizes else 'none'))
    blocks_file.write('  Average block size: %.1f\n' %
                      ((sum(sizes) / len(sizes)) if sizes else 0))
    blocks_file.write('  Smallest enumerated block size: %s\n' %
                      (min(enumeratedSizes) if enumeratedSizes else 'none'))
    blocks_file.write('  Largest enumerated block size: %s\n' %
                      (max(enumeratedSizes) if enumeratedSizes else 'none'))
    blocks_file.write('  Average enumerated block size: %.1f\n' %
                      ((sum(enumeratedSizes) / len(enumeratedSizes)) if enumeratedSizes else 0))
    blocks_file.write('  Largest optimal block size: %s\n' %
                      (max(optimalSizes) if optimalSizes else 'none'))
    blocks_file.write('  Largest improved block size: %s\n' %
                      (max(improvedSizes) if improvedSizes else 'none'))
    blocks_file.write('  Smallest timed out block size: %s\n' %
                      (min(timedOutSizes) if timedOutSizes else 'none'))
    blocks_file.write('  Average optimal solution time: %d ms\n' %
                      ((sum(optimalTimes) / len(optimalTimes)) if optimalTimes else 0))


def calculateBlockStats(output):
  blocks = output.split('Opt Scheduling **********')[1:]
  stats = []
  for index, block in enumerate(blocks):
    lines = [line[6:] for line in block.split('\n') if line.startswith('INFO:')]
    block = '\n'.join(lines)

    try:
      name, size = BLOCK_NAME_AND_SIZE_REGEX.findall(block)[0]

      failed = BLOCK_LIST_FAILED_REGEX.findall(block) != []
      if failed:
        timeTaken = 0
        isEnumerated = isOptimal = False
        listCost = improvement = 0
      else:
        start_time = int(BLOCK_START_TIME_REGEX.findall(block)[0])
        end_time = int(BLOCK_END_TIME_REGEX.findall(block)[0])
        timeTaken = end_time - start_time

        listCost = int(BLOCK_COST_REGEX.findall(block)[0])
        isEnumerated = BLOCK_NOT_ENUMERATED_REGEX.findall(block) == []
        if isEnumerated:
          isOptimal = bool(BLOCK_ENUMERATED_OPTIMAL_REGEX.findall(block))
          improvement = int(BLOCK_IMPROVEMENT_REGEX.findall(block)[0])
        else:
          isOptimal = False
          improvement = 0

      stats.append({
        'name': name,
        'size': int(size),
        'time': timeTaken,
        'success': not failed,
        'isEnumerated': isEnumerated,
        'isOptimal': isOptimal,
        'listCost': listCost,
        'improvement': improvement
      })
    except:
      print '  WARNING: Could not parse block #%d:' % (index + 1)
      print "Unexpected error:", sys.exc_info()[0]
      for line in blocks[index].split('\n')[1:-1][:10]:
        print '   ', line

  return stats


def runBenchmarks(benchmarks):
  results = {}

  for bench in benchmarks:
    print 'Running', bench
    try:
      p = subprocess.Popen('/bin/bash', stdin=subprocess.PIPE, 
                           stdout=subprocess.PIPE)
      p.stdin.write("source shrc" + "\n"); 
      p.stdin.write(COMMAND % bench + "\n");
      p.stdin.write("runspec --loose -size=ref -iterations=1 -config=Intel_llvm_3.3.cfg --tune=base -r 1 -I -a scrub %s" % bench);
      p.stdin.close()
      output = p.stdout.read();

    except subprocess.CalledProcessError as e:
      print '  WARNING: Benchmark command failed: %s.' % e
    else:
      results[bench] = {
        'time': int(TIMES_REGEX.findall(output)[0]),
        'spills': [int(i) for i in SPILLS_REGEX.findall(output)],
        'blocks': calculateBlockStats(output)
      }

  return results


def main(args):
  # Select benchmarks.
  if args.bench == 'ALL':
    benchmarks = ALL_BENCHMARKS
  elif args.bench == 'INT':
    benchmarks = INT_BENCHMARKS
  elif args.bench == 'FP':
    benchmarks = FP_BENCHMARKS
  else:
    benchmarks = args.bench.split(',')
    for bench in benchmarks:
      if bench not in ALL_BENCHMARKS:
        print 'WARNING: Unknown benchmark specified: "%s".' % bench

  # Run the benchmarks and collect results.
  if args.opt is not None:

    # Temporarily adjust the INI file.
    with open(args.ini) as ini_file: ini = ini_file.read()
    new_ini = SETTING_REGEX.sub('USE_OPT_SCHED ' + args.opt, ini)
    with open(args.ini, 'w') as ini_file: ini_file.write(new_ini)
    try:
      results = runBenchmarks(banchmarks)
    finally:
      with open(args.ini, 'w') as ini_file: ini_file.write(ini)
  else:
    results = runBenchmarks(benchmarks)

  # Write out the results.
  writeStats(results, args.spills, args.times, args.blocks)


if __name__ == '__main__':
  parser = optparse.OptionParser(
      description='Wrapper around runspec for collecting spill counts.')
  parser.add_option('-s', '--spills',
                    metavar='filepath',
                    default='spills.dat',
                    help='Where to write the spill counts (%default).')
  parser.add_option('-t', '--times',
                    metavar='filepath',
                    default='times.dat',
                    help='Where to write the run compile times (%default).')
  parser.add_option('-k', '--blocks',
                    metavar='filepath',
                    default='blocks.dat',
                    help='Where to write the run block stats (%default).')
  parser.add_option('-i', '--ini',
                    metavar='filepath',
                    default='../OptSchedCfg/sched.ini',
                    help='Where to find sched.ini (%default).')
  parser.add_option('-o', '--opt',
                    metavar='YES|NO|HOTONLY',
                    default=None,
                    choices=['YES', 'NO', 'HOTONLY'],
                    help='Override the USE_OPT_SCHED setting in sched.ini.')
  parser.add_option('-b', '--bench',
                    metavar='ALL|INT|FP|name1,name2...',
                    default='ALL',
                    help='Which benchmarks to run.')
  main(parser.parse_args()[0])
