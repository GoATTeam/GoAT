#!/bin/bash

sudo apt update
sudo apt install cmake

git submodule update --init --recursive

cd deps/relic
mkdir build
cd build
../preset/goat-x64-pbc-bls381.sh  ../
make -j
sudo make install
cd ../../..

