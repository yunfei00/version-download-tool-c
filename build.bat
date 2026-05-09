@echo off
setlocal

set BUILD_VERSION=local

echo Building version-download-tool.exe ...
gcc src\main.c src\app.c src\downloader.c src\config.c resources\app.rc -o version-download-tool.exe -mwindows -lcomctl32 -lwinhttp -D BUILD_VERSION=\"%BUILD_VERSION%\"
if errorlevel 1 (
  echo Build failed.
  exit /b 1
)

echo Build succeeded: version-download-tool.exe
