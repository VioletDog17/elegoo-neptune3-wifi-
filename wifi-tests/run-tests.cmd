@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cl /nologo /EHsc /W4 /WX /std:c++14 protocol_test.cpp /Fe:protocol_test.exe
if errorlevel 1 exit /b 1
protocol_test.exe
