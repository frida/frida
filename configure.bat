@setlocal
@echo off
rem:: Based on: https://github.com/microsoft/terminal/issues/217#issuecomment-737594785
goto :_start_

:set_real_dp0
set dp0=%~dp0
set "dp0=%dp0:~0,-1%"
goto :eof

:_start_
call :set_real_dp0

if not exist "%dp0%\releng\meson\meson.py" (
  python "%dp0%\tools\ensure-submodules.py"
  if %errorlevel% neq 0 exit /b %errorlevel%
)

endlocal & goto #_undefined_# 2>nul || title %COMSPEC% & python ^
    -c "import sys; sys.path.insert(0, sys.argv[1]); from releng.meson_configure import main; main()" ^
    "%dp0%" ^
    %*
