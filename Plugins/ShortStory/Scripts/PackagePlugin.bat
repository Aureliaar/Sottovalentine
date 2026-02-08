@echo off
echo Packaging ShortStory Plugin for Distribution...

REM Navigate to plugin root
cd /d "%~dp0.."

REM Build the Electron editor
echo Building editor...
cd Editor
call npm install
call npm run build:electron

REM Copy packaged editor to plugin root
echo Copying packaged editor...
xcopy /E /Y dist_electron\short-story-editor-win32-x64\* .\

REM Clean up build artifacts (optional)
REM rmdir /S /Q dist
REM rmdir /S /Q dist_electron

echo.
echo ✓ Plugin packaged successfully!
echo ✓ Ready for distribution at: Plugins/ShortStory/
echo.
echo Distribution checklist:
echo   [x] C++ source (Source/)
echo   [x] Editor source (Editor/src/, Editor/electron/)
echo   [x] Editor binary (Editor/short-story-editor.exe)
echo   [x] Content (Content/Stories/)
echo   [x] Documentation (README.md, Editor/README-editor.md)
echo.
echo Excluded (via .gitignore):
echo   [ ] node_modules/ (users run npm install)
echo   [ ] Build artifacts (dist/, dist_electron/)
pause
