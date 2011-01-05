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

@IF "%PROCESSOR_ARCHITECTURE%" EQU "AMD64" (
  SET X64PLATFORM=amd64
  SET REG_BINARY=%WINDIR%\SysWOW64\REG
  SET REG_FLAGS=/reg:64
) ELSE (
  SET X64PLATFORM=x86_amd64
  SET REG_BINARY=REG
)

@IF "%PLATFORM%" EQU "Win32" (
  SET VCVARSPLATFORM=x86
) ELSE IF "%PLATFORM%" EQU "x64" (
  SET VCVARSPLATFORM=%X64PLATFORM%
)

@SET LaunchVCVars=
@IF NOT DEFINED CXX (
  FOR /F "usebackq tokens=2*" %%A IN (`%REG_BINARY% QUERY "HKLM\SOFTWARE\Microsoft\VisualStudio\10.0\Setup\VC" /v "ProductDir" 2^> NUL`) DO @CALL SET LaunchVCVars=%%~dpBvcvarsall.bat
  SET CXX=vc10
  IF NOT DEFINED LaunchVCVars (
    FOR /F "usebackq tokens=2*" %%A IN (`%REG_BINARY% QUERY "HKLM\SOFTWARE\Microsoft\VisualStudio\9.0\Setup\VC" /v "ProductDir" 2^> NUL`) DO @CALL SET LaunchVCVars=%%~dpBvcvarsall.bat
    SET CXX=vc9
  )
) ELSE IF "%CXX%" EQU "vc10" (
  FOR /F "usebackq tokens=2*" %%A IN (`%REG_BINARY% QUERY "HKLM\SOFTWARE\Microsoft\VisualStudio\10.0\Setup\VC" /v "ProductDir" 2^> NUL`) DO @CALL SET LaunchVCVars=%%~dpBvcvarsall.bat
) ELSE IF "%CXX%" EQU "vc9" (
  FOR /F "usebackq tokens=2*" %%A IN (`%REG_BINARY% QUERY "HKLM\SOFTWARE\Microsoft\VisualStudio\9.0\Setup\VC" /v "ProductDir" 2^> NUL`) DO @CALL SET LaunchVCVars=%%~dpBvcvarsall.bat
)

@IF NOT DEFINED LaunchVCVars (
  @ECHO Visual Studio not found!
  EXIT /B 1
)

@CALL "%LaunchVCVars%" %VCVARSPLATFORM%
IF "%CXX%" EQU "vc9" (
  devenv mordor.sln /Build "%CONFIG%|%PLATFORM%"
) ELSE IF "%CXX%" EQU "vc10" (
  MSBuild /maxcpucount -P:configuration=%CONFIG% -P:platform=%PLATFORM% mordor2010.sln
)
@IF ERRORLEVEL 1 EXIT /B

MKDIR packages
COPY /y %PLATFORM%\%CONFIG%\tests.exe packages\tests.exe
