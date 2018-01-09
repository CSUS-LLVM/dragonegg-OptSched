#!/bin/python3
# Find the SOM (Sum of minimums) from the spilling stats generated
# from multiple input scheduling algorithms.
# This script takes one argument, the path to the directory with the test run
# results folder. The test results should be generated with the
# 'runspec-wrapper' script. The directory name is the name of the test run, and
# a spills.dat file should exist in that directory.

import os
import sys

# Constants
SPILLS_FILENAME = 'spills.dat'

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
            spills, functName = line.split(' ')
            benchResult[functName] = spills;

    return benchResult

# Collect SOM stats and generate output file.
def generateSOMStats(somData):
    try:
        with open(os.path.join('spills_min.dat'), 'w') as minSpillsFile:
            # Iterate through benchmarks using a random entry in somData,
            # all tests must have the same benchmarks.
            testName = next(iter(somData))
            # SOM
            som = 0
            for bench in somData[testName]:
                benchData = generateBenchStats(bench, testName, somData)
                minSpillsFile.write(benchData[0])
                som+=benchData[1]

            # Wrtie SOM
            somString = '-'*12 + '\n' + 'Total:'  + str(som)
            minSpillsFile.write(somString)



    except IOError as error:
        print ('Fatal: Could not create spills_min.dat file.')
        raise

# Find min spills for this benchmarks across all tests.
def generateBenchStats(benchName, testName, somData):
    benchData = benchName + ':\n'
    # Verify that this benchmark exists in all tests, and that the benchmark
    # has the same number of functions in each test.
    numberOfFunctions = len(somData[testName][benchName])
    for test in somData:
        if somData[test][benchName] is None:
            print ('Error: Benchmark ' + benchName + ' does not exist in test ' + testName)
            return
        if len(somData[test][benchName]) != numberOfFunctions:
            print ('Error: Benchmark ' + benchName + ' does not have the same number of functions in ' + test + ' as in ' + testName)
            return

    benchSOMSpills = 0
    for funct in somData[testName][benchName]:
        # Tests which generated the minimum number of spills.
        minTests = []
        # The minimum number of spills for this function.
        minSpills = sys.maxsize
        for test in somData:
            spills = int(somData[test][benchName][funct])
            if spills == minSpills:
                minTests.append(test)
            elif spills < minSpills:
                minTests = []
                minTests.append(test)
                minSpills = spills

        # Format output data for this function
        bestTests = '[All]' if len(minTests) == len(somData) else str(minTests)
        benchData += ' '*10 + str(minSpills) + ' ' + funct + ' ' + bestTests + '\n'
        # Add function spills to total.
        benchSOMSpills += minSpills

    benchData += '  ---------\n' + 'Sum: ' + str(benchSOMSpills) + '\n\n'
    return benchData, benchSOMSpills


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

    # Find SOM stats and generate SOM spills.dat file.
    generateSOMStats(somData)
