#!/bin/sh
#
# This script is used to generate a new release
#
# Guy Turcotte, March 2021
#

if [ "$3" = "" ]
then
  echo "Usage: $0 version type extended_case"
  echo "type = 6, 10, 6plus"
  echo "extended_case = 0, 1"
  return 1
fi

if [ "$3" = "0" ]
then
  folder="release-$1-inkplate_$2"
  release_folder="inkplate_$2_release"
  environment="inkplate_$2_release"
else
  folder="release-$1-inkplate_extended_case_$2"
  release_folder="inkplate_$2_extended_case_release"
  environment="inkplate_$2_extended_case_release"
fi

if [ -f "$folder.zip" ]
then
  echo "File $folder.zip already exist!"
  return 1
fi

pio run -e $environment

if [ $? -ne 0 ]
then
  echo "pio run error!"
  return 1
fi

mkdir "$folder"

cp .pio/build/$release_folder/bootloader.bin bin
cp .pio/build/$release_folder/partitions.bin bin
cp .pio/build/$release_folder/firmware.bin bin
cp -r bin "$folder"

cp -r SDCard "$folder"
rm -rf $folder/SDCard/books
mkdir $folder/SDCard/books
cp SDCard/books/Austen* $folder/SDCard/books

if [ -f "$folder/SDCard/books_dir.db" ]
then
  rm "$folder/SDCard/books_dir.db"
fi

if [ -f "$folder/SDCard/last_book.txt" ]
then
  rm "$folder/SDCard/last_book.txt"
fi

rm $folder/SDCard/config.txt

if [ "$3" = "0" ]
then
  cp "doc/USER GUIDE.pdf" "$folder"
else
  cp "doc/USER GUIDE for Extended Case.pdf" "$folder"
fi
cp "doc/INSTALL.pdf" "$folder"

cp adjust_size.sh "$folder"

zip -r "$folder.zip" "$folder"

rm -rf "$folder"

echo "Completed."
