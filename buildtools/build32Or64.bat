IF DEFINED _ECHO ECHO ON
SETLOCAL

REM Script to compile Debug or Release for either 32 or 64 bit
REM This also supports performing a cmake generation for developers who want to do compilation
REM themselves from the generated project files.
REM This script can be invoked directly from command line or from Visual Studio via
REM PROJECTNAME_cmake.vcxproj.
REM
REM usage examples:
REM    build32Or64.bat Debug x64 cmakegen
REM or
REM    build32Or64.bat Release Win32 rebuild

REM By default it build mordor, but higher level clients can use it if they specify
REM /project and /root arguments.

SET CONFIG=Release
SET USE_CMAKE=0
SET CMAKEGEN=0
SET CLEAN=0
SET CLEANONLY=0

SET PLATFORM=Win32
SET MSBUILD_TARGET=Build

SET PROJECT=Mordor

REM Determine absolute path of root of the source tree no matter how this script was invoked
SET ROOT=%~dp0%\..
pushd %ROOT%
SET ROOT=%CD%
popd

:ARGSLOOP

IF "%1" EQU "" GOTO DONEARGS

IF /i "%1" EQU "/project" (
  SET PROJECT=%2
  SHIFT /1
  SHIFT /1
  GOTO ARGSLOOP
)

IF /i "%1" EQU "/root" (
  SET ROOT=%2
  SHIFT /1
  SHIFT /1
  GOTO ARGSLOOP
)

IF /i "%1" EQU "debug" SET CONFIG=Debug
IF /i "%1" EQU "coverage" SET CONFIG=Debug
IF /i "%1" EQU "release" SET CONFIG=Release

IF /i "%1" EQU "x64" SET PLATFORM=x64
IF /i "%1" EQU "win32" SET PLATFORM=Win32

IF /i "%1" EQU "clean" (
	SET CLEAN=1
	SET CLEANONLY=1
	SET MSBUILD_TARGET=Clean
)

IF /i "%1" EQU "rebuild" (
	SET CLEAN=1
	SET CLEANONLY=0
	SET MSBUILD_TARGET=Rebuild
)

IF /i "%1" EQU "nocmake" SET USE_CMAKE=0
IF /i "%1" EQU "cmake" SET USE_CMAKE=1

IF /i "%1" EQU "cmakegen" (
	SET CMAKEGEN=1
	SET USE_CMAKE=1
)

SHIFT /1
GOTO ARGSLOOP

:DONEARGS


if "%winclientlib%" == "" (
    REM winclientlib variable is needed to find the third party libs
    REM and tools
    REM Normally it should be set as a global env variable with the path
    REM of the winclientlib git repro on the dev machine.
    REM The convention for windows slaves is in the root
    REM profile directory so that is the default.

    SET winclientlib=%USERPROFILE%\winclientlib
)

REM Cmake is installed to winclientlib
SET CMAKE_FULLPATH=%winclientlib%\tools\cmake-3.7.1-win64-x64\bin\cmake

ECHO %MSBUILD_TARGET%ing %PROJECT% %PLATFORM% %CONFIG%

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

IF "%PLATFORM%" EQU "Win32" (
    SET GENERATOR="Visual Studio 12 2013"
) ELSE (
    SET GENERATOR="Visual Studio 12 2013 Win64"
    SET PreferredToolArchitecture=x64
)

REM Generate vcxproj
REM IF EXIST CMakeCache.txt DEL CMakeCache.txt
%CMAKE_FULLPATH% %CMAKE_OPTIONS% -G %GENERATOR% %ROOT%
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

REM VS120 is Visual Studio 2013
SET LaunchVCVars="%VS120COMNTOOLS%..\..\VC\vcvarsall.bat"

IF NOT EXIST %LaunchVCVars% (
  ECHO Visual Studio not found!
  EXIT /B 1
)

IF "%PLATFORM%" EQU "Win32" (
    SET VCVARARG=x86
) ELSE (
    SET VCVARARG=AMD64
)

@CALL %LaunchVCVars% %VCVARARG%
IF DEFINED _ECHO ECHO ON

ECHO Adding winclientlib tools to path
SET PATH=%winclientlib%\tools;%PATH%
SET SOLUTION=%PROJECT%.sln

MSBuild /maxcpucount -P:configuration=%CONFIG% -P:platform=%PLATFORM% %SOLUTION% /t:%MSBUILD_TARGET%
IF ERRORLEVEL 1 EXIT /B

:donebuild

ENDLOCAL
