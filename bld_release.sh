#!/bin/sh
#
# This script is used to generate a new release
#
# Guy Turcotte, November 2020
#

if [ "$1" = "" ]
then
  echo "Usage: $0 version"
  return 1
fi

folder="release-$1"

if [ -f "$folder.zip" ]
then
  echo "File $folder.zip already exist!"
  return 1
fi

mkdir "$folder"

cp .pio/build/inkplate6/bootloader.bin bin
cp .pio/build/inkplate6/partitions.bin bin
cp .pio/build/inkplate6/firmware.bin bin
cp -r bin "$folder"

cp -r SDCard "$folder"

if [ -f "$folder/SDCard/books_dir.db" ]
then
  rm "$folder/SDCard/books_dir.db"
fi

if [ -f "$folder/SDCard/last_book.txt" ]
then
  rm "$folder/SDCard/last_book.txt"
fi


cd doc
./gener.sh
cd ..

cp "doc/USER GUIDE.pdf" "$folder"
cp "doc/INSTALL.pdf" "$folder"

zip -r "$folder.zip" "$folder"

rm -rf "$folder"

echo "Completed."
