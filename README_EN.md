# ExternalSettingsWindow
[中文版README](https://github.com/vivid-stasis-revival/ExternalSettingsWindow/blob/main/README.md)<br>
[LICENSE](https://github.com/vivid-stasis-revival/ExternalSettingsWindow/blob/main/LICENSE)

# Build
Modify build.bat:
```batch
set VC=[Your msvc install path]\VC
set KIT=[Your msvc install path]\Windows Kits

set MSVC=%VC%\Tools\MSVC\[Your msvc version]
set SDK=%KIT%\[Your Windows Kits version]

set PATH=%MSVC%\bin\Hostx64\x64;%PATH%
set INCLUDE=%MSVC%\include;%SDK%\Include\[Your Windows SDK version\ucrt;%SDK%\Include\[Your Windows SDK version]\um;%SDK%\Include\[Your Windows SDK version]\shared
set LIB=%MSVC%\lib\x64;%SDK%\Lib\[Your Windows SDK version]\um\x64;%SDK%\Lib\[Your Windows SDK version]\ucrt\x64
```

Double-click build.bat and wait build finish
- execute_shell_simple_ext_x64.dll
- imgui.dll

# Deployment
Copy dlls to vivid/stasis game path and overwrite(remember to backup!)
