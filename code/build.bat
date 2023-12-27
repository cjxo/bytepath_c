@echo off

set CompilerOpts=/nologo /Od /W4 /DBP_DEBUG /Z7 /wd4201 /Zc:strictStrings-
set Libs=user32.lib D3D11.lib  dxguid.lib D3DCompiler.lib winmm.lib Gdi32.lib

if not exist ..\build mkdir ..\build
pushd ..\build
cl %CompilerOpts% ..\code\main.c /link /incremental:no /out:bytepath.exe %Libs%
popd