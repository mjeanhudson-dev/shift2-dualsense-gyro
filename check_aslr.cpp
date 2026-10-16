#include <stdio.h>
#include <stdint.h>
#include <windows.h>

int main() {
    FILE* f = fopen("C:\\Users\\Zachariah\\Downloads\\Need for Speed Shift 2 Unleashed\\Need for Speed Shift 2 Unleashed\\shift2u.exe", "rb");
    if (!f) return 1;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t* b = (uint8_t*)malloc(sz);
    fread(b, 1, sz, f);
    fclose(f);

    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)b;
    IMAGE_NT_HEADERS32* nt = (IMAGE_NT_HEADERS32*)(b + dos->e_lfanew);
    printf("ImageBase: 0x%08lX\n", nt->OptionalHeader.ImageBase);
    printf("DllCharacteristics: 0x%04X\n", nt->OptionalHeader.DllCharacteristics);
    if (nt->OptionalHeader.DllCharacteristics & IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE) {
        printf("ASLR IS ENABLED!\n");
    } else {
        printf("NO ASLR (fixed base).\n");
    }

    printf("AddressOfEntryPoint: 0x%08lX (VA 0x%08lX)\n", nt->OptionalHeader.AddressOfEntryPoint, nt->OptionalHeader.ImageBase + nt->OptionalHeader.AddressOfEntryPoint);

    free(b);
    return 0;
}
