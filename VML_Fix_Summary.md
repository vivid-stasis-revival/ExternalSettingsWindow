# VML ImGui Bridge 失效 — 根因分析与修复

## 现象

UTMT 导入 `modify_gml/` 中的 GML 文件后 ImGui 窗口正常工作（滑条调节生效），但通过 VML 打包的 `CompatibleInstallationForVML` 模组部署后，ImGui 窗口能弹出但**滑条调节无反应**。

## 排查过程

### 1. 排除 DLL 问题

ImGui 窗口能弹出 → `@@internal:imgui.dll|ImGui_Show` 调用成功 → `extshell.dll` 和 `imgui.dll` 工作正常。问题在 GML 层。

### 2. 定位失效环节

滑条无反应说明**GML 侧的文件桥同步逻辑（`o_imgui_sync_Step_0`）没有被执行**。该逻辑负责每 10 帧读取 `imgui_bridge.json` 并将 ImGui 修改的值应用到 `global.op_*` 变量。

### 3. 分析 VML 源码

阅读 `vml_src/vividstasisModLoader/CodePatcher.cs` 发现关键逻辑：

**`codes/` 目录处理（第 31-133 行）：**

对于 `gml_Object_*` 开头的文件名，VML 解析对象名、事件类型和子类型：

```csharp
var objectName = codeName.AsSpan(new Range(objectPrefix.Length, secondLastUnderscore));
var eventType = codeName.AsSpan(new Range(secondLastUnderscore + 1, lastUnderscore));
// ...
if (!uint.TryParse(codeName.AsSpan(lastUnderscore + 1), out var eventSubtype))
{
    // eventSubtype 无法解析 → manualLink = true → 碰撞事件 → 自动创建对象
}
```

- **`manualLink = true`**（碰撞事件）：**会自动创建对象**并 link 事件（第 109-125 行）
- **`manualLink = false`**（Create/Step/Draw 等普通事件）：不走自动创建对象，直接 `QueueReplace(codeName, code)`（第 98-106 行）

## 根因

VML 的 `codes/` 目录对于 **Create、Step、Draw 等普通事件**：

1. 解析事件类型 → `eventSubtype` 能正常解析为整数 → `manualLink = false`
2. 直接执行 `QueueReplace(codeName, code)`
3. **`QueueReplace` 要求代码条目已存在**。如果对象 `o_imgui_sync` 在 data.win 中不存在，相关的代码条目也没有被 link 到对象上，`QueueReplace` 失败但未报错
4. 代码被静默丢弃，Step 中的文件桥同步逻辑完全没有被注入

对比 UTMT 的"导入 GML"功能：UTMT 在遇到新对象的事件时，会先创建对象，再创建 `UndertaleCode` 并以 `LinkEvent` 关联。VML 的 `codes/` 缺少这一步。

**`codepatches.json` 同样不能创建新 Entry**（第 141-144 行）：
```csharp
var code = data.Code.ByName(patch.Entry);
if (code is null) {
    ConsoleOutput.PrintWarning($"条目 {patch.Entry} 不存在。", ...);
    return;
}
```

## 修复方案

**放弃创建新对象**，改为将文件桥同步逻辑注入到已有的持久化对象 `o_st_handle` 的 Step 事件中。

`o_st_handle` 在 `initiategame_Create_0` 中被创建，在整个游戏生命周期中持续存在，其 `Step_0` 中原本只有一行 `steam_update();`。

### 修改清单

| 文件 | 变更 |
|------|------|
| `codepatches.json` | 3 个条目，全部 Type 2 ExternalFile |
| `codepatches/gml_Object_initiategame_Create_0.gml` | 添加 `ImGui_Show` 调用 |
| `codepatches/gml_Script_execute_shell_simple.gml` | 保持 4 参数包装器 |
| `codepatches/gml_Object_o_st_handle_Step_0.gml` | 保留 `steam_update()` + 追加全部文件桥同步逻辑（18 项 JSON 读写） |

移除：
- `objects/o_imgui_sync.json`（不再需要创建新对象）
- `codes/` 目录（不再通过 codes 导入事件代码）

### 为什么用 Type 2（InsertBefore, Function 为空）

VML 的 Type 2 在 `Function` 为空时，会把 `Value` **追加到代码末尾**。因此原始 `steam_update()` 保持不变，文件桥逻辑被追加在后面，每帧都会执行。

## 经验教训

1. **VML 的 `codes/` 只能替换已有 Entry**——对已存在对象的事件来说是替换，对不存在对象来说是静默失败
2. **创建新对象并添加事件代码**需要 UTMT 手动操作，或通过 `objects/` + 碰撞事件变通实现
3. **`codepatches.json` 也只能修改已有 Entry**——所有 Type 都要求 Entry 已存在于 data.win 中
4. **优先复用已有持久化对象**——`o_st_handle`（`steam_update`）是一个全局持久化对象，是注入全局逻辑的最佳载体
5. 对比 UTMT 和 VML 的同名功能时，要注意它们在代码链接（LinkEvent）机制上的差异

## 最终目录结构

```
CompatibleInstallationForVML/
├── codepatches.json
├── codepatches/
│   ├── gml_Object_initiategame_Create_0.gml
│   ├── gml_Object_o_st_handle_Step_0.gml
│   └── gml_Script_execute_shell_simple.gml
├── raw/
│   ├── execute_shell_simple_ext_x64.dll
│   └── imgui.dll
└── README.md
```
