#!/bin/bash

# update the path to the extracted "Generic" directory
BASEDIR="/home/austin/Documents/LLVMDRAGONEGG/Generic"
CPU2006DIR="$BASEDIR/CPU2006"

cd $CPU2006DIR
sudo chmod u+x install.sh
./install.sh
cd $CPU2006DIR
sudo chmod u+x shrc
./shrc
