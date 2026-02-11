#!/bin/bash

set -e

cd "$(dirname "$0")"

FONT_SIZES=(8 10 12)

for size in ${FONT_SIZES[@]}; do
  font_name="cmu_${size}"
  font_path="../builtinFonts/source/CMUSerif.ttf"
  output_path="../builtinFonts/${font_name}.h"
  python fontconvert.py $font_name $size $font_path --2bit > $output_path
  echo "Generated $output_path"
done