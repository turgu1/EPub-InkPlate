#!/bin/sh
#
# This script is used to generate a all releases
#
# Guy Turcotte, March 2021
#

if [ "$1" = "" ]
then
  echo "Usage: $0 version"
  return 1
fi

cd doc
./gener.sh
cd ..

./bld_release.sh $1 6 0
./bld_release.sh $1 6 1
./bld_release.sh $1 10 0
./bld_release.sh $1 10 1
