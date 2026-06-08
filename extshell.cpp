/*    ExternalSettingsWindow (Copyright (C) 2026 MalNEW)
    
    本程序是自由软件：您可以根据自由软件基金会发布的 GNU 通用公共许可证（第 3 版）或（根据您的选择）任何后续版本的条款对其进行重新分发和/或修改。
    
    本程序的分发是希望它能有用，但没有任何担保；甚至没有对适销性或特定用途适用性的暗示担保。有关详细信息，请参阅 GNU 通用公共许可证。
    
    您应该已随本程序收到一份 GNU 通用公共许可证的副本。如果没有，请参阅 <http://gnu.org>。
*/

#include <windows.h>

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    return TRUE;
}

// Parse @@internal: protocol and dispatch DLL calls with args.
// Format: "@@internal:dllPath|funcName"             -> void(void)
//         "@@internal:dllPath|funcName|int"         -> void(int) or float(int)
//         "@@internal:dllPath|funcName|int|float"   -> void(int,float)
static BOOL internalDispatch(LPCCH utf8_file)
{
    // Skip past prefix until ':'
    const char* payload = utf8_file;
    while (*payload && *payload != ':') payload++;
    if (*payload == ':') payload++;

    char dllPath[640], funcName[384];
    int argCount = 0;
    int iArgs[2] = {0, 0};
    float fArgs[2] = {0.0f, 0.0f};
    int argTypes[2] = {0, 0};

    const char* segs[4] = {NULL, NULL, NULL, NULL};
    int nSeg = 0;
    segs[0] = payload;
    for (const char* p = payload; *p; p++) {
        if (*p == '|') {
            nSeg++;
            if (nSeg < 4) segs[nSeg] = p + 1;
        }
    }

    if (nSeg < 1) return 40;

    // dllPath
    int dllLen = 0;
    {
        const char* end = nSeg >= 2 ? segs[1] - 1 : segs[0];
        while (end[dllLen] && end[dllLen] != '|') dllLen++;
    }
    if (dllLen >= (int)sizeof(dllPath)) return 41;
    memcpy(dllPath, segs[0], dllLen);
    dllPath[dllLen] = 0;

    // funcName
    int funcLen = 0;
    {
        const char* start = segs[1];
        while (start[funcLen] && start[funcLen] != '|') funcLen++;
    }
    if (funcLen >= (int)sizeof(funcName)) return 42;
    memcpy(funcName, segs[1], funcLen);
    funcName[funcLen] = 0;

    // extra args
    argCount = nSeg - 1;
    for (int i = 0; i < argCount && i < 2; i++) {
        const char* s = segs[i + 2];
        if (!s) break;
        // Auto-detect float: has dot, OR function name contains "Float" and this is arg 1
        int isFloat = 0;
        for (const char* q = s; *q && *q != '|'; q++)
            if (*q == '.') isFloat = 1;
        if (!isFloat && i == 1 && strstr(funcName, "Float"))
            isFloat = 1;

        if (isFloat) {
            argTypes[i] = 1;
            fArgs[i] = 0.0f;
            int decimals = 0, pastDot = 0;
            for (const char* q = s; *q && *q != '|'; q++) {
                if (*q == '.') { pastDot = 1; continue; }
                fArgs[i] = fArgs[i] * 10 + (*q - '0');
                if (pastDot) decimals++;
            }
            while (decimals-- > 0) fArgs[i] /= 10.0f;
        } else {
            argTypes[i] = 0;
            iArgs[i] = 0;
            for (const char* q = s; *q && *q != '|'; q++)
                iArgs[i] = iArgs[i] * 10 + (*q - '0');
        }
    }

    // load DLL and dispatch
    WCHAR wszDllPath[640];
    if (MultiByteToWideChar(CP_UTF8, 0, dllPath, -1, wszDllPath, 640) == 0)
        return 10;

    HMODULE hDll = LoadLibraryW(wszDllPath);
    if (!hDll) return 20;

    FARPROC pFunc = GetProcAddress(hDll, funcName);
    if (!pFunc) { FreeLibrary(hDll); return 30; }

    BOOL ret = 99;
    if (argCount == 0) {
        ((void (*)(void))pFunc)();
    } else if (argCount == 1) {
        if (argTypes[0] == 1)
            ret = (BOOL)(int)((float (*)(float))pFunc)(fArgs[0]);
        else if (strstr(funcName, "ReadFloat"))
            ret = (BOOL)(int)((float (*)(int))pFunc)(iArgs[0]);
        else
            ret = ((int (*)(int))pFunc)(iArgs[0]);
    } else if (argCount == 2) {
        if (argTypes[0] == 0 && argTypes[1] == 1)
            ((void (*)(int, float))pFunc)(iArgs[0], fArgs[1]);
        else if (argTypes[0] == 0 && argTypes[1] == 0)
            ((void (*)(int, int))pFunc)(iArgs[0], iArgs[1]);
    }

    return ret;
}

extern "C" __declspec(dllexport) BOOL execute_shell_simple_raw(LPCCH utf8_file, LPCCH utf8_params, LPCCH utf8_operation, int nShowCmd)
{
    // Accept any of: "@@", "@internal:", "DLLCALL:", "DL:"
    if (utf8_file) {
        if (utf8_file[0] == '@')
            return internalDispatch(utf8_file);
        if (utf8_file[0] == 'D' && utf8_file[1] == 'L')
            return internalDispatch(utf8_file);
        if (utf8_file[0] == 'D' && utf8_file[1] == 'L' && utf8_file[2] == ':')
            return internalDispatch(utf8_file);
    }

    // normal ShellExecuteW path
    WCHAR wszFile[1024], wszParams[1024], wszOperation[1024], wszDirectory[1024];

    if (MultiByteToWideChar(CP_UTF8, 0, utf8_file, -1, wszFile, 1024) == 0) return FALSE;
    if (MultiByteToWideChar(CP_UTF8, 0, utf8_params, -1, wszParams, 1024) == 0) return FALSE;
    if (MultiByteToWideChar(CP_UTF8, 0, utf8_operation, -1, wszOperation, 1024) == 0) return FALSE;
    if (GetCurrentDirectoryW(1024, wszDirectory) == 0) return FALSE;

    HINSTANCE hInst = ShellExecuteW(NULL, wszOperation, wszFile, wszParams, wszDirectory, nShowCmd);
    return (INT_PTR)hInst > 32;
}

extern "C" __declspec(dllexport) BOOL WINAPI ExecuteExternalLibrary(LPCWSTR lpszDllPath, LPCSTR lpszFuncName)
{
    if (!lpszDllPath || !lpszFuncName) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    HMODULE hExternalDll = LoadLibraryW(lpszDllPath);
    if (hExternalDll == NULL) return FALSE;

    FARPROC pFunc = GetProcAddress(hExternalDll, lpszFuncName);
    if (pFunc == NULL) { FreeLibrary(hExternalDll); return FALSE; }

    ((void (*)(void))pFunc)();
    FreeLibrary(hExternalDll);
    return TRUE;
}
