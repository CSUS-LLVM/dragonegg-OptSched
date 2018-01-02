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
COMMAND = "runspec --loose -size=ref -iterations=1 -config=Intel_llvm_3.9.austin.cfg --tune=base -r 1 -I -a build %s"

# Regular expressions.
SETTING_REGEX = re.compile(r'\bUSE_OPT_SCHED\b.*')
# SPILLS_REGEX = re.compile(r'Function: (.*?)\nEND FAST RA: Number of spills: (\d+)\n')
SPILLS_REGEX = re.compile(r'GREEDY RA: Number of spilled live ranges: (\d+)')
TIMES_REGEX = re.compile(r'(\d+) total seconds elapsed')
BLOCK_NAME_AND_SIZE_REGEX = re.compile(r'Processing DAG (.*) with (\d+) insts')
BLOCK_NOT_ENUMERATED_REGEX = re.compile(r'The list schedule .* is optimal')
BLOCK_ZERO_TIME_LIMIT = re.compile(
    r'Bypassing optimal scheduling due to zero time limit')
BLOCK_ENUMERATED_OPTIMAL_REGEX = re.compile(r'DAG solved optimally')
BLOCK_COST_REGEX = re.compile(
    r'list schedule is of length \d+ and spill cost \d+. Tot cost = (\d+)')
BLOCK_IMPROVEMENT_REGEX = re.compile(r'cost imp=(\d+)')
BLOCK_START_TIME_REGEX = re.compile(r'-{20} \(Time = (\d+) ms\)')
BLOCK_END_TIME_REGEX = re.compile(r'verified successfully \(Time = (\d+) ms\)')
BLOCK_LIST_FAILED_REGEX = re.compile(r'List scheduling failed')
BLOCK_RP_MISMATCH = re.compile(r'RP-mismatch falling back!')
BLOCK_PEAK_REG_PRESSURE_REGEX = re.compile(
    r'PeakRegPresAfter  Index (\d+) Name (.*) Peak (\d+) Limit (\d+)')
BLOCK_PEAK_REG_BLOCK_NAME = re.compile(
    r'LLVM max pressure after scheduling for BB (\S+)')
REGION_OPTSCHED_SPILLS_REGEX = re.compile(
    r"INFO: OPT_SCHED LOCAL RA: DAG Name: (\S+) Number of spills: (\d+) \(Time")


