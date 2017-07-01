#/usr/bin/python3

import sys
import re

RE_DAG_NAME = re.compile(r'INFO: Processing DAG (.*) with')
RE_OPT_DAG_COST = re.compile(r'INFO: DAG solved optimally in (\d+) ms with length=(\d+), spill cost = (\d+),')
RE_DAG_COST = re.compile(r'INFO: DAG timed out with length=(\d+), spill cost = (\d+),')
RE_OPT_LIST_COST = re.compile(r'INFO: The list schedule of length (\d+) and cost (\d+) is optimal.')

dagCosts = []

with open(str(sys.argv[1])) as logfile1:
    log1 = logfile1.read()
    blocks1 = log1.split('INFO: ********** Opt Scheduling **********')

with open(str(sys.argv[2])) as logfile2:
    log2 = logfile2.read()
    blocks2 = log2.split('INFO: ********** Opt Scheduling **********')

for index, block in enumerate(blocks1):
    if (len(RE_DAG_NAME.findall(block)) == 0):
        continue
    
    name = RE_DAG_NAME.findall(block)[0]

    if (len(RE_OPT_DAG_COST.findall(block)) != 0):
        time, length, cost = RE_OPT_DAG_COST.findall(block)[0]
    elif (len(RE_DAG_COST.findall(block)) != 0):
        length, cost = RE_DAG_COST.findall(block)[0]
    elif (len(RE_OPT_LIST_COST.findall(block)) != 0):
        length, cost = RE_OPT_LIST_COST.findall(block)[0]
        
    dagCosts.append((name, int(cost)))

for index, block in enumerate(blocks2):
    if (len(RE_DAG_NAME.findall(block)) == 0):
        continue

    name = RE_DAG_NAME.findall(block)[0]

    if (len(RE_OPT_DAG_COST.findall(block)) != 0):
        time, length, cost = RE_OPT_DAG_COST.findall(block)[0]
    elif (len(RE_DAG_COST.findall(block)) != 0):
        length, cost = RE_DAG_COST.findall(block)[0]
    elif (len(RE_OPT_LIST_COST.findall(block)) != 0):
        length, cost = RE_OPT_LIST_COST.findall(block)[0]

    for entry in dagCosts:
        if (entry[0] == name):
            if (entry[1] < int(cost)):
                print ('DAG ' + name + 'is worse in new test. Length = ' + length)
            #else:
                #print ('DAG ' + name + 'matches')
            continue
