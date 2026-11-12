@echo off
cd /d "%~dp0"
if exist "..\.tools\python\ziglang\zig.exe" goto zig
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cl /nologo /EHsc /W4 /WX /utf-8 /std:c++14 protocol_test.cpp /Fe:protocol_test.exe
if errorlevel 1 exit /b 1
protocol_test.exe
if errorlevel 1 exit /b 1
cl /nologo /EHsc /W4 /WX /utf-8 /std:c++14 receiver_test.cpp /Fe:receiver_test.exe
if errorlevel 1 exit /b 1
receiver_test.exe
if errorlevel 1 exit /b 1
cl /nologo /EHsc /W4 /WX /utf-8 /std:c++14 upload_test.cpp /Fe:upload_test.exe
if errorlevel 1 exit /b 1
upload_test.exe
if errorlevel 1 exit /b 1
cl /nologo /EHsc /W4 /WX /utf-8 /std:c++14 compatibility_test.cpp /Fe:compatibility_test.exe
if errorlevel 1 exit /b 1
compatibility_test.exe
if errorlevel 1 exit /b 1
cl /nologo /EHsc /W4 /WX /utf-8 /std:c++14 preview_test.cpp /Fe:preview_test.exe
if errorlevel 1 exit /b 1
preview_test.exe
if errorlevel 1 exit /b 1
cl /nologo /EHsc /W4 /WX /utf-8 /std:c++14 filename_metadata_test.cpp /Fe:filename_metadata_test.exe
if errorlevel 1 exit /b 1
filename_metadata_test.exe
exit /b %errorlevel%

:zig
set "ZIG_GLOBAL_CACHE_DIR=%~dp0..\.tools\zig-cache"
set "ZIG_LOCAL_CACHE_DIR=%~dp0..\.tools\zig-local"
for %%T in (protocol receiver upload compatibility preview filename_metadata) do (
  "..\.tools\python\ziglang\zig.exe" c++ -std=c++14 -Wall -Wextra -Werror %%T_test.cpp -o %%T_test.exe
  if errorlevel 1 exit /b 1
  %%T_test.exe
  if errorlevel 1 exit /b 1
)
