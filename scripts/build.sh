#!/bin/bash

set -e

if [[ "$#" -lt 2 ]]; then
  echo "Usage: $0 <server/client/combined> <platform> [build arguments]"
  exit 1
fi

# class is either server, client or combined
CLASS="$1"

# this is the build platform (Linux, Debug, STM32 etc.)
PLATFORM="$2"

# build flags is everything else, this is passed directly
BUILD_FLAGS="${@:3}"
echo "$BUILD_FLAGS"

# create the build output directory
BUILD_PATH=build/$CLASS-$PLATFORM
mkdir -p $BUILD_PATH

# generate the meson file
./tools/meson/mgen.sh > $PWD/meson.build

# run meson and then build with ninja
meson setup --prefix "/opt/vzlu" --libdir lib --pkg-config-path "/opt/vzlu/lib/pkgconfig" -Dbuild_platform=$PLATFORM -Dbuild_class=$CLASS $BUILD_FLAGS $BUILD_PATH
cd $BUILD_PATH && ninja
