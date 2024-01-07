@echo off

set CommonCompilerFlags=-MT -nologo -Gm- -GR- -EHa- -Oi -Od -WX -W4 -wd4201 -wd4100 -wd4189 -wd4211 -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -DHANDMADE_WIN32=1 -FC -Z7
set CommonLinkerFlags=-opt:ref user32.lib Gdi32.lib winmm.lib

if not exist ..\..\..\build mkdir ..\..\..\build
pushd ..\..\..\build
call c:\HandmadeHero\misc\shell.bat

REM 32-bit build
REM cl %CommonCompilerFlags%  ..\HandmadeHero\win32_handmade.cpp /link -subsystem:windows,5.1 %CommonLinkerFlags%

REM 64-bit build
cl %CommonCompilerFlags%  ..\HandmadeHero\handmade.cpp -Fm /LD /link /EXPORT:GameGetSoundSamples /EXPORT:GameUpdateAndRender
cl %CommonCompilerFlags%  ..\HandmadeHero\win32_handmade.cpp -Fmwin32_handmade.map /link %CommonLinkerFlags%
popd