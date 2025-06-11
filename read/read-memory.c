#include <windows.h>
#include "beacon.h"

WINBASEAPI FARPROC WINAPI KERNEL32$GetProcAddress (HMODULE, LPCSTR);
WINBASEAPI HANDLE WINAPI KERNEL32$GetCurrentProcess (VOID);
WINBASEAPI HMODULE WINAPI KERNEL32$GetModuleHandleA (LPCSTR);
WINBASEAPI HMODULE WINAPI KERNEL32$LoadLibraryA (LPCSTR);
WINBASEAPI WINBOOL WINAPI KERNEL32$ReadProcessMemory (HANDLE, LPCVOID, LPVOID, SIZE_T, SIZE_T*);
WINBASEAPI HANDLE WINAPI KERNEL32$GetProcessHeap (VOID);
WINBASEAPI LPVOID WINAPI KERNEL32$HeapAlloc (HANDLE, DWORD, SIZE_T);
WINBASEAPI WINBOOL WINAPI KERNEL32$HeapFree (HANDLE, DWORD, LPVOID);
WINBASEAPI WINBOOL WINAPI KERNEL32$CloseHandle (HANDLE hObject);

void go(char *args, int length) {
    LPCSTR dllName = NULL;
    char *functionName;
    int startIndex;
    int numberOfBytes;
    HANDLE hModule;
    FARPROC functionAddress;
    SIZE_T bytesRead;
    BYTE *buffer;

    // Initialize the data parser
    datap parser;
    BeaconDataParse(&parser, args, length);

    // Extract arguments
    dllName = BeaconDataExtract(&parser, NULL);
    functionName = BeaconDataExtract(&parser, NULL);
    startIndex = BeaconDataInt(&parser);
    numberOfBytes = BeaconDataInt(&parser);

    // Allocate buffer dynamically based on numberOfBytes using HeapAlloc
    buffer = (BYTE*)KERNEL32$HeapAlloc(KERNEL32$GetProcessHeap(), 0, numberOfBytes);
    if (!buffer) {
        BeaconPrintf(CALLBACK_ERROR, "Failed to allocate memory for buffer.\n");
        return;
    }

    // Attempt to load the DLL if it's not already loaded
    hModule = GetModuleHandleA(dllName);
    if (hModule == NULL) {
        hModule = LoadLibraryA(dllName);
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
        KERNEL32$HeapFree(KERNEL32$GetProcessHeap(), 0, buffer); // Free allocated memory
        return;
    }

    // Calculate target address
    void *targetAddress = (void*)((BYTE*)functionAddress + startIndex);

    // Open the current process (assuming self-targeting for demonstration)
    HANDLE hProcess = KERNEL32$GetCurrentProcess();
    if (!hProcess) {
        BeaconPrintf(CALLBACK_ERROR, "Failed to open process.\n");
        KERNEL32$HeapFree(KERNEL32$GetProcessHeap(), 0, buffer); // Free allocated memory
        return;
    }

    // Read memory
    if (KERNEL32$ReadProcessMemory(hProcess, targetAddress, buffer, numberOfBytes, &bytesRead)) {
        // Allocate enough space for the hex string representation
        char *hexOutput = (char*)KERNEL32$HeapAlloc(KERNEL32$GetProcessHeap(), 0, bytesRead * 4 + 1); // \xXX and XX
        char *simpleHexOutput = (char*)KERNEL32$HeapAlloc(KERNEL32$GetProcessHeap(), 0, bytesRead * 2 + 1); // XX

        if (!hexOutput || !simpleHexOutput) {
            BeaconPrintf(CALLBACK_ERROR, "Failed to allocate memory for hex output.\n");
            KERNEL32$HeapFree(KERNEL32$GetProcessHeap(), 0, buffer);
            KERNEL32$HeapFree(KERNEL32$GetProcessHeap(), 0, hexOutput);
            KERNEL32$HeapFree(KERNEL32$GetProcessHeap(), 0, simpleHexOutput);
            KERNEL32$CloseHandle(hProcess);
            return;
        }

        // Create \xXX format
        for (SIZE_T i = 0; i < bytesRead; i++) {
            hexOutput[i * 4] = '\\';
            hexOutput[i * 4 + 1] = 'x';
            hexOutput[i * 4 + 2] = "0123456789ABCDEF"[buffer[i] >> 4];
            hexOutput[i * 4 + 3] = "0123456789ABCDEF"[buffer[i] & 0xF];
        }
        hexOutput[bytesRead * 4] = '\0';

        // Create plain hex format
        for (SIZE_T i = 0; i < bytesRead; i++) {
            simpleHexOutput[i * 2] = "0123456789ABCDEF"[buffer[i] >> 4];
            simpleHexOutput[i * 2 + 1] = "0123456789ABCDEF"[buffer[i] & 0xF];
        }
        simpleHexOutput[bytesRead * 2] = '\0';

        BeaconPrintf(CALLBACK_OUTPUT, "Bytes in \\x format: %s\n", hexOutput);
        BeaconPrintf(CALLBACK_OUTPUT, "Bytes in hex format: %s\n", simpleHexOutput);

        // Clean up
        KERNEL32$HeapFree(KERNEL32$GetProcessHeap(), 0, hexOutput);
        KERNEL32$HeapFree(KERNEL32$GetProcessHeap(), 0, simpleHexOutput);
    } else {
        BeaconPrintf(CALLBACK_ERROR, "Failed to read memory.\n");
    }
    // Clean up
    KERNEL32$HeapFree(KERNEL32$GetProcessHeap(), 0, buffer); // Free allocated memory
    KERNEL32$CloseHandle(hProcess);
}