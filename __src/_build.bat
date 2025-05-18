@ECHO OFF

VERIFY OTHER 2>nul
SETLOCAL ENABLEEXTENSIONS ENABLEDELAYEDEXPANSION
IF NOT ERRORLEVEL 0 (
  echo Unable to enable extensions
)

SET "SCRIPT_DIR=%~dp0"
ECHO batch dir: %SCRIPT_DIR%
FOR /F "delims=" %%A IN ('cd') DO SET "ORIGINAL_DIR=%%A"
ECHO orig dir: %ORIGINAL_DIR%

SET "logFolder=.\_build_logs"

GOTO :main

:doCommand
SET "logFile=%logFolder%\%~1.log"
SET "command=%~2"
ECHO %command%>"%logFile%" 2>&1
%command%>>"%logFile%" 2>&1
EXIT /B %ERRORLEVEL%

:main

cd "%SCRIPT_DIR%"

RMDIR /S /Q %logFolder%

MKDIR %logFolder%

CALL :doCommand "00_made_build_logs" "echo we did it" && cd>NUL || Goto :END

CALL :doCommand "01_xmake_set_theme" "xmake global --theme=plain" && cd>NUL || Goto :END

CALL :doCommand "02_xmake_configure" "xmake config --import=.vscode\xmake.windows.shared.release.MD.conf -vD -y" && cd>NUL || Goto :END

CALL :doCommand "03_xmake_build" "xmake build -a -vD" && cd>NUL || Goto :END

CALL :doCommand "05_xmake_run" "xmake run -vD Video-Frame-Generator '%ORIGINAL_DIR%'" && cd>NUL || Goto :END

:END
cd %ORIGINAL_DIR%
ENDLOCAL
EXIT /B %ERRORLEVEL%
