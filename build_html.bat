@echo off
echo Building HTML for production...
cd dev_html
call npm install
call node prod.js
echo.
echo HTML built successfully!
echo File size: 
for %%A in (..\main\wwwroot\index.html) do echo %%~zA bytes
echo.
echo You can now build and flash the ESP32 project.
pause
