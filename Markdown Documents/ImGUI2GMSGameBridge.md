# ImGui DLL to GameMaker File-Based Bridge

## 架构概览

ImGui DLL 和 GameMaker GML 通过**文件 I/O**（`imgui_bridge.json`）进行双向数据通信，实现实时游戏变量修改。

```
GML (o_imgui_sync Step)          imgui.dll (ImGui Thread)
       │                                    │
       │  写初始值                           │
       │──────────────────────────────────► │
       │  imgui_bridge.json                 │
       │                                    │  每帧读文件 → 显示滑条
       │                                    │  滑条变化 → 写回文件
       │                                    │
       │  每10帧读文件                        │
       │◄────────────────────────────────── │
       │  imgui_bridge.json                 │
       │                                    │
       │  _val != op_note_speed?            │
       │  op_note_speed = _val              │
       │  mod_scrollspeed 更新               │
```

## 关键设计决策

### 为什么不用 Shared Memory

1. `.shared` section 只在**同一 DLL 的多个加载实例**间共享，跨 DLL（extshell → imgui）不起作用
2. `extshell` 通过 `GetProcAddress` 调 `ImGui_UpdateFloat` 等函数传递数据，但**频繁的 DLL 函数调用导致 `@@internal:` 前缀匹配失败**（GameMaker 的 Extension 调用频率限制或线程问题）
3. ImGui 在线程中运行，跨 DLL 的函数调用在 x64 调用约定下参数传递不稳定

### 为什么用文件 I/O

1. 两个进程/线程都能读写同一文件
2. GML 的 `file_text_*` 函数原生支持
3. C++ 的 `fopen`/`fprintf`/`fscanf` 简单可靠
4. 低频率同步（每 N 帧一次），文件 I/O 开销可忽略

### 文件路径

两边都必须用同一个绝对路径：
- **GML 侧**：`working_directory + "imgui_bridge.json"` → `%LOCALAPPDATA%\VIVIDSTASIS\`
- **C++ 侧**：`ExpandEnvironmentStringsA("%LOCALAPPDATA%\\VIVIDSTASIS\\imgui_bridge.json")`

## 源码

### imgui_dll.cpp（核心改动）

```cpp
// 文件路径构建
static char g_bridgePath[MAX_PATH] = {0};
static void BuildBridgePath() {
    if (g_bridgePath[0]) return;
    ExpandEnvironmentStringsA("%LOCALAPPDATA%\\VIVIDSTASIS\\imgui_bridge.json", g_bridgePath, MAX_PATH);
}

// 读文件（ImGui 渲染线程每帧调用）
static void ReadBridge() {
    BuildBridgePath();
    FILE* f = fopen(g_bridgePath, "r");
    if (f) { fscanf(f, "%f", &g_noteSpeed); fclose(f); }
}

// 写文件（滑条变化时调用）
static void WriteBridge() {
    BuildBridgePath();
    FILE* f = fopen(g_bridgePath, "w");
    if (f) { fprintf(f, "%.1f", g_noteSpeed); fflush(f); fclose(f); }
}
```

ImGui UI 渲染帧中：
```cpp
// 每帧从文件读取 GML 写入的最新值
ReadBridge();

// 滑条绑定全局变量
if (ImGui::SliderFloat("Note Speed", &g_noteSpeed, 1.0f, 50.0f, "%.0f"))
    WriteBridge();  // 主动变化时写回文件
```

### GML 侧（o_imgui_sync_Step_0）

```gml
// 初始化：启动 ImGui + 写初始值
if (!variable_global_exists("__imgui_init_done")) {
    global.__imgui_init_done = true;
    execute_shell_simple("@@internal:imgui.dll|ImGui_Show");
    var _f = file_text_open_write(working_directory + "imgui_bridge.json");
    file_text_write_string(_f, string(op_note_speed));
    file_text_close(_f);
}

// 每 10 帧读回 ImGui 修改的值
if (++tick >= 10) {
    tick = 0;
    if (file_exists(working_directory + "imgui_bridge.json")) {
        var val = real(file_text_read_string(file_text_open_read(...)));
        if (val > 0 && abs(val - op_note_speed) >= 0.5) {
            op_note_speed = val;
            // 应用到 chart controller
            if (instance_exists(cc))
                with (cc) mod_scrollspeed = (val / 5) + 1;
        }
    }
}
```

## 故障排查

### 前缀匹配问题

GameMaker 的 GML 字符串 `"@@..."` 传到 DLL 时，`@@` 可能被转义。解决方案：
- C++ 侧只检测第一个 `@` 字符（不要求两个 `@`）
- 动态跳过前缀到第一个 `:` 之后

```cpp
if (utf8_file[0] == '@' || (utf8_file[0] == 'D' && utf8_file[1] == 'L'))
    return internalDispatch(utf8_file);
```

内部 `internalDispatch` 动态跳过前缀：
```cpp
const char* payload = utf8_file;
while (*payload && *payload != ':') payload++;
if (*payload == ':') payload++;
```

### Draw 事件不触发

GameMaker 的 `Draw`/`Draw_64` 事件需要对象 `visible=true`。持久化对象在不同 room 切换后可能不触发 Draw。**改用 Step 事件**（每帧必定触发）。

### 文件路径不一致

GML 的 `working_directory` 指向 `%LOCALAPPDATA%\VIVIDSTASIS\`，C++ 的 `GetCurrentDirectory` 可能不同。**两方都用 `%LOCALAPPDATA%\VIVIDSTASIS\imgui_bridge.json` 绝对路径**。

### DLL 不能频繁 FreeLibrary

ImGui 在已加载的 DLL 中持有状态（Context、D3D Device、Window）。如果每次调用后 `FreeLibrary`，下次调用时 DLL 被重新加载，所有状态丢失导致崩溃。**调用后不卸载 DLL**，进程退出时系统自动清理。

## 编译

```bash
cl.exe /LD /MD /EHsc imgui_dll.cpp imgui.cpp imgui_draw.cpp imgui_tables.cpp \
  imgui_widgets.cpp imgui_demo.cpp imgui_impl_win32.cpp imgui_impl_dx11.cpp \
  /Feimgui.dll d3d11.lib d3dcompiler.lib dxgi.lib
```

## 导出函数

| 函数 | 签名 | 用途 |
|------|------|------|
| `ImGui_Show` | `void(void)` | 启动 ImGui 渲染线程 |
| `ImGui_Hide` | `void(void)` | 停止 ImGui 窗口 |

`ImGui_UpdateFloat`、`ImGui_ReadFloat` 等函数在文件桥接方案中不再使用。
