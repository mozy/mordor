@SETLOCAL
@SET CONFIG=Release
@IF "%1" EQU "debug" SET CONFIG=Debug
@IF "%1" EQU "coverage" SET CONFIG=Debug
@IF NOT DEFINED PLATFORM (
  IF "%arch%" EQU "i386" (
    SET PLATFORM=Win32
  ) ELSE IF "%arch%" EQU "amd64" (
    SET PLATFORM=x64
  ) ELSE (
    SET PLATFORM=Win32
  )
)

MSBuild /maxcpucount -P:configuration=%CONFIG% -P:platform=%PLATFORM% mordor2010.sln
@IF ERRORLEVEL 1 EXIT /B

MKDIR packages
COPY /y %PLATFORM%\%CONFIG%\tests.exe packages\tests.exe
