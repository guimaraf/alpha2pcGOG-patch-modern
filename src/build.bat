@echo off
rem Script de compilacao para o dinput.dll (XInput Mod para Street Fighter Alpha 2)
rem Requer compilador C para x86 / i686 (32 bits), como MinGW-w64 ou Clang

echo Compilando dinput.dll (32-bit)...
clang -target i686-w64-windows-gnu -shared -O2 -s dinput_xinput.c dinput.def -o dinput.dll -lwinmm -lm
if %errorlevel% equ 0 (
    echo Compilado com sucesso: dinput.dll
) else (
    echo Falha na compilacao.
)
pause
