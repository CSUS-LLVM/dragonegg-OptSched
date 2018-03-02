#!/bin/bash
BASEDIR=$(pwd)"/Generic"
LLVM_TARGET="SystemZ"

LLVMDIR="$BASEDIR/llvmTip"

if [ -z ${LLVM_TOOLCHAIN} ]; then
    echo "Please set LLVM_TOOLCHAIN env variable to the root of where you have cloned your SystemZ_ToolChain."
    echo "if you have not clone SystemZ_ToolChain, then please use the following command to clone the project."
    echo "git clone git@gitlab.com:CSUS_LLVM/SystemZ_ToolChains.git"
    echo "export LLVM_TOOLCHAIN=/path/to/SystemZ_ToolChains"
    exit 1
fi

export PATH=${LLVM_TOOLCHAIN}/bin:$PATH

echo "Building llvm on System Z"

if [ ! -d "$LLVMDIR/release_build" ]; then
    mkdir $LLVMDIR/release_build
fi

cd $LLVMDIR/release_build 
cmake -DCMAKE_INSTALL_PREFIX=${LLVM_TOOLCHAIN} -DLLVM_TARGETS_TO_BUILD=$LLVM_TARGET -DCMAKE_BUILD_TYPE="Release" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON $LLVMDIR/llvm-master -DCMAKE_CXX_LINK_FLAGS="-Wl,-rpath,${LLVM_TOOLCHAIN}/lib64 -L${LLVM_TOOLCHAIN}/lib64"
cp compile_commands.json $BASEDIR

make 
sudo cmake --build . --target install

cd $BASEDIR