// test_fixed.c
#include <windows.h>
#include <wininet.h>  // Different header than WinHTTP

int main() {
    // Use InternetOpen from wininet.h instead of WinHttpOpen
    HINTERNET hSession = InternetOpenA("Test", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (hSession) {
        InternetCloseHandle(hSession);
        return 0;
    }
    return 1;
}