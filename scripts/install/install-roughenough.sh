#!/bin/bash

git submodule update --init --recursive

cd deps/roughenough

sudo apt install cargo

cargo build --release