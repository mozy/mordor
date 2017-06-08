@ECHO OFF

REM Script to clean up all the 3rd party dlls required by the client
REM in the directory where the client binaries are compiled.
REM cleanlibs.bat $(SolutionDir)$(Platform)\$(Configuration)

REM check if destination exists
IF NOT EXIST "%1" GOTO :EOF

FOR %%X IN (libeay32.dll libeay32.pdb ssleay32.dll ssleay32.pdb zlib1.dll horizon-api.dll) DO (
   IF EXIST "%1\%%X" (
       ECHO Removing %1\%%X
       DEL /F /Q "%1\%%X"
   )
)
