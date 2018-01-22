#!/bin/bash
# This script runs dragonegg with OptSched and the options you want to enable
# Run this script from the base directory of the repository
# Update the path to the config files you want to load below
# Options:
# -dopt            : print OptSched debugging info
# -dms             : print msched debugging info
# -g++             : compile with g++
# -o filename      : the name for the output binary
# -p               : print command string so it can be copied
# example: ./RunOptSched.sh -dopt -g++ -o test test.cpp

BASEDIR=$(dirname $0)"/Generic"
OPTSCHEDCFG="$BASEDIR/OptSchedCfg"
DRAGONEGGPATH="$BASEDIR/dragonegg/dragonegg.so"
GCCPATH="/usr/bin"
CLANGPATH="$BASEDIR/llvmTip/build/bin"
LLVMOPT="-fplugin-arg-dragonegg-llvm-option="

args="-mllvm -misched=optsched -O3 -mllvm -optsched-cfg=$OPTSCHEDCFG"
cc=clang

# process command line arguments
while [[ $# -gt 0 ]]; do
    key="$1"

    case $key in
        -dopt)
            args="$args -mllvm -debug-only=optsched"
            ;;
        -dms)
            args="$args -mllvm -debug-only=misched"
            ;;
        -g++)
            cc=clang++
            ;;
        -p)
            PRINT='True'
            ;;
        -*)
            echo "Unknown argument: \"$key\""; exit 1
            ;;
        *)
            break
            ;;
    esac
    shift
done

#create command string
COMMAND="$CLANGPATH/$cc $args $@"

if [[ $PRINT == 'True' ]]; then
  echo $COMMAND
else
  $COMMAND
fi
