#!/bin/sh
#Build benchmarks and copy them to the A7 machine.

BENCH="401.bzip2 445.gobmk 456.hmmer 464.h264ref 470.lbm 482.sphinx3 444.namd"

. ./shrc
#runspec --loose -size=ref -iterations=1 -config=Intel_llvm_3.9.cfg --tune=base -r 1 -I -a $BENCH &> /dev/null
runspec --fake --loose --size test --tune base --config Intel_llvm_3.9.cfg $BENCH
cd ./benchspec/CPU2006/
tar cJvf ziped_benches.tar.xz $BENCH
scp ziped_benches.tar.xz ghassan@99.113.71.118:~
