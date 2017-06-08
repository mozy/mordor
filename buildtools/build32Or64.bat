IF DEFINED _ECHO ECHO ON
SETLOCAL

REM Script to compile mordor Debug or Release for either 32 or 64 bit
REM This also supports performing a cmake generation for developers who want to do compilation
REM themselves from the generated project files.
REM This script can be invoked directly from command line or from Visual Studio via
REM mordor_cmake.vcxproj.
REM
REM usage examples:
REM    build32Or64.bat Debug x64 cmakegen
REM or
REM    build32Or64.bat Release Win32 rebuild

SET CONFIG=Release
SET USE_CMAKE=0
SET CMAKEGEN=0
SET CLEAN=0
SET CLEANONLY=0

SET PLATFORM=Win32
SET MSBUILD_TARGET=Build

REM Determine absolute path of mordor no matter how this script was invoked
SET ROOT=%~dp0%\..
pushd %ROOT%
SET ROOT=%CD%
popd

FOR %%A IN (%*) DO (
    IF /i "%%A" EQU "debug" SET CONFIG=Debug
    IF /i "%%A" EQU "coverage" SET CONFIG=Debug
    IF /i "%%A" EQU "release" SET CONFIG=Release

    IF /i "%%A" EQU "x64" SET PLATFORM=x64
    IF /i "%%A" EQU "win32" SET PLATFORM=Win32

    IF /i "%%A" EQU "clean" (
        SET CLEAN=1
        SET CLEANONLY=1
        SET MSBUILD_TARGET=Clean
    )
    IF /i "%%A" EQU "rebuild" (
        SET CLEAN=1
        SET CLEANONLY=0
        SET MSBUILD_TARGET=Rebuild
    )

    IF /i "%%A" EQU "nocmake" SET USE_CMAKE=0
    IF /i "%%A" EQU "cmake" SET USE_CMAKE=1
    IF /i "%%A" EQU "cmakegen" (
        SET CMAKEGEN=1
        SET USE_CMAKE=1
    )
)

REM VS120 is Visual Studio 2013
SET LaunchVCVars="%VS120COMNTOOLS%..\..\VC\vcvarsall.bat"

IF NOT EXIST %LaunchVCVars% (
  ECHO Visual Studio not found!
  EXIT /B 1
)

if "%winclientlib%" == "" (
    REM winclientlib variable is needed to find the third party libs
    REM see thirdPartyPaths-win64.props
    REM Normally it should be set as a global env variable if not set
    REM we default to assumption that it is located in the root of the profile.
    REM See winclientlib git project for details

    SET winclientlib=%USERPROFILE%\winclientlib
)

REM Cmake is installed to winclientlib
SET CMAKE_FULLPATH=%winclientlib%\tools\cmake-3.7.1-win64-x64\bin\cmake

ECHO %MSBUILD_TARGET%ing Mordor %PLATFORM% %CONFIG%

@CALL %LaunchVCVars% x86
IF DEFINED _ECHO ECHO ON

IF "%PLATFORM%" EQU "Win32" (
    SET VCVARARG=x86
    SET GENERATOR="Visual Studio 12 2013"
) ELSE (
    SET VCVARARG=AMD64
    SET GENERATOR="Visual Studio 12 2013 Win64"
)

IF "%CLEAN%" EQU "1" (
    REM For cmake case all the generated output is centralized outside of the
    REM source, include CMakeCache.txt, so it is easy to clean or prepare for a rebuild
    RD /S /Q %PLATFORM%
)

IF "%USE_CMAKE%" EQU "0" GOTO buildsolution

IF "%CLEANONLY%" EQU "1" (
    ECHO Clean complete
    EXIT /B 0
)

REM CMake generation phase
REM We need to generate separately for 32 versus 64 bit so generated files are put into subdirectory based on platform
REM Actual output will go into Debug or Release directory inside the platform directory

IF NOT EXIST %PLATFORM% mkdir %PLATFORM%
pushd %PLATFORM%

REM Generate vcxproj
REM IF EXIST CMakeCache.txt DEL CMakeCache.txt
%CMAKE_FULLPATH% -G %GENERATOR% %ROOT%
IF ERRORLEVEL 1 EXIT /B

popd

REM If only generating then skip actual build process
IF "%CMAKEGEN%" EQU "1" GOTO :donebuild

REM Cmake Build phase

%CMAKE_FULLPATH% --build %PLATFORM% --target ALL_BUILD --config %CONFIG%
IF ERRORLEVEL 1 EXIT /B
GOTO donebuild

:buildsolution
REM Old method using handmade sln file

ECHO Adding winclientlib tools to path
REM This is based on knowledge of where happens to be installed on the build machine
REM see winclientlib git project for details
SET PATH=%winclientlib%\tools;%PATH%
SET SOLUTION=Mordor.sln

MSBuild /maxcpucount -P:configuration=%CONFIG% -P:platform=%PLATFORM% %SOLUTION% /t:%MSBUILD_TARGET%
IF ERRORLEVEL 1 EXIT /B

:donebuild

ENDLOCAL
