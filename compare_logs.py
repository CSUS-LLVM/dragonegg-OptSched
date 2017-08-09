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

if len(dags1) != len(dags2):
    print('Error: Different number of dags in each log file')

for dagName in dags1:
    if dagName not in dags2:
        print('Error: Could not find ' + dagName + ' in the second log file.')
        continue

    dag1 = dags1[dagName]
    dag2 = dags2[dagName]
    if dag1['isOptimal'] and dag2['isOptimal']:
        if dag1['cost'] != dag2['cost']:
            print('Mismatch for dag ' + dagName)

    elif dag1['isOptimal']:
        print('dag1 is optimal and dag2 is not for ' + dagName)
        if dag1['cost'] > dag2['cost']:
            print('Mismatch for dag ' + dagName)

    elif dag2['isOptimal']:
        print('dag2 is optimal and dag1 is not for ' + dagName)
        if dag2['cost'] > dag1['cost']:
            print('Mismatch for dag ' + dagName)
