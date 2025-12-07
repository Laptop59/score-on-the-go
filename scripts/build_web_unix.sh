#!/bin/bash
cd "${0%/*}"

# We need the mpg123 port.
emcmake cmake -S .. -B ../build/web -DCMAKE_C_FLAGS="-sUSE_MPG123=1" -DCMAKE_CXX_FLAGS="-sUSE_MPG123=1"

cd "../build/web" || return
emmake make 

cd Release
mv -f score-on-the-go.html index.html