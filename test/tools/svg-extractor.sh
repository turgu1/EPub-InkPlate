#!/bin/bash
# Enable recursive globbing
shopt -s globstar

mkdir -p "$1"

i=1

# Loop through all .epub files
for file in **/*.epub; do
  echo "Accessing: $file"
  # Add your logic here
  unzip -j "$file" *.svg -d "$1" && cd "$1"
  for f in *.svg; do 
    mv "$f" "$(printf "image_%03d.svg" $i)"
     i=$((i+1)) done

done
