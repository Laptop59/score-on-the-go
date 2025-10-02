@echo OFF
cd /d "%~dp0"

cmake -S .. -B ..\build\win

cd "..\build\win"
emmake make