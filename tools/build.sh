#!/bin/bash

# set up the installation prefix
prefix="/opt/vzlu"
if [ $# -lt 1 ] ; then
  echo "Library prefix not specified, assuming default"
else
  prefix=$1
fi
echo "Using VZLU libraries in $prefix"

# generate the meson file
./$(dirname "$0")/meson/mgen.sh > $PWD/meson.build

# run meson to configure the build
meson setup --prefix "$prefix" \
 --libdir lib \
 --pkg-config-path "$prefix/lib/pkgconfig" \
 -Dbuild_autotest=false \
 -Denable_lpldgen=true \
 -Denable_csp=true \
 -Dbuild_cmdline=plugin build

# finally, build it using ninja 
cd build && ninja
