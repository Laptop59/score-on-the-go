@echo OFF

cmake -S .. -B ..\build\win

cd "..\build\win"
make