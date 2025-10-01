#!/bin/bash
cd "${0%/*}"

emcmake cmake -S .. -B ../build/web -DCMAKE_C_FLAGS="-sUSE_MPG123=1" -DCMAKE_CXX_FLAGS="-sUSE_MPG123=1"

cd "../build/web" || return
emmake make 

mkdir -p Release
mv -f score-on-the-go.html Release/score-on-the-go.html
mv -f score-on-the-go.js Release/score-on-the-go.js
mv -f score-on-the-go.wasm Release/score-on-the-go.wasm
mv -f score-on-the-go.data Release/score-on-the-go.data