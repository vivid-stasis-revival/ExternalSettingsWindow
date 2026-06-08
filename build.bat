@echo off
setlocal
set VC=C:\env\msvc\VC
set KIT=C:\env\msvc\Windows Kits
set MSVC=%VC%\Tools\MSVC\14.51.36231
set SDK=%KIT%\10

set PATH=%MSVC%\bin\Hostx64\x64;%PATH%
set INCLUDE=%MSVC%\include;%SDK%\Include\10.0.28000.0\ucrt;%SDK%\Include\10.0.28000.0\um;%SDK%\Include\10.0.28000.0\shared
set LIB=%MSVC%\lib\x64;%SDK%\Lib\10.0.28000.0\um\x64;%SDK%\Lib\10.0.28000.0\ucrt\x64

cd /d "%~dp0"

echo Compiling ExtShell...
cl.exe /LD /MD extshell.cpp /Feexecute_shell_simple_ext_x64.dll shell32.lib user32.lib
if %errorlevel% neq 0 (echo FAILED & pause & exit /b %errorlevel%)
echo OK.

echo Compiling ImGUI...
set IMGUI_SRC=%~dp0imgui_src
set INCLUDE=%INCLUDE%;%IMGUI_SRC%;%IMGUI_SRC%\backends

cl.exe /LD /MD /EHsc /utf-8 imgui_dll.cpp imgui_src\imgui.cpp imgui_src\imgui_draw.cpp imgui_src\imgui_tables.cpp imgui_src\imgui_widgets.cpp imgui_src\imgui_demo.cpp imgui_src\backends\imgui_impl_win32.cpp imgui_src\backends\imgui_impl_dx11.cpp /Feimgui.dll d3d11.lib d3dcompiler.lib dxgi.lib
if %errorlevel% neq 0 (echo FAILED & pause & exit /b %errorlevel%)
echo OK.

echo Cleaning up...
del /q *.obj *.exp *.lib 2>nul
echo All done.
pause
endlocal
