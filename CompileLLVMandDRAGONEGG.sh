#!/bin/bash
# run from the base directory of the repository
BASEDIR=$(pwd)"/Generic"

LLVMDIR="$BASEDIR/llvmTip"
DRAGONEGGDIR="$BASEDIR/dragonegg"
CPU2006DIR="$BASEDIR/CPU2006"
GCCPATH="/usr/bin/gcc-4.8"

echo -n 'Install Prerequisites? [y/n]: '
read needPrereq

if [ $needPrereq == 'y' ]; then
echo 'Installing Prerequisites'
sudo apt-get install cmake
sudo apt-get install gcc-4.8
sudo apt-get install gcc-4.8-plugin-dev
sudo apt-get install gfortran-4.8
sudo apt-get install g++-4.8
fi

echo -n 'Build release version? [y/n]: '
read isRelease

echo 'building LLVM'

if [ $isRelease == 'y' ]; then
  if [ ! -d "$LLVMDIR/release_build" ]; then
    mkdir $LLVMDIR/release_build
  fi
  cd $LLVMDIR/release_build
  cmake -DLLVM_TARGETS_TO_BUILD="X86" -DCMAKE_BUILD_TYPE:STRING=Release $LLVMDIR/llvm-master

else
  if [ ! -d "$LLVMDIR/build" ]; then
    mkdir $LLVMDIR/build
  fi
  # The default for cmake is the debug build
  cd $LLVMDIR/build
  cmake -DLLVM_TARGETS_TO_BUILD="X86" $LLVMDIR/llvm-master
fi

make
sudo cmake --build . --target install

echo 'building dragonegg'
cd $DRAGONEGGDIR
make clean
if [ $isRelease == 'y' ]; then
  GCC=$GCCPATH LLVM_CONFIG=$LLVMDIR/release_build/bin/llvm-config make -B
else
  GCC=$GCCPATH LLVM_CONFIG=$LLVMDIR/build/bin/llvm-config make -B
fi
