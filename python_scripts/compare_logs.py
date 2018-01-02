#/usr/bin/python3

import sys
import re

RE_DAG_COST = re.compile(r"INFO: Best schedule for DAG (.*) has cost (\d+) and length (\d+). The schedule is (.*) \(Time")

dags1 = {}
dags2 = {}

with open(str(sys.argv[1])) as logfile1:
    log1 = logfile1.read()
    for dag in RE_DAG_COST.finditer(log1):
        dagName = dag.group(1)
        cost = dag.group(2)
        length = dag.group(3)
        isOptimal = dag.group(4)
        dags1[dagName] = {}
        dags1[dagName]['cost'] = int(cost)
        dags1[dagName]['length'] = int(length)
        dags1[dagName]['isOptimal'] = (isOptimal == 'optimal')

with open(str(sys.argv[2])) as logfile2:
    log2 = logfile2.read()
    for dag in RE_DAG_COST.finditer(log2):
        dagName = dag.group(1)
        cost = dag.group(2)
        length = dag.group(3)
        isOptimal = dag.group(4)
        dags2[dagName] = {}
        dags2[dagName]['cost'] = int(cost)
        dags2[dagName]['length'] = int(length)
        dags2[dagName]['isOptimal'] = (isOptimal == 'optimal')

numDagsLog1 = len(dags1)
numDagsLog2 = len(dags2)
# The number of blocks that are optimal in both logs.
optimalInBoth = 0
# The number of blocks that are only optimal in log 1.
optimalLog1 = 0
# The number of blocks that are only optimal in log 2.
optimalLog2 = 0

if numDagsLog1 != numDagsLog2:
    print('Error: Different number of dags in each log file.')

print(str(numDagsLog1) + ' blocks in log file 1.')
print(str(numDagsLog2) + ' blocks in log file 2.')

for dagName in dags1:
    if dagName not in dags2:
        print('Error: Could not find ' + dagName + ' in the second log file.')
        continue

    dag1 = dags1[dagName]
    dag2 = dags2[dagName]
    if dag1['isOptimal'] and dag2['isOptimal']:
        optimalInBoth+=1
        if dag1['cost'] != dag2['cost']:
            print('Mismatch for dag ' + dagName)

    elif dag1['isOptimal']:
        optimalLog1+=1
        #print('dag1 is optimal and dag2 is not for ' + dagName)
        if dag1['cost'] > dag2['cost']:
            print('Mismatch for dag ' + dagName)

    elif dag2['isOptimal']:
        optimalLog2+=1
        #print('dag2 is optimal and dag1 is not for ' + dagName)
        if dag2['cost'] > dag1['cost']:
            print('Mismatch for dag ' + dagName)

print(str(optimalInBoth) + ' blocks are optimal in both files.')
print(str(optimalLog1) + ' blocks are optimal in log 1 but not in log 2.')
print(str(optimalLog2) + ' blocks are optimal in log 2 but not in log 1.')
