@ECHO OFF

REM Script to copy the 3rd party dlls required by our executables
REM into the directory where the binaries are compiled.
REM That makes it easier to launch the executables
REM with no dependency on dlls being found in the PATH.
REM For example this script can be triggered in Visual Studio as
REM getlibs.bat $(SolutionDir)$(Platform)\$(Configuration)

REM make sure destination exists
IF NOT EXIST %1 MD %1

FOR %%X IN (libeay32.dll ssleay32.dll zlib1.dll) DO (
   CALL :findlib %%X %1
   IF ERRORLEVEL 1 EXIT /B %ERRORLEVEL%
)

EXIT /B %ERRORLEVEL%


REM ------------------------

:findlib

SET WHICH=%~$LIB:1
IF "%WHICH%" == "" (
  ECHO Could not find %1
  EXIT /B 1
) ELSE (
  ECHO Copying %WHICH% to %2
  COPY /Y "%WHICH%" %2 >NUL
  EXIT /B 0
)


