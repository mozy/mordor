@IF DEFINED _ECHO ECHO ON
@SETLOCAL

@SET CONFIG=Release
@SET BUILD32=1

REM For the moment forcing cmake by default
@SET CMAKEARG=

REM Most arguments are processed in build32Or64.bat
FOR %%A IN (%*) DO (    
    @IF /i "%%A" EQU "debug" SET CONFIG=Debug
    @IF /i "%%A" EQU "coverage" SET CONFIG=Debug
    @IF /i "%%A" EQU "release" SET CONFIG=Release

    @IF /i "%%A" EQU "no32" SET BUILD32=0

    @IF /i "%%A" EQU "cmake" SET CMAKEARG=cmake
    @IF /i "%%A" EQU "nocmake" SET CMAKEARG=
)

@IF "%BUILD32%" EQU "0" (
    ECHO Skipping 32-bit build
    GOTO :build64
)

CALL buildtools\build32Or64.bat %CONFIG% Win32 %CMAKEARG%
IF ERRORLEVEL 1 EXIT /B

:build64

CALL buildtools\build32Or64.bat %CONFIG% x64 %CMAKEARG%
IF ERRORLEVEL 1 EXIT /B

:done64build

ECHO Copying 64-bit unit tests to package directory

@IF NOT EXIST packages (
  MKDIR packages
)

COPY /Y x64\%CONFIG%\tests.exe packages\tests.exe

REM Copy openssl, zlib dlls needed by tests
COPY /Y x64\%CONFIG%\*.dll packages
