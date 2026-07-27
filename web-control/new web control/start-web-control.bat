@echo off
setlocal
chcp 65001 >nul
pushd "%~dp0"

where node >nul 2>&1
if errorlevel 1 (
  echo Node.js is required. Install Node.js and run this file again.
  pause
  popd
  exit /b 1
)

where npm >nul 2>&1
if errorlevel 1 (
  echo npm was not found. Reinstall Node.js with npm enabled.
  pause
  popd
  exit /b 1
)

if not exist "node_modules\ws\package.json" (
  echo Installing dependencies...
  call npm install --no-audit --no-fund
  if errorlevel 1 (
    echo Dependency installation failed.
    pause
    popd
    exit /b 1
  )
)

echo Starting ThirdHand Web Control...
start "" powershell.exe -NoProfile -WindowStyle Hidden -Command "Start-Sleep -Seconds 2; Start-Process 'http://127.0.0.1:3000/'"
call npm start
set "THIRDHAND_EXIT_CODE=%ERRORLEVEL%"

popd
endlocal & exit /b %THIRDHAND_EXIT_CODE%
