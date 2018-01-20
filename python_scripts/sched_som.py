from __future__ import division
#!/bin/python3
# Find the SOM (Sum of minimums) from the spilling stats generated
# from multiple input scheduling algorithms.
# This script takes one argument, the path to the directory with the test run
# results folder. The test results should be generated with the
# 'runspec-wrapper' script. The directory name is the name of the test run, and
# a spills.dat file should exist in that directory.

import os
import sys
import statistics

# Constants
SPILLS_FILENAME = 'spills.dat'
SPILLS_MIN_FILE = 'spills_min.dat'
SPILLS_STATS_FILE = 'spills_stats.dat'

# Track metrics for each benchmark.
class BenchStats:
    totalFuncs = 0
    totalSpills = 0
    # Number of functions where every test did NOT find a schedule will zero spills.
    funcsWithSpills = 0

    def __init__(self, benchName, testNames):
        self.testNames = testNames
        # The output string for the min_spills file.
        self.benchDataStr = benchName + ':\n'
        # Dict with test names as keys which tracks the number of functions at the
        # minimum.
        self.funcsAtMin = {}
        # The number of spills in this benchmark for each test.
        self.testSpills = {}
        # The maximum number of spills above the minimum for this test (spillsAboveMin, spillsInMin)
        self.maxExtraFunc = {}
        for testName in testNames:
            self.funcsAtMin[testName] = 0
            self.testSpills[testName] = 0
            self.maxExtraFunc[testName] = (0, 0)

# Stats for each test that include all benchmarks.
class TestTotals:
    totalSom = 0
    totalFuncs = 0
    totalFuncsWithSpills = 0

    def __init__(self, testNames):
        self.totalFuncsAtMin = {}
        self.totalTestSpills = {}
        self.totalMaxExtraFunc = {}
        for testName in testNames:
            self.totalFuncsAtMin[testName] = 0
            self.totalTestSpills[testName] = 0
            self.totalMaxExtraFunc[testName] = (0, 0)

# Process all benchmarks for one test run.
def processSpillsFile(spillsFile):
    spillsResult = {}
    data = spillsFile.readlines()
    dataItr = iter(data)

    done = False
    while not done:
        line = next(dataItr).strip()

        # End of file
        if line == '------------':
            done = True

        else:
            benchName = line.split(':')[0]
            spillsResult[benchName] = processBenchmark(dataItr)

    return spillsResult

# Process spill stats for one benchmark.
def processBenchmark(dataItr):
    benchResult = {}
    done = False
    while not done:
        line = next(dataItr).strip()

        # End of benchmark
        if line == "---------":
            # seek 2 forward from the current position
            for i in range(2):
                next(dataItr)
            done = True

        else:
            spills, funcName = line.split(' ')
            benchResult[funcName] = spills;

    return benchResult

# Collect SOM stats and generate output files.
def generateSOMFiles(somData):
    try:
        with open(os.path.join(SPILLS_MIN_FILE), 'w') as minSpillsFile, \
        open(os.path.join(SPILLS_STATS_FILE), 'w') as spillsStatsFile:
            # Iterate through benchmarks using a random entry in somData,
            # all tests must have the same benchmarks.
            testName = next(iter(somData))
            totals = TestTotals(iter(somData))
            # Collect stats for each benchmark, add totals for all tests
            for bench in somData[testName]:
                benchData = generateBenchStats(bench, somData)
                # Wrtie minimums for each function to min spills file.
                minSpillsFile.write(benchData.benchDataStr)

                # Update global totals for all benchmarks
                totals.totalSom += benchData.totalSpills
                totals.totalFuncs += benchData.totalFuncs
                totals.totalFuncsWithSpills += benchData.funcsWithSpills

                # Calculate aggregate stats for each benchmarks.
                for testName in iter(somData):
                    totals.totalFuncsAtMin[testName] += benchData.funcsAtMin[testName]
                    totals.totalTestSpills[testName] += benchData.testSpills[testName]
                    if benchData.maxExtraFunc[testName][0] > totals.totalMaxExtraFunc[testName][0]:
                        totals.totalMaxExtraFunc[testName] = (benchData.maxExtraFunc[testName][0], benchData.maxExtraFunc[testName][1])


            # Wrtie SOM total spills to per-function min spills file
            somString = '-'*12 + '\n' + 'Total:'  + str(totals.totalSom)
            minSpillsFile.write(somString)

            # Write details for each test to stats file
            testStr = 'Spilling Statistics:\n\n'
            testStr += "\n{:<25}{:>30}{:>25}{:>25}\n".format('Heuristic', 'Extra Spills', '%Funcs at min', 'Max extra per func')
            testStr += '---------------------------------------------------------------------------------------------------------\n'


            for testName in sorted(totals.totalTestSpills, key=totals.totalTestSpills.__getitem__):
                extraSpills = totals.totalTestSpills[testName] - totals.totalSom
                extraSpillsP = "{:.2%}".format(extraSpills / totals.totalSom)
                funcAtMinP = "{:.2%}".format(totals.totalFuncsAtMin[testName] / totals.totalFuncsWithSpills)

                maxExtraSpills = totals.totalMaxExtraFunc[testName][0]
                minSpillsInFuncWithMaxExtra = totals.totalMaxExtraFunc[testName][1]
                try:
                    maxExtraSpillsP = maxExtraSpills / minSpillsInFuncWithMaxExtra
                except ZeroDivisionError:
                    maxExtraSpillsP = -1

                maxExtraPerFuncStr = 'inf' if maxExtraSpillsP == -1 else "{:.2%}".format(maxExtraSpillsP)

                #stdev = findStdev(somData[testName])
                testStr += "{:<30}{:>17}{:>9}{:>21}{:>28}\n".format(testName, \
                                                         "{:,}".format(extraSpills), \
                                                         "({})".format(extraSpillsP), \
                                                         funcAtMinP, \
                                                         "{:>7} {:>7} {:>7}".format(maxExtraSpills, "({})".format(minSpillsInFuncWithMaxExtra), \
                                                         maxExtraPerFuncStr))

            testStr += '---------------------------------------------------------------------------------------------------------\n\n'

            spillsStatsFile.write(testStr)

            # Write SOM and other metrics to stats file
            statsStr = '-------------------------------------------------------\n'
            statsStr += "{:<30}{:>25}\n".format('Total Functions:', "{:,}".format(totals.totalFuncs))
            statsStr += "{:<30}{:>25}\n".format('Functions with spills:', "{:,}".format(totals.totalFuncsWithSpills))
            statsStr += "{:<30}{:>25}\n".format('Sum of minima (SOM):', "{:,}".format(totals.totalSom))
            statsStr += '-------------------------------------------------------\n'
            spillsStatsFile.write(statsStr)


    except IOError as error:
        print ('Fatal: Could not create function min spills file.')
        raise

