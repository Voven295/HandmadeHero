@echo off

if not exist ..\..\..\build mkdir ..\..\..\build
pushd ..\..\..\build
call c:\HandmadeHero\misc\shell.bat
cl -nologo -GR- -EHa- -Oi -WX -W4 -wd4201 -wd4100 -wd4189 -wd4211 -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -DHANDMADE_WIN32=1 -FC -Z7 ..\HandmadeHero\win32_handmade.cpp user32.lib Gdi32.lib
popd