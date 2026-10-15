@echo off
setlocal

set "MSYS2_ROOT=C:\msys64"
set "GCC_DIR=%MSYS2_ROOT%\ucrt64\bin"
set "MARIADB_INCLUDE=%MSYS2_ROOT%\ucrt64\include"

if not exist "%GCC_DIR%\g++.exe" set "GCC_DIR=%MSYS2_ROOT%\mingw64\bin"
if not exist "%GCC_DIR%\g++.exe" (
  echo.
  echo ERROR: g++ not found. Install the MSYS2 compiler first:
  echo   C:\msys64\usr\bin\pacman.exe -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-libmariadbclient
  echo.
  pause
  exit /b 1
)

if not exist "%MARIADB_INCLUDE%\mariadb\mysql.h" set "MARIADB_INCLUDE=%MSYS2_ROOT%\mingw64\include"
if not exist "%MARIADB_INCLUDE%\mariadb\mysql.h" (
  echo.
  echo ERROR: mysql.h not found. Install the MariaDB dev package first:
  echo   C:\msys64\usr\bin\pacman.exe -S --needed mingw-w64-ucrt-x86_64-libmariadbclient
  echo.
  pause
  exit /b 1
)

set "PATH=%GCC_DIR%;%PATH%"

"%GCC_DIR%\g++.exe" main.cpp Cashier.cpp Database.cpp Product.cpp Transaction.cpp Gemini.cpp -o CashierApp.exe ^
  -I"%MARIADB_INCLUDE%" ^
  -I"%MARIADB_INCLUDE%\mariadb" ^
  -lmariadb -lwinhttp -lcomctl32 -mwindows -static-libgcc -static-libstdc++

if errorlevel 1 (
  echo.
  echo BUILD FAILED - read the first error message above.
  pause
  exit /b 1
)

if exist "%MSYS2_ROOT%\ucrt64\bin\libmariadb.dll" copy /Y "%MSYS2_ROOT%\ucrt64\bin\libmariadb.dll" "%~dp0libmariadb.dll" > nul
if exist "%MSYS2_ROOT%\mingw64\bin\libmariadb.dll" copy /Y "%MSYS2_ROOT%\mingw64\bin\libmariadb.dll" "%~dp0libmariadb.dll" > nul

echo.
echo BUILD OK - run CashierApp.exe
if not "%1"=="--no-pause" pause
