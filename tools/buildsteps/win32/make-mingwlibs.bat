@ECHO OFF
SETLOCAL

rem batch file to compile mingw libs via BuildSetup
SET WORKDIR=%base_dir%

SET PROMPTLEVEL=prompt
SET BUILDMODE=clean
SET opt=mintty
SET build32=no
SET build64=yes
SET vcarch=x86
SET msys2=msys64
SET tools=mingw
FOR %%b in (%1, %2, %3) DO (
  IF %%b==noprompt SET PROMPTLEVEL=noprompt
  IF %%b==clean SET BUILDMODE=clean
  IF %%b==noclean SET BUILDMODE=noclean
  IF %%b==sh SET opt=sh
  IF %%b==build64 ( 
    SET build64=yes 
    SET build32=no
    SET vcarch=x64
    )
  IF %%b==msvc SET tools=msvc
)
rem set MSVC env
call "%VS140COMNTOOLS%..\..\VC\vcvarsall.bat" %vcarch% || exit /b 1

IF "%WORKDIR%"=="" (
  SET WORKDIR=%~dp0\..\..\..
)

REM Prepend the msys and mingw paths onto %PATH%
SET MSYS_INSTALL_PATH=%WORKDIR%\project\BuildDependencies\msys
SET PATH=%MSYS_INSTALL_PATH%\mingw\bin;%MSYS_INSTALL_PATH%\bin;%PATH%

SET ERRORFILE=%WORKDIR%\project\Win32BuildSetup\errormingw

SET BS_DIR=%WORKDIR%\project\Win32BuildSetup
rem cd %BS_DIR%

IF EXIST %ERRORFILE% del %ERRORFILE% > NUL

rem compiles a bunch of mingw libs and not more
IF %opt%==sh (
  IF EXIST %WORKDIR%\project\BuildDependencies\%msys2%\usr\bin\sh.exe (
    ECHO starting sh shell
    %WORKDIR%\project\BuildDependencies\%msys2%\usr\bin\sh.exe --login -i /xbmc/tools/buildsteps/win32/make-mingwlibs.sh --prompt=%PROMPTLEVEL% --mode=%BUILDMODE% --build32=%build32% --build64=%build64% --tools=%tools%
    GOTO END
  ) ELSE (
    GOTO ENDWITHERROR
  )
)
IF EXIST %WORKDIR%\project\BuildDependencies\%msys2%\usr\bin\mintty.exe (
  ECHO starting mintty shell
  %WORKDIR%\project\BuildDependencies\%msys2%\usr\bin\mintty.exe -d -i /msys2.ico /usr/bin/bash --login /xbmc/tools/buildsteps/win32/make-mingwlibs.sh --prompt=%PROMPTLEVEL% --mode=%BUILDMODE% --build32=%build32% --build64=%build64% --tools=%tools%
  GOTO END
)
GOTO ENDWITHERROR

:ENDWITHERROR
  ECHO msys environment not found
  ECHO bla>%ERRORFILE%
  EXIT /B 1
  
:END
  ECHO exiting msys environment
  IF EXIST %ERRORFILE% (
    ECHO failed to build mingw libs
    EXIT /B 1
  )
  EXIT /B 0

ENDLOCAL
