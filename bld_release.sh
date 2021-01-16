#!/bin/sh
#
# This script is used to generate a new release
#
# Guy Turcotte, January 2021
#

if [ "$1" = "" ]
then
  echo "Usage: $0 version type"
  echo "type = 6, 10, 6plus"
  return 1
fi

folder="release-$1-inkplate_$2"

if [ -f "$folder.zip" ]
then
  echo "File $folder.zip already exist!"
  return 1
fi

mkdir "$folder"

cp .pio/build/inkplate_$2_release/bootloader.bin bin
cp .pio/build/inkplate_$2_release/partitions.bin bin
cp .pio/build/inkplate_$2_release/firmware.bin bin
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

if [ -d "$folder/SDCard/books/temp" ]
then
  rm -rf "$folder/SDCard/books/temp"
fi

rm $folder/SDCard/books/*.locs
rm $folder/SDCard/books/*.pars
rm $folder/SDCard/config.txt

cd doc
./gener.sh
cd ..

cp "doc/USER GUIDE.pdf" "$folder"
cp "doc/INSTALL.pdf" "$folder"

cp adjust_size.sh "$folder"

zip -r "$folder.zip" "$folder"

rm -rf "$folder"

echo "Completed."
