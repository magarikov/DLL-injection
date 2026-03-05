// dllmain.cpp : Определяет точку входа для приложения DLL.
#include "framework.h"
#include <stdio.h>
#define MAGIC 14

void my_func() {
    FILE* file2;
    fopen_s(&file2, "C:/Users/magar/Desktop/1000101/popka22.txt", "w");
    fprintf(file2, "hello niggazz123");
    fclose(file2);

    WCHAR text[64] = L"hello world";
    MessageBox(NULL, text, L"my func", MB_ICONINFORMATION);
    Beep(10000, 1000);
    return;
}

BYTE bytes_to_change[14] = {
    0xFF, 0x25, 0x00, 0x00, 0x00, 0x00, // JMP [RIP+0]
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 // Сюда ляжет 64-битный адрес
};

int InstallHook() {

    HMODULE hKernel32 = GetModuleHandle(L"kernel32.dll");
    void* FuncAddr = (void*)GetProcAddress(hKernel32, "CreateFileW");

    *(void**)(&bytes_to_change[6]) = (void*)my_func;

    // разрешаем запись в адресное пространство функции
    DWORD oldProtect;
    VirtualProtect(FuncAddr, MAGIC, PAGE_EXECUTE_READWRITE, &oldProtect);

    // на всякий случай сохраняем старые байты
    BYTE origBytes[MAGIC];
    memcpy(origBytes, FuncAddr, MAGIC);

    memcpy(FuncAddr, bytes_to_change, MAGIC);
    return 1;
}

BOOL APIENTRY DllMain( HMODULE hModule, DWORD  reason, LPVOID lpReserved)
{
    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        if (InstallHook()) {
            FILE* file2;
            fopen_s(&file2, "C:/Users/magar/Desktop/1000101/popka.txt", "w");
            fprintf(file2, "hello niggazz11");
            fclose(file2);
        }
       

    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;

}

