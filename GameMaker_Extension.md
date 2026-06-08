# GameMaker Extension DLL 参数数量兼容性问题

## 问题描述

替换 GameMaker 项目中的外部 DLL（Extension DLL）后，原有功能（如 `ShellExecuteW` 启动 `otherside.exe`）完全失效，无任何报错或弹窗。

## 排查过程

### 1. 检查 DLL 是否正确加载
在原 DLL 中加入 `DllMain` 的 `DLL_PROCESS_ATTACH` 弹窗 → 弹窗出现，证明 DLL 被正确加载。

### 2. 检查函数是否被导出
`dumpbin /exports` → 函数已正确导出。

### 3. 检查 GML 脚本是否被调用
通过 csx 替换 GML 脚本插入 `show_message` → 弹窗未出现，说明 GML 层代码未被执行。

### 4. 检查 VM 层面的调用绑定
对 `gml_Script_execute_shell_simple` 进行反汇编，发现关键指令：

```
call.i execute_shell_simple_raw(argc=4)
```

**`call.i` 而非 `call.e`**：说明 `execute_shell_simple_raw` 在 VM 层面被当作内部函数处理（即使它实际由 Extension DLL 导出）。

## 根因

GameMaker 的 Extension 机制通过 `autogen.gml` 将 DLL 导出函数注册到 VM 中。注册时函数签名（参数数量）被固定写在 `data.win` 的 CODE 字节码中。

当 C++ DLL 的函数签名从 4 参数改为 5 或 7 参数时：

```c
// 原版 (4参数)
BOOL execute_shell_simple_raw(LPCCH file, LPCCH params, LPCCH op, int showCmd);

// 修改版 (5参数) — 不兼容！
BOOL execute_shell_simple_raw(LPCCH file, LPCCH params, LPCCH op, int showCmd, BOOL isInternal);
```

GM 的 VM 仍然只压栈 4 个参数。C++ 侧的第 5 个参数会读取栈上的**未初始化数据（垃圾值）**。

如果垃圾值恰好为非零（x64 调用约定下极大概率），`isExecuteInternalFunc` 被解释为 `TRUE`，函数进入内部 DLL 执行分支，尝试用垃圾字符串指针调用 `MultiByteToWideChar` / `LoadLibraryW`，失败并返回 `FALSE`，导致本应执行的 `ShellExecuteW` 被完全跳过。

## 原理

### GameMaker Extension 调用链

```
GML 脚本 (gml_Script_execute_shell_simple)
  └─ VM 字节码: call.i execute_shell_simple_raw(argc=N)
       └─ Extension 绑定表 → DLL 导出函数
            └─ C++ 函数接收 N 个压栈参数
```

### 参数绑定机制

`autogen.gml` 是 Extension 自动生成的 VM 绑定文件。它在 Extension 加载时将 DLL 函数签名注册到 GM 虚拟机。GM 的编译/导入工具根据 Extension 定义中的参数数量在字节码中硬编码 `argc`。

UTMT 无法直接查看 Extension 的内部绑定信息（它只导出 GML 脚本），但通过反汇编 CODE 条目中的 `call.i` 指令可以确定实际参数数量。

### Windows x64 调用约定

x64 使用 fastcall 变体，前 4 个整型参数通过寄存器传递（RCX, RDX, R8, R9），第 5 个开始通过栈传递。当 VM 只压 4 个参数时，第 5 个参数的栈位置是未初始化的，无法预测其值。

## 解决方案

保持 DLL 导出函数与原版**参数数量完全一致**。通过已有参数的空间编码额外功能：

```c
// 完全兼容原版 4 参数
BOOL execute_shell_simple_raw(LPCCH utf8_file, LPCCH utf8_params, LPCCH utf8_operation, int nShowCmd)
{
    // 通过 utf8_file 的特殊前缀触发新增功能
    if (is_prefix(utf8_file, "@@internal:")) {
        // 解析 "@@internal:dllPath|funcName"
        // 执行 LoadLibrary + GetProcAddress + call
    }
    // 原版逻辑不变
    ShellExecuteW(...);
}
```

### 调用方式

```gml
// 原版调用（不变）
execute_shell_simple("otherside.exe");

// 新增：执行内部 DLL
execute_shell_simple("@@internal:example.dll|ShowTest");
```

## 关键教训

1. 修改 GameMaker Extension DLL 时，**导出函数的参数数量必须与原版一致**，否则 VM 压栈不足导致未定义行为
2. 扩展新功能应通过**已有参数的内容编码**（前缀、分隔符等）而非增加参数
3. 排查此类问题时，**反汇编 GML 字节码**是确定实际调用签名的关键步骤
4. UTMT 的 Scripts/Code 反汇编视图可以看到 `call.i` 和 `argc`，比只看 .gml 源码更准确
