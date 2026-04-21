#!/bin/bash
# Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
#
# build.sh - Development build for FRANK OS
#
# Builds firmware WITHOUT USB HID (USB CDC stdio for debug output).
# For release builds with USB HID enabled, use release.sh instead.
#
# Usage: ./build.sh [DVI|VGA]
#   Default: DVI (HDMI output)
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

DISPLAY_MODE="${1:-DVI}"

echo "FRANK OS Development Build"
echo "Display: $DISPLAY_MODE  |  USB HID: OFF (CDC stdio for debug)"
echo ""

rm -rf build
mkdir build
cd build

cmake .. -DFRANK_DISPLAY="$DISPLAY_MODE"
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
