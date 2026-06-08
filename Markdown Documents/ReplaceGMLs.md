# ReplaceGMLs.md — ImGui Bridge 所需的 GML 替换清单

## 需要导入/替换的文件

所有文件位于 `modify_gml/` 目录下。

---

### 1. gml_Script_execute_shell_simple.gml

替换原版的包装器脚本，保持 4 参数兼容。

```gml
var _path = argument[0];
var _args = (argument_count > 1) ? argument[1] : "";
var _action = (argument_count > 2) ? argument[2] : "open";
var _showCmd = (argument_count > 3) ? argument[3] : 5;
return execute_shell_simple_raw(_path, _args, _action, _showCmd);
```

**与原版区别：** 无改动，与原版一致。仅确保未受之前 csx 修改影响。

---

### 2. gml_Object_initiategame_Create_0.gml

游戏初始化对象。新增 `o_imgui_sync` 创建 + ImGui 启动。

在原版末尾 `event_user(0);` 之前加入：

```gml
instance_create_depth(0, 0, -999990, o_imgui_sync);
execute_shell_simple("@@internal:imgui.dll|ImGui_Show");
```

**完整改后代码（第 61-63 行）：**
```gml
instance_create_depth(0, 0, 0, o_st_handle);
instance_create_depth(0, 0, -999990, o_imgui_sync);
execute_shell_simple("@@internal:imgui.dll|ImGui_Show");
blacklist = http_get_file("https://shrinereport.xyz/blacklist.vsd", "blacklist.vsd");
```

其余代码不变。

---

### 3. gml_Object_o_imgui_sync_Create_0.gml（新对象）

新建对象 `o_imgui_sync` 的 Create 事件。

```gml
depth = -999990;
persistent = true;
```

---

### 4. gml_Object_o_imgui_sync_Step_0.gml（新对象）

新建对象 `o_imgui_sync` 的 Step 事件。初始化 ImGui + 文件桥同步。

```gml
// Init ImGui + bridge file on first step
if (!variable_global_exists("__imgui_init_done"))
{
    global.__imgui_init_done = true;
    global.__imgui_tick = 0;
    execute_shell_simple("@@internal:imgui.dll|ImGui_Show");
    var _f = file_text_open_write(working_directory + "imgui_bridge.json");
    file_text_write_string(_f, string(variable_global_exists("op_note_speed") ? global.op_note_speed : 10));
    file_text_close(_f);
}

// Read bridge file every 10 frames
global.__imgui_tick = global.__imgui_tick + 1;
if (global.__imgui_tick >= 10)
{
    global.__imgui_tick = 0;
    var _path = working_directory + "imgui_bridge.json";
    if (file_exists(_path))
    {
        var _f = file_text_open_read(_path);
        var _raw = file_text_read_string(_f);
        file_text_close(_f);
        var _val = real(_raw);
        if (_val > 0 && abs(_val - global.op_note_speed) >= 0.5)
        {
            global.op_note_speed = _val;
            if (instance_exists(cc))
                with (cc) mod_scrollspeed = (_val / 5) + 1;
        }
    }
}
```

---

### 5. gml_Object_o_imgui_sync_Draw_64.gml（新对象 - 空）

```gml
```

（空文件，保留以备后续添加调试绘制）

---

### 6. gml_Object_persistence_Draw_64.gml（清空）

```gml
```

清空之前残留的调试代码。

---

## 需要部署的 DLL 文件

| DLL | 源路径 | 目标路径 |
|-----|--------|----------|
| `execute_shell_simple_ext_x64.dll` | 编译产物 | 游戏目录（覆盖原版） |
| `imgui.dll` | 编译产物 | 游戏目录（新文件） |

---

## 导入步骤

1. 在 UTMT 中打开 `data.win`
2. 依次导入以上 6 个 `.gml` 文件（`gml_Script_*` 导入到 Scripts，`gml_Object_*` 导入到 Objects）
3. 对于新对象 `o_imgui_sync`，需要先在 UTMT 中手动右键创建该 object
4. 勾选 **自动链接导入代码**
5. 应用并保存 `data.win`
6. 将两个 DLL 复制到游戏目录
