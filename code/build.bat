@echo off

if not exist ..\build mkdir ..\build
pushd ..\build
cl /nologo /Od /W4 /DBP_DEBUG /Z7 /wd4201 /Zc:strictStrings- ..\code\main.c /link /incremental:no /out:bytepath.exe user32.lib D3D11.lib  dxguid.lib D3DCompiler.lib winmm.lib Gdi32.lib
popd