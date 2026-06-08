# ExternalSettingsWindow
[English version readme](https://github.com/vivid-stasis-revival/ExternalSettingsWindow/blob/main/README_EN.md)<br>
[LICENSE](https://github.com/vivid-stasis-revival/ExternalSettingsWindow/blob/main/LICENSE)

# 如何构建
build.bat进行修改
```batch
set VC=[你的msvc安装路径]\VC
set KIT=[你的msvc安装路径]\Windows Kits

set MSVC=%VC%\Tools\MSVC\[你的msvc版本]
set SDK=%KIT%\[你的Windows Kits版本]

set PATH=%MSVC%\bin\Hostx64\x64;%PATH%
set INCLUDE=%MSVC%\include;%SDK%\Include\[你的Windows SDK版本\ucrt;%SDK%\Include\[你的Windows SDK版本]\um;%SDK%\Include\[你的Windows SDK版本]\shared
set LIB=%MSVC%\lib\x64;%SDK%\Lib\[你的Windows SDK版本]\um\x64;%SDK%\Lib\[你的Windows SDK版本]\ucrt\x64
```

然后双击运行build.bat等待构建完成即可
- execute_shell_simple_ext_x64.dll
- imgui.dll

# 如何使用
复制到vivid/stasis游戏目录并覆盖（记得备份！）
