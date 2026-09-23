@echo off
setlocal
cd /d "%~dp0"

set "GXX="
where g++ >nul 2>nul && set "GXX=g++"
if not defined GXX if exist "C:\mingw64\bin\g++.exe" set "GXX=C:\mingw64\bin\g++.exe"
if not defined GXX if exist "C:\WinLibs\mingw64\bin\g++.exe" set "GXX=C:\WinLibs\mingw64\bin\g++.exe"
if not defined GXX for /d %%D in ("%LocalAppData%\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT*") do (
  if exist "%%D\mingw64\bin\g++.exe" set "GXX=%%D\mingw64\bin\g++.exe"
)

if not defined GXX (
  echo g++ bulunamadi. WinLibs MinGW kurun.
  exit /b 1
)

"%GXX%" -std=c++17 -static -static-libgcc -static-libstdc++ -municode -mwindows -O2 -finput-charset=UTF-8 FiratKebapAdmin.cpp -o FiratKebapAdmin.exe -lwinhttp -lcrypt32 -lcomctl32 -lcomdlg32 -lole32 -lgdi32
if errorlevel 1 exit /b 1
copy /Y FiratKebapAdmin.exe "%USERPROFILE%\Desktop\FiratKebapAdmin.exe" >nul
echo Derlendi ve masaustune kopyalandi.
endlocal
