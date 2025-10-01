#!/bin/bash
cd "${0%/*}"/

cmake -B "../build/`uname`" -S ..

cd "../build/`uname`" || return
emmake make