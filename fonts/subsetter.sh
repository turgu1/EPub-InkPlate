#!/bin/bash
#
# Subsetting tool for fonts part of the EPub-InkPlate app.
#
# This script is used to minify fonts loaded by the application
# to optimize the use of the available memory. Latin-1 is the subset
# targetted by this script. It can be updated to extract other subsets.
# All input fonts are in TTF or OTF formats. The output format is OTF and
# greatly minimized as no special features are kept.
#
# Guy Turcotte, November 2020

LATIN1="U+0000-00FF,U+0131,U+0152-0153,U+02BB-02BC,U+02C6,U+02DA,U+02DC,U+2000-206F,U+2074,U+20AC,U+2122,U+2191,U+2193,U+2212,U+2215,U+FEFF,U+FFFD"

FOLDER_IN=orig
FOLDER_OUT=subset-latin1

# FONTS=(Asap AsapCondensed Caladea CrimsonPro DejaVuSans DejaVuSansCondensed DejaVuSerif DejaVuSerifCondensed RedHatDisplay)
# SUBS=(Regular Bold BoldItalic Italic)

# mkdir $FOLDER_OUT/woff
# mkdir $FOLDER_OUT/otf

# for name in ${FONTS[@]}; do
#   for sub in ${SUBS[@]}; do
#     font_name="$name-$sub"
#     if [ -f $FOLDER_IN/$font_name.ttf ]; then
#       echo $font_name
#       ext="ttf"
#     else
#       if [ -f $FOLDER_IN/$font_name.otf ]; then
#         echo $font_name
#         ext="otf"
#       else
#         echo "Hum... nothing found for $name-$sub"
#         ext="none"
#       fi
#     fi
#     if [ -f $FOLDER_IN/$font_name.$ext ]; then
#       pyftsubset "$FOLDER_IN/$font_name.$ext" --output-file="$FOLDER_OUT/woff/$font_name.woff" --flavor=woff --layout-features='' --unicodes="$LATIN1"
#       ./woff2otf.py "$FOLDER_OUT/woff/$font_name.woff" "$FOLDER_OUT/otf/$font_name.otf"
#     fi
#   done
# done

pyftsubset "$FOLDER_IN/drawings.ttf" --output-file="$FOLDER_OUT/woff/drawings.woff" --flavor=woff --layout-features='' --unicodes='*'
./woff2otf.py "$FOLDER_OUT/woff/drawings.woff" "$FOLDER_OUT/otf/drawings.otf"

rm "$FOLDER_OUT/woff/*"
