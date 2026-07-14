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

# LATIN1="U+0000-00FF,U+0131,U+0152-0153,U+0160-0161,U+0178,U+02BB-02BC,U+02C6,U+02DA,U+02DC,U+2000-206F,U+2074,U+20AC,U+2122,U+2190-2193,U+2212,U+2215,U+FB00-FB4F,U+FEFF,U+FFFD"
# LATIN1="U+0000-017F,U+02BB-02BC,U+02C6,U+02DA,U+02DC,U+1E9E,U+2000-206F,U+2074,U+20AC,U+2122,U+2190-2193,U+2212,U+2215,U+FB00-FB4F,U+FEFF,U+FFFD"
LATIN1="U+0000-00FF,U+0104-0107,U+010C-0111,U+0118-011B,U+011E-011F,U+0130-0133,U+0141-0144,U+0147-0148,U+014D,U+0150-0153,U+0158-015B,U+015E-0161,U+0164-0165,U+016E-0171,U+0178-017E,U+1E9E,U+0201,U+020D,U+02BB-02BC,U+02C6,U+02DA,U+02DC,U+2000-206F,U+2074,U+20AC,U+2122,U+2190-2193,U+2212,U+2215,U+FB00-FB4F,U+FEFF,U+FFFD"

FOLDER_IN=orig
FOLDER_OUT=subset-latin1

FONTS=(Asap AsapCondensed Bitter Caladea Cuprum DejaVuSans DejaVuSansCondensed DejaVuSerif DejaVuSerifCondensed RedHatDisplay Roboto RobotoCondensed OpenSans OpenSans_Condensed IbarraRealNova NewCMMono10 NewCMSans10 NewCM10 CrimsonPro CMUConcrete CMUSerif CMUSansSerif CMUTypewriter)
# FONTS=(CMUConcrete CMUSerif CMUSansSerif CMUTypewriter)
# FONTS=(Cuprum)
# FONTS=(FiraSans)
SUBS=(Regular Roman Bold BoldItalic Italic Medium MediumItalic Oblique BoldOblique)

mkdir -p $FOLDER_OUT/woff
mkdir -p $FOLDER_OUT/otf

for name in ${FONTS[@]}; do
  echo $name ...
  for sub in ${SUBS[@]}; do
    font_name="$name-$sub"
    if [ -f $FOLDER_IN/$font_name.ttf ]; then
      echo $font_name
      ext="ttf"
    else
      if [ -f $FOLDER_IN/$font_name.otf ]; then
        echo $font_name
        ext="otf"
      else
        echo "Hum... nothing found for $name-$sub"
        ext="none"
      fi
    fi
    if [ -f $FOLDER_IN/$font_name.$ext ]; then
      pyftsubset "$FOLDER_IN/$font_name.$ext" --output-file="$FOLDER_OUT/woff/$font_name.woff" --flavor=woff --layout-features='kern','gpos' --unicodes="$LATIN1"
      ./woff2otf.py "$FOLDER_OUT/woff/$font_name.woff" "$FOLDER_OUT/otf/$font_name.otf"
    fi
  done
done

pyftsubset "$FOLDER_IN/drawings.ttf" --output-file="$FOLDER_OUT/woff/drawings.woff" --flavor=woff --layout-features='' --unicodes='*'
./woff2otf.py "$FOLDER_OUT/woff/drawings.woff" "$FOLDER_OUT/otf/drawings.otf"

# rm "$FOLDER_OUT/woff/*"
