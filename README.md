# AutoPatch
A few BOF files to help facilitate reading and writing bytes to DLL function calls. 

Using these BOFs, you should be able to implement your own patches, or a few automatically calculated JMP to C3 patches. 

This is useful for your own AMSI / ETW byte patches, or any other DLL you might want to patch functionality out of.

## read-memory

Usage:

`read-memory dllName functionName startIndex numOfBytesToRead`

For example, to read the first 8 bytes from the AmsiScanBuffer function call:

`read-memory amsi AmsiScanBuffer 0 8`

## write-memory

Usage:

`write-memory dllName functionName startIndex bytesToWrite`

For example, to write \0x90,\0x90,\0x90 to the AmsiScanBuffer function call:

`write-memory amsi AmsiScanBuffer 0 909090`

## autopatch

Usage:

`autopatch dllName functionName numOfBytesToSearch`

For example, to search through 300 bytes from the AmsiScanBuffer function call, looking for a C3 operation to JMP to:

`autopatch amsi AmsiScanBuffer 300`