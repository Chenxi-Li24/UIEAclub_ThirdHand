@echo off
title ThirdHand Gripper Control

echo ==========================================
echo   ThirdHand - Gripper WiFi Control
echo ==========================================
echo.

echo [1/3] Stopping old server...
taskkill /F /IM node.exe >nul 2>&1

echo [2/3] Starting proxy server...
cd /d "d:\Documents\Thirdhand\GITHUB\UIEAclub_ThirdHand\web-control\server"
start "ThirdHand-Proxy" cmd /c "node proxy.js"

echo [3/3] Waiting for server...
timeout /t 3 /nobreak >nul

echo Opening browser...
start "" http://localhost:3000

echo.
echo Done! Browser opened to http://localhost:3000
echo Keep the proxy window open - closing it will stop the server.
timeout /t 3 /nobreak >nul