def writeStats(stats, spills, times, blocks, regp):
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
        totalSpills = 0
        for benchName in stats:
            totalSpillsPerBenchmark = 0
            spills_file.write('%s:\n' % benchName)
            spills = stats[benchName]['spills']
            for functionName in spills:
                spillCount = spills[functionName]
                totalSpillsPerBenchmark += spillCount
                spills_file.write('      %5d %s\n' %
                                  (spillCount, functionName))
            spills_file.write('  ---------\n')
            spills_file.write('  Sum:%5d\n\n' % totalSpillsPerBenchmark)
            totalSpills += totalSpillsPerBenchmark
        spills_file.write('------------\n')
        spills_file.write('Total:%5d\n' % totalSpills)

        # for benchName in stats:
        #   spills = stats[benchName]['spills']
        #   total_spills += sum(spills)
        #   spills_file.write('%s:\n' % benchName)
        #   for spill in spills:
        #     spills_file.write('      %5d\n' % spill)
        #   spills_file.write('  ---------\n')
        #   spills_file.write('  Sum:%5d\n\n' % sum(spills))
        # spills_file.write('------------\n')
        # spills_file.write('Total:%5d\n' % total_spills)

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
        totalOptSchedSpills = 0
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
            optSchedSpills = 0

            for block in blocks:
                count += 1
                if block['success']:
                    successful += 1
                    cost += block['listCost']
                    sizes.append(block['size'])
                    optSchedSpills += block['optSchedSpills']
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
            blocks_file.write('  Heuristic cost: %d\n' %
                              cost)
            blocks_file.write('  B&B cost: %d\n' %
                              (cost - improvement))
            blocks_file.write('  Cost improvement: %d (%.2f%%)\n' %
                              (improvement, (100 * improvement / cost) if cost else 0))
            blocks_file.write('  Region Spills: %d\n' %
                              optSchedSpills)

            totalCount += count
            totalSuccessful += successful
            totalEnumerated += enumerated
            totalOptimalImproved += optimalImproved
            totalOptimalNotImproved += optimalNotImproved
            totalTimedOutImproved += timedOutImproved
            totalTimedOutNotImproved += timedOutNotImproved
            totalCost += cost
            totalImprovement += improvement
            totalOptSchedSpills += optSchedSpills

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
        blocks_file.write('  Heuristic cost: %d\n' %
                          totalCost)
        blocks_file.write('  B&B cost: %d\n' %
                          (totalCost - totalImprovement))
        blocks_file.write('  Cost improvement: %d (%.2f%%)\n' %
                          (totalImprovement, (100 * totalImprovement / totalCost) if totalCost else 0))
        blocks_file.write('  Total Region Spills: %d\n' %
                          totalOptSchedSpills)
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

    # Write peak pressure stats
    with open(regp, 'w') as regp_file:
        for benchName in stats:
            regp_file.write('Benchmark %s:\n' % benchName)
            regpressure = stats[benchName]['regpressure']
            numberOfFunctionsWithPeakExcess = 0
            numberOfBlocksWithPeakExcess = 0
            numberOfBlocks = 0
            peakPressureSetSumsPerBenchmark = {}
            for functionName in regpressure:
                peakPressureSetSums = {}
                regp_file.write('  Function %s:\n' % functionName)
                listOfBlocks = regpressure[functionName]
                if len(listOfBlocks) == 0:
                    continue
                for blockName, listOfExcessPressureTuples in listOfBlocks:
                    numberOfBlocks += 1
                    if len(listOfExcessPressureTuples) == 0:
                        continue
                    # regp_file.write('    Block %s:\n' % blockName)
                    for setName, peakExcessPressure in listOfExcessPressureTuples:
                        # If we ever enter this loop, that means there exists a peak excess pressure
                        # regp_file.write('      %5d %s\n' % (peakExcessPressure, setName))
                        if not setName in peakPressureSetSums:
                            peakPressureSetSums[setName] = peakExcessPressure
                        else:
                            peakPressureSetSums[setName] += peakExcessPressure
                regp_file.write(
                    '  Pressure Set Sums for Function %s:\n' % functionName)
                for setName in peakPressureSetSums:
                    regp_file.write('    %5d %s\n' %
                                    (peakPressureSetSums[setName], setName))
                    if not setName in peakPressureSetSumsPerBenchmark:
                        peakPressureSetSumsPerBenchmark[setName] = peakPressureSetSums[setName]
                    else:
                        peakPressureSetSumsPerBenchmark[setName] += peakPressureSetSums[setName]
            regp_file.write(
                'Pressure Set Sums for Benchmark %s:\n' % benchName)
            for setName in peakPressureSetSumsPerBenchmark:
                regp_file.write('%5d %s\n' % (
                    peakPressureSetSumsPerBenchmark[setName], setName))
            # regp_file.write('Number of blocks with peak excess:    %d\n' % numberOfBlocksWithPeakExcess)
            # regp_file.write('Number of blocks total:               %d\n' % numberOfBlocks)
            # regp_file.write('Number of functions with peak excess: %d\n' % numberOfFunctionsWithPeakExcess)
            # regp_file.write('Number of functions total:            %d\n' % len(regpressure))
            regp_file.write('------------\n')


def calculatePeakPressureStats(output):
    """
    Output should look like:
    Benchmark:
      Function1:
        Block1:
          PERP1 RegName1
          PERP2 RegName2
          ...
      Function1 Peak: PERPMax RegNameMax
    Number of blocks with peak excess register pressure: M
    Number of blocks total: N
    Number of functions with excess register pressure: F
    Number of functions total: G

    Then the data structure will look like:
    {
      function1: [
        (block1, [(SetName1, PERP1), ...]),
        ...
        ],
      function2: ...
    }
    """
    blocks = output.split('Scheduling **********')[1:]
    functions = {}
    for block in blocks:
        if len(BLOCK_PEAK_REG_BLOCK_NAME.findall(block)) == 0:
            continue
        dagName = BLOCK_PEAK_REG_BLOCK_NAME.findall(block)[0]
        functionName = dagName.split(':')[0]
        blockName = dagName.split(':')[1]
        pressureMatches = BLOCK_PEAK_REG_PRESSURE_REGEX.findall(block)
        peakExcessPressures = []
        for indexString, name, peakString, limitString in pressureMatches:
            peak = int(peakString)
            limit = int(limitString)
            excessPressure = peak - limit
            if excessPressure < 0:
                excessPressure = 0
            element = tuple((name, excessPressure))
            peakExcessPressures.append(element)
        if len(peakExcessPressures) > 0:
            blockStats = (blockName, peakExcessPressures)
            if not functionName in functions:
                functions[functionName] = []
            functions[functionName].append(blockStats)
    return functions


