@echo off

if not exist ..\..\..\build mkdir ..\..\..\build
pushd ..\..\..\build
call c:\HandmadeHero\misc\shell.bat
cl -FC -Zi ..\HandmadeHero\win32_handmade.cpp user32.lib Gdi32.lib
popd