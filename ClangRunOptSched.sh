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

# update path to input files
#####################################
OPTSCHEDCFG="$BASEDIR/OptSchedCfg"
MACHINEMODELCONFIG="machine_model.cfg "
SCHEDULEINIFILE="sched.ini "
HOTFUNCTIONSINIFILE="hotfuncs.ini "
######################################

DRAGONEGGPATH="$BASEDIR/dragonegg/dragonegg.so"
GCCPATH="/usr/bin"
CLANGPATH="$BASEDIR/llvmTip/build/bin"
LLVMOPT="-fplugin-arg-dragonegg-llvm-option="
CLANGOPT="-mllvm "
LLVMOPTSCHED="$CLANGOPT"'-optsched -O3 '
MCFG="$CLANGOPT-optsched-mcfg=$OPTSCHEDCFG/$MACHINEMODELCONFIG "
SINI="$CLANGOPT-optsched-sini=$OPTSCHEDCFG/$SCHEDULEINIFILE "
HFINI="$CLANGOPT-optsched-hfini=$OPTSCHEDCFG/$HOTFUNCTIONSINIFILE "

# process command line arguments
while [[ $# -gt 0 ]]
do
key="$1"

case $key in
  -dopt)
  DOPT="$CLANGOPT-debug-only=optsched "
  ;;
  -dms)
  DMS="$CLANGOPT-debug-only=misched "
  ;;
  -g++)
  GPP='True'
  ;;
  -o)
  OUTPUT="-o $2 "
  shift
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

if [[ $GPP == 'True' ]] 
then
  CLANGPATH=$CLANGPATH'/clang++ '
else
  CLANGPATH=$CLANGPATH'/clang '
fi

INPUT=$1

#create command string
COMMAND="$CLANGPATH$LLVMOPTSCHED$DOPT$DMS$MCFG$SINI$HFINI$OUTPUT$INPUT"
#COMMAND="$GCCPATH-fplugin=$DRAGONEGGPATH $LLVMOPTSCHED$DOPT$DMS$MCFG$SINI$HFINI$OUTPUT$INPUT"

if [[ $PRINT == 'True' ]]
then
  echo $COMMAND
else
  $COMMAND
fi
