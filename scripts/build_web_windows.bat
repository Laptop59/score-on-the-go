@echo OFF

emcmake cmake -S .. -B ..\build\web

cd "..\build\web"
make