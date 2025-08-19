#!/bin/bash
export PKG_CONFIG_PATH=/opt/sdl3/lib/pkgconfig:$PKG_CONFIG_PATH

rm -rf CMakeCache.txt CMakeFiles
cmake . \
  -DCMAKE_PREFIX_PATH=/usr/local \
  -DCMAKE_INSTALL_RPATH=/usr/local/lib \
  -DCMAKE_BUILD_RPATH=/usr/local/lib
