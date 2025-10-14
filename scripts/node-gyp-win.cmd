@echo off
setlocal
set NG=%~dp0..\node_modules\.bin\node-gyp.cmd
if exist "%NG%" (
  "%NG%" %*
) else (
  node "%~dp0..\node_modules\node-gyp\bin\node-gyp.js" %*
)

