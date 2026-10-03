@echo off
setlocal
set "BVB_PLAYER=%~dp0build\windows\Release\beetle_vb_openxr.exe"
if not exist "%BVB_PLAYER%" (
  echo Build the Windows preset first. See README.md.
  pause
  exit /b 1
)
"%BVB_PLAYER%" %*
if errorlevel 1 pause
