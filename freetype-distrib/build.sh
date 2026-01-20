#! /usr/bin/bash
#
# Freetype build tool. 
#
# This script will configure and compile the Freetype library. 
# This must be done once when a new version of the library
# is retrieved or any change in the configuration of this library is performed.
#
# Before running this script, please do have esp-idf definitions exported to the shell:
#
# like so:
#
# $ . ~/esp/esp-idf/export.sh
#

FREETYPE_DIR=freetype-2.13.3
TARGET_LOC=../components/freetype

rm -rf $TARGET_LOC
mkdir $TARGET_LOC
cp CMakeLists.txt $TARGET_LOC

cd $FREETYPE_DIR


if [ ! -f builds/unix/configure ]; then
    ./autogen.sh
fi

cp ../modules.cfg .

# Save the original ftoption.h file. See at the end of the build below for its
# recovery
# if [ ! -f ../$FREETYPE_DIR/include/freetype/config/ftoption.h.orig ]; then
#     mv ../$FREETYPE_DIR/include/freetype/config/ftoption.h ../freetype/include/freetype/config/ftoption.h.orig
# fi

cp ../ftoption.h ../$FREETYPE_DIR/include/freetype/config/ftoption.h

./configure --host=xtensa-esp32-elf --prefix=$PWD/../$TARGET_LOC --disable-shared --enable-static \
  --with-png=no \
  --with-zlib=no \
  --with-bzip2=no \
  --with-brotli=no \
  --with-harfbuzz=no \
  --with-librsvg=no \
  --with-old-mac-fonts=no \
  --with-fsspec=no \
  --with-fsref=no \
  --with-quickdraw-toolbox=no \
  --with-quickdraw-carbon=no \
  --with-ats=no \
  CFLAGS="-mlongcalls -O2"

make
make install
make clean

# This is to remove the copied ftoption.h from the FreeType component such as
# git won't see it as a change to an imported component.

# if [ -f ../$FREETYPE_DIR/include/freetype/config/ftoption.h.orig ]; then
#     mv ../$FREETYPE_DIR/include/freetype/config/ftoption.h.orig ../freetype/include/freetype/config/ftoption.h 
# fi

cd ..

