#!/bin/sh
#
# This script is used to generate a new release
#
# Guy Turcotte, March 2021
# Update for idf.py, October 2024
#

if [ ! command -v idf.py >/dev/null 2>&1 ]; then
  . ~/esp/esp-idf/export.sh
  if [ ! command -v idf.py >/dev/null 2>&1 ]; then
    echo "Unable to get esp-idf ready. Aborting." >&2
    return 1
  fi
fi

if [ "$3" = "" ]; then
  echo "Usage: $0 version_nbr type extended_case [build_only]"
  echo "type = 6, 10, 6plus, 6plusv2, 6flick"
  echo "extended_case = 0, 1"
  echo "   0 = no extended case"
  echo "   1 = with extended case"
  echo "build_only (optional) = 1, 2"
  echo "   1 = clean build folder"
  echo "   2 = keep build folder"
  return 1
fi

if [ "$3" = "0" ]; then
  folder="release-v$1-inkplate_$2"
  environment="inkplate_$2_release"
  case "$2" in
    "6") device="INKPLATE_6" ;;
    "10") device="INKPLATE_10" ;;
    "6plus") device="INKPLATE_6PLUS" ;;
    "6plusv2") device="INKPLATE_6PLUS_V2" ;;
    "6flick") device="INKPLATE_6FLICK" ;;
    *) echo "UNKNOWN DEVICE NAME. ABORTING!"
       return 1
       ;;
  esac
  echo "Device is ${device}"
else
  folder="release-v$1-inkplate_extended_case_$2"
  environment="inkplate_$2_extended_case_release"
fi

if [ "$4" = "" ]; then
  if [ -f "$folder.zip" ]; then
    echo "File $folder.zip already exist!"
    return 1
  fi
fi

if [ ! "$4" = "2" ]; then
  rm -rf build
fi

idf.py build -DDEVICE=$device -DAPP_VERSION=$1

if [ ! -f "build/EPub-InkPlate.bin" ]; then
  echo "idf.py run error!"
  return 1
fi

if [ ! "$4" = "" ]; then
  echo "Compilation only... bld_release completed."
  return 0
fi

rm bin/*.bin

mkdir "$folder"

cp build/bootloader/bootloader.bin bin
cp build/partition_table/partition-table.bin bin/partitions.bin
cp build/EPub-InkPlate.bin bin/firmware.bin
cp -r bin "$folder"

mkdir "$folder/SDCard"
mkdir "$folder/SDCard/fonts"
mkdir "$folder/SDCard/books"

cp SDCard/config_distrib.txt $folder/SDCard/config.txt
cp SDCard/fonts_list.xml $folder/SDCard/fonts_list.xml
cp SDCard/fonts_list_orig.xml $folder/SDCard/fonts_list_orig.xml
cp SDCard/fonts/* $folder/SDCard/fonts
cp SDCard/books/Austen*.epub $folder/SDCard/books
cp doc/timezones.csv $folder

if [ "$3" = "0" ]; then
  case "$2" in
    "6plus"|"6plusv2"|"6flick")  cp "doc/USER GUIDE TOUCH.pdf" "$folder/USER GUIDE.pdf" ;;
    *) cp "doc/USER GUIDE.pdf" "$folder" ;;
  esac
else
  cp "doc/USER GUIDE for Extended Case.pdf" "$folder/USER GUIDE.pdf"
fi
cp "doc/INSTALL.pdf" "$folder"

cp adjust_size.sh "$folder"

zip -r "$folder.zip" "$folder"

rm -rf "$folder"

echo "bld_release completed."
