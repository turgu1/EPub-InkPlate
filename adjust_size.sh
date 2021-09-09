#!/usr/bin/bash
#
# Adjust the size of EPub embedded images to grayscale
# with maximum geometry of 600x800.
#
# Guy Turcotte, December 2020

USAGE="Usage: $0 big-in.epub small-out.epub"
: ${2:?$USAGE}

unzip -d "$1-tmp" "$1"

cd "$1-tmp"

find . -type f  \
  -iregex  '^.*[.]\(jpg\|jpeg\|png\)$' \
  -exec mogrify -quality 85 -grayscale average -resize '600x800>' {} \;  

zip -Duro "../$2" .

cd .. 

rm -rf "$1-tmp"   # Cleanup