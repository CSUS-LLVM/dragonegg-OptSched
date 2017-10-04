#!/bin/bash

# This bash script will run performance tests for the SLIL cost function for the following scenarios:
#   1. Take all blocks
#   2. Take only optimal blocks
#   3. Take only zero-cost blocks
# Each scenario will run the scheduler for all functions and for hot functions.

SCHED_INI_DIR="/home/chris/csc199/LLVM_DRAGONEGG/Generic/OptSchedCfg/"

TEST_DIR="/home/chris/csc199/test_100/"
TEST_DIR_SHARED="/home/chris/csc199/test_100_2017_09_23_chris/"

CPU2006_DIR="/media/ssd0/CPU2006"
CPU2006_USER_DIR="/media/ssd0/chris/spec"

RUNSPEC_SCRUB="runspec --loose -size=ref -iterations=1 -config=Intel_llvm_3.9_chris.cfg --tune=base -r 1 -I -a scrub all"

function clean_dirs() {
    echo runspec --loose -size=ref -iterations=1 -config=Intel_llvm_3.9_chris.cfg --tune=base -r 1 -I -a scrub all
    runspec --loose -size=ref -iterations=1 -config=Intel_llvm_3.9_chris.cfg --tune=base -r 1 -I -a scrub all
    echo rm -R $CPU2006_DIR/wrapper* $CPU2006_USER_DIR/result/*
    rm -R $CPU2006_DIR/wrapper* $CPU2006_USER_DIR/result/*
}

#  FUNCTION ARGUMENTS:
#    $1: sched.ini file that contains the preconfigured settings
#    $2: name of test
#    $3: subfolder of result
function run_test() {
    clean_dirs

    echo cp "$SCHED_INI_DIR/$1" "$SCHED_INI_DIR/sched.ini"
    cp "$SCHED_INI_DIR/$1" "$SCHED_INI_DIR/sched.ini"

    echo python runspec-wrapper-chris.py
    python runspec-wrapper-chris.py
    
    echo cp "$SCHED_INI_DIR/$1" "$SCHED_INI_DIR/sched.ini"
    cp "$SCHED_INI_DIR/$1" "$SCHED_INI_DIR/sched.ini"
    
    RESULT_DIR="$TEST_DIR/$3"
    RESULT_DIR_SHARED="$TEST_DIR_SHARED/$3"
    if [ ! -d "$RESULT_DIR" ]; then
        echo mkdir "$RESULT_DIR"
        mkdir "$RESULT_DIR"
    fi
    if [ ! -d "$RESULT_DIR_SHARED" ]; then
        echo mkdir "$RESULT_DIR_SHARED"
        mkdir "$RESULT_DIR_SHARED"
    fi
    
    echo cp "$CPU2006_DIR/wrapper*" "$SCHED_INI_DIR/sched.ini" "$RESULT_DIR"
    cp -R $CPU2006_DIR/wrapper* $SCHED_INI_DIR/sched.ini $RESULT_DIR
    
    echo cp "$CPU2006_DIR/wrapper*" "$SCHED_INI_DIR/sched.ini" "$RESULT_DIR"
    cp $CPU2006_DIR/wrapperStats/*.dat $SCHED_INI_DIR/sched.ini $RESULT_DIR_SHARED
}

if [ ! -d "$TEST_DIR" ]; then
    echo "Output folder $TEST_DIR doesn't exist. Creating it now."
    echo mkdir "$TEST_DIR"
    mkdir "$TEST_DIR"
fi

if [ ! -d "$TEST_DIR_SHARED" ]; then
    echo mkdir "$TEST_DIR_SHARED"
    mkdir "$TEST_DIR_SHARED"
fi

echo "Using $TEST_DIR to collect log files and stat files."

echo cd "$CPU2006_DIR"
cd "$CPU2006_DIR"
echo source shrc
source shrc

run_test "sched.slil1.20.ini" "" "slil1_20ms/"
run_test "sched.peak1.20.ini" "" "peak1_20ms/"
run_test "sched.slil1.100.ini" "" "slil1_100ms/"
run_test "sched.peak1.100.ini" "" "peak1_100ms/"

run_test "sched.slil2.20.ini" "" "slil2_20ms/"
run_test "sched.peak2.20.ini" "" "peak2_20ms/"
run_test "sched.slil2.100.ini" "" "slil2_100ms/"
run_test "sched.peak2.100.ini" "" "peak2_100ms/"

run_test "sched.slil3.20.ini" "" "slil3_20ms/"
run_test "sched.peak3.20.ini" "" "peak3_20ms/"
run_test "sched.slil3.100.ini" "" "slil3_100ms/"
run_test "sched.peak3.100.ini" "" "peak3_100ms/"

run_test "sched.slil4.20.ini" "" "slil4_20ms/"
run_test "sched.peak4.20.ini" "" "peak4_20ms/"
run_test "sched.slil4.100.ini" "" "slil4_100ms/"
run_test "sched.peak4.100.ini" "" "peak4_100ms/"
