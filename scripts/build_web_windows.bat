@echo OFF
cd /d "%~dp0"

emcmake cmake -S .. -B ../build/web -DCMAKE_C_FLAGS="-sUSE_MPG123=1" -DCMAKE_CXX_FLAGS="-sUSE_MPG123=1"

cd "..\build\web"
emmake make

mkdir Release

REM files are moved here.

copy /Y score-on-the-go.html Release\index.html
copy /Y score-on-the-go.js Release\score-on-the-go.js
copy /Y score-on-the-go.wasm Release\score-on-the-go.wasm
copy /Y score-on-the-go.data Release\score-on-the-go.data

del score-on-the-go.html
del score-on-the-go.js
del score-on-the-go.wasm
del score-on-the-go.data