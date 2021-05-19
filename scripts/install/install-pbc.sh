#!/bin/bash

sudo apt install gcc make libsodium-dev libgmp-dev build-essential flex bison

if [ ! -f pbc-0.5.14.tar.gz ];
then
    wget https://crypto.stanford.edu/pbc/files/pbc-0.5.14.tar.gz 
    tar -xf pbc-0.5.14.tar.gz 
fi

cd pbc-0.5.14 

./configure && make

sudo make install 

if [ ! $? -eq 0 ];
then
    echo "Unable to configure PBC library" > /dev/stderr
    exit 3
fi

sudo ldconfig

cd -