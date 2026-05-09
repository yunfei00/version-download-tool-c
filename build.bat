@echo off
setlocal

echo Building version-download-tool.exe ...
gcc src\main.c src\app.c src\downloader.c resources\app.rc -o version-download-tool.exe -mwindows -lcomctl32 -lwinhttp
if errorlevel 1 (
  echo Build failed.
  exit /b 1
)

echo Build succeeded: version-download-tool.exe