def calculateBlockStats(output):
    blocks = output.split('Opt Scheduling **********')[1:]
    stats = []
    for index, block in enumerate(blocks):
        lines = [line[6:]
                 for line in block.split('\n') if line.startswith('INFO:')]
        block = '\n'.join(lines)

        try:
            name, size = BLOCK_NAME_AND_SIZE_REGEX.findall(block)[0]

            failed = BLOCK_LIST_FAILED_REGEX.findall(
                block) != [] or BLOCK_RP_MISMATCH.findall(block) != []

            if failed:
                timeTaken = 0
                isEnumerated = isOptimal = False
                listCost = improvement = 0
            else:
                start_time = int(BLOCK_START_TIME_REGEX.findall(block)[0])
                end_time = int(BLOCK_END_TIME_REGEX.findall(block)[0])
                timeTaken = end_time - start_time
                optSchedSpills = int(
                    REGION_OPTSCHED_SPILLS_REGEX.findall(block)[0][1])

                listCost = int(BLOCK_COST_REGEX.findall(block)[0])
                # The block is not enumerated if the list schedule is optimal or there is a zero
                # time limit for enumeration.
                isEnumerated = (BLOCK_NOT_ENUMERATED_REGEX.findall(block) == []) or (
                    BLOCK_ZERO_TIME_LIMIT.findall(block) == [])
                if isEnumerated:
                    isOptimal = bool(
                        BLOCK_ENUMERATED_OPTIMAL_REGEX.findall(block))
                    """
                    Sometimes the OptScheduler doesn't print out cost improvement.
                    This happens when the scheduler determines that the list schedule is
                    already optimal, which means no further improvements can be made.

                    The rest of this if-block ensures that this tool doesn't crash.
                    If the improvement is not found, it is assumed to be 0.
                    """
                    matches = BLOCK_IMPROVEMENT_REGEX.findall(block)
                    if matches == []:
                        improvement = 0
                    else:
                        improvement = int(matches[0])
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
                'improvement': improvement,
                'optSchedSpills': optSchedSpills
            })
        except:
            print '  WARNING: Could not parse block #%d:' % (index + 1)
            print "Unexpected error:", sys.exc_info()[0]
            for line in blocks[index].split('\n')[1:-1][:10]:
                print '   ', line
                raise

    return stats


def calculateSpills(output):
    spills = {}
    for functionName, spillCountString in SPILLS_REGEX.findall(output):
        spills[functionName] = int(spillCountString)
    return spills


"""
Defining this function makes it easier to parse files.
"""


def getBenchmarkResult(output):
    return {
        'time': int(TIMES_REGEX.findall(output)[0]),
        'spills': calculateSpills(output),
        'blocks': calculateBlockStats(output),
        'regpressure': calculatePeakPressureStats(output)
    }


def runBenchmarks(benchmarks):
    results = {}

    for bench in benchmarks:
        print 'Running', bench
        try:
            p = subprocess.Popen('/bin/bash', stdin=subprocess.PIPE,
                                 stdout=subprocess.PIPE)
            p.stdin.write("source shrc" + "\n")
            p.stdin.write(COMMAND % bench + "\n")
            p.stdin.write(
                "runspec --loose -size=ref -iterations=1 -config=Intel_llvm_3.9.austin.cfg --tune=base -r 1 -I -a scrub %s" % bench)
            p.stdin.close()
            output = p.stdout.read()

        except subprocess.CalledProcessError as e:
            print '  WARNING: Benchmark command failed: %s.' % e
        else:
            results[bench] = getBenchmarkResult(output)

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

    # Parse single log file instead of running benchmark
    results = {}
    if args.logfile is not None:
        with open(args.logfile) as log_file:
            output = log_file.read()
            results[args.logfile] = getBenchmarkResult(output)
    # Run the benchmarks and collect results.
    elif args.opt is not None:

        # Temporarily adjust the INI file.
        with open(args.ini) as ini_file:
            ini = ini_file.read()
        new_ini = SETTING_REGEX.sub('USE_OPT_SCHED ' + args.opt, ini)
        with open(args.ini, 'w') as ini_file:
            ini_file.write(new_ini)
        try:
            results = runBenchmarks(banchmarks)
        finally:
            with open(args.ini, 'w') as ini_file:
                ini_file.write(ini)
    else:
        results = runBenchmarks(benchmarks)

    # Write out the results.
    writeStats(results, args.spills, args.times, args.blocks, args.regp)


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
    parser.add_option('-r', '--regp',
                      metavar='filepath',
                      default='regp.dat',
                      help='Where to write the reg pressure stats (%default).')

    # Add the ability to parse log files.
    parser.add_option('-l', '--logfile',
                      metavar='CPU2006.###.log',
                      default=None,
                      help='Parse log file instead of running benchmark. Only single-benchmark log files produce valid statistics.')
    main(parser.parse_args()[0])
