#!/bin/bash
cd "${0%/*}"/

./build_generic.sh

cd "../build/`uname`/Release"
./score-on-the-go