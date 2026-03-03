#!/bin/bash
# Build FRANK OS
#
# Usage:
#   ./build.sh              — build for FRANK M2 board (default)
#   ./build.sh fruit_jam    — build for Adafruit Fruit Jam board

BOARD=${1:-m2}

rm -rf ./build
mkdir build
cd build
cmake .. -DFRANK_BOARD=${BOARD}
make -j4
