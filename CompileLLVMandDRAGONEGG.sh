#!/bin/bash

# update the path to the extracted "Generic" directory
BASEDIR="/home/austin/Documents/LLVMDRAGONEGG/Generic"

LLVMDIR="$BASEDIR/llvmTip"
DRAGONEGGDIR="$BASEDIR/dragonegg"
CPU2006DIR="$BASEDIR/CPU2006"
GCCPATH="/usr/bin/gcc-4.8"

echo -n 'Install Prerequisites? [y/n]: '
read answer

if [ $answer == 'y' ]; then
echo 'Installing Prerequisites'
sudo apt-get install cmake
sudo apt-get install gcc-4.8
sudo apt-get install gcc-4.8-plugin-dev
sudo apt-get install gfortran-4.8
sudo apt-get install g++-4.8
fi

echo 'building LLVM'

if [ ! -d "$LLVMDIR/build" ]; then
mkdir $LLVMDIR/build
fi

cd $LLVMDIR/build
cmake $LLVMDIR/llvm-master
make
sudo cmake --build . --target install

echo 'building dragonegg'
cd $DRAGONEGGDIR
make clean
GCC=$GCCPATH LLVM_CONFIG=$LLVMDIR/build/bin/llvm-config make -B