def findStdev(data):
    spillsList = []
    for bench in data:
        for func in data[bench]:
            # Get spills for this function
            spillsList.append(int(data[bench][func]))
    return statistics.stdev(spillsList)

# Find min spills for this benchmarks across all tests.
def generateBenchStats(benchName, somData):
    # Verify that this benchmark exists in all tests, and that the benchmark
    # has the same number of functions in each test.
    testName = next(iter(somData))
    numberOfFunctions = len(somData[testName][benchName])
    for test in somData:
        if somData[test][benchName] is None:
            print ('Error: Benchmark ' + benchName + ' does not exist in test ' + testName)
        if len(somData[test][benchName]) != numberOfFunctions:
            print ('Error: Benchmark ' + benchName + ' does not have the same number of functions in ' + test + ' as in ' + testName)

    benchData = BenchStats(benchName, iter(somData))
    benchData.totalFuncs = numberOfFunctions
    for func in somData[testName][benchName]:
        # Tests which generated the minimum number of spills.
        minTests = []
        # The minimum number of spills for this function.
        minSpills = sys.maxsize
        for test in somData:
            spills = int(somData[test][benchName][func])
            benchData.testSpills[test]+=spills
            # Is this the minimum number of spills for this function
            if spills == minSpills:
                minTests.append(test)
            elif spills < minSpills:
                minTests = []
                minTests.append(test)
                minSpills = spills

        # Check for Max extra spills above min in a function
        for test in somData:
            spills = int(somData[test][benchName][func])
            diff = spills - minSpills
            if (diff > benchData.maxExtraFunc[test][0]):
                benchData.maxExtraFunc[test] = (diff, minSpills)

        # Format output data for this function
        bestTests = '[All]' if len(minTests) == len(somData) else str(minTests)

        # Is this a function with spills.
        if not (bestTests == '[All]' and minSpills == 0):
            benchData.funcsWithSpills+=1

            # Track every test that is at the minimum for this fucntion.
            for test in minTests:
                benchData.funcsAtMin[test]+=1

        benchData.benchDataStr += ' '*10 + str(minSpills) + ' ' + func + ' ' + bestTests + '\n'
        # Add function spills to total.
        benchData.totalSpills += minSpills

    benchData.benchDataStr += '  ---------\n' + 'Sum: ' + str(benchData.totalSpills) + '\n\n'
    return benchData


if __name__ == '__main__':
    somData = {}

    # Find all test run direcotires.
    dirNames = os.listdir(sys.argv[1])
    # Gather spill data for each test run.
    for dirName in dirNames:
        try:
            with open(os.path.join(dirName, SPILLS_FILENAME)) as spillsFile:
                somData[dirName] = processSpillsFile(spillsFile)

        except IOError as error:
            print ('Error: Could not open spills file.')
            print (error)

    # Find SOM stats and generate SOM spills and stats files.
    generateSOMFiles(somData)
