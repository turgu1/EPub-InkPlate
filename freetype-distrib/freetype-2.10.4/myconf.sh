#! /usr/bin/bash

./configure --host=xtensa-esp32-elf --prefix=$HOME/Dev/EPub-InkPlate/lib_freetype --disable-shared --enable-static --without-png --without-zlib --without-harfbuzz CFLAGS="-mlongcalls"
