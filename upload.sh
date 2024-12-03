#!/usr/bin/env bash

set -e

output="$(dirname $0)/image.bmp"

convert "$2" -resize 1200x825^ -gravity Center -extent 1200x825 -depth 8 -type palette "BMP2:${output}"
./client/target/debug/client "$1" "${output}"
