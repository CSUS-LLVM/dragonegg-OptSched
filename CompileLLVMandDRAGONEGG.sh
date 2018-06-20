#!/bin/bash
# run from the base directory of the repository
BASEDIR=$(pwd)"/Generic"

LLVMDIR="$BASEDIR/llvmTip"
DRAGONEGGDIR="$BASEDIR/dragonegg"
CPU2006DIR="$BASEDIR/CPU2006"
GCCPATH="/usr/bin/gcc-4.8"

read -p 'Install Prerequisites on Ubuntu? [y/n] (default: n): ' -r

if [[ $REPLY =~ ^([yY][eE][sS]|[yY])+$ ]]
then
  echo 'Installing Prerequisites'
  sudo apt-get install cmake
  sudo apt-get install gcc-4.8
  sudo apt-get install gcc-4.8-plugin-dev
  sudo apt-get install gfortran-4.8
  sudo apt-get install g++-4.8
fi

read -p 'Build release version? [y/n] (default: y): ' -r

if [[ $REPLY =~ ^([yY][eE][sS]|[yY])+$ || -z "$REPLY" ]]
then
  isRelease='True'
else
  isRelease='False'
fi

read -p 'Choose target architecture (default: X86): ' -r targetArch
if [ -z "$targetArch" ]
then
  targetArch='X86'
fi
echo "Targeting $targetArch"

# Create build directory and call cmake
if [ $isRelease == 'True' ]
then
  if [ ! -d "$LLVMDIR/release_build" ]
  then
    mkdir $LLVMDIR/release_build
  fi
  cd $LLVMDIR/release_build
  cmake -DLLVM_TARGETS_TO_BUILD=$targetArch -DCMAKE_BUILD_TYPE="Release" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON $LLVMDIR/llvm-master
  cp compile_commands.json $BASEDIR

else
  if [ ! -d "$LLVMDIR/build" ]
  then
    mkdir $LLVMDIR/build
  fi
  # The default for cmake is the debug build
  cd $LLVMDIR/build
  cmake -DLLVM_TARGETS_TO_BUILD=$targetArch -DCMAKE_EXPORT_COMPILE_COMMANDS=ON $LLVMDIR/llvm-master
  cp compile_commands.json $BASEDIR
fi

# Build LLVM/clang
echo 'Building LLVM'
make

read -p 'Install LLVM/clang? [y/n] (default: n): ' -r
if [[ $REPLY =~ ^([yY][eE][sS]|[yY])+$ ]]
then
  sudo cmake --build . --target install
fi

cd $BASEDIR

echo 'Note: This assumes you are using Ubuntu and you installed the prerequisites.'
read -p 'Build dragonegg? [y/n] (default: n): ' -r
if [[ $REPLY =~ ^([yY][eE][sS]|[yY])+$ ]]
then
  echo 'building dragonegg'
  cd $DRAGONEGGDIR
  make clean
  if [ $isRelease == 'True' ]; then
    GCC=$GCCPATH LLVM_CONFIG=$LLVMDIR/release_build/bin/llvm-config make -B
  else
    GCC=$GCCPATH LLVM_CONFIG=$LLVMDIR/build/bin/llvm-config make -B
  fi
fi
