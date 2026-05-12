#! /usr/bin/bash

for f in *.svg; do
  echo "Processing $f... to ${f%.svg}.bmp"

#   size=$(identify -format "%wx%h" "${f}")

#   echo $size

  magick "$f" -depth 8 -compress none "${f%.svg}.bmp"

done

