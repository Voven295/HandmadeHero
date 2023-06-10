@echo off
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
set path=c:\4coder\4coder\HandmadeHero\misc;%path%