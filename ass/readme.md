File Collection & Transmission Tool (Red Team Lab Simulation)

Overview

This project demonstrates a simulated data collection and exfiltration workflow for red team or cybersecurity lab environments. It is intended strictly for educational use inside controlled virtual machines (VMs).

The program scans user directories, processes selected files, and transmits them to a remote endpoint to emulate attacker behavior for analysis and detection exercises.

---

Features

- Stealth Execution
  
  - Hides console window
  - Introduces delayed execution to mimic real-world evasion

- Targeted File Discovery
  
  - Recursively scans common user directories:
    - Documents
    - Desktop
    - Downloads
    - Pictures
    - Music
  - Filters files by predefined extensions (documents, images, archives, configs)

- File Processing
  
  - Encrypts files using:
    - AES-256 (Windows CryptoAPI)
    - XOR fallback (if AES fails)
  - Stores processed files temporarily

- Network Communication
  
  - Sends processed files to a remote server over HTTPS
  - Uses WinHTTP API for outbound requests

- Cleanup Mechanism
  
  - Deletes temporary working files and directories
  - Optionally removes the executable after execution

---

How It Works

1. Initialization
   
   - Program hides its window and waits before execution.

2. Environment Setup
   
   - Creates a temporary working directory.

3. Discovery Phase
   
   - Identifies target directories from the user profile.
   - Recursively scans for files matching specific extensions.

4. Processing Phase
   
   - Copies and encrypts selected files into a temporary location.

5. Transmission Phase
   
   - Sends encrypted files to a configured remote endpoint via HTTPS.

6. Cleanup Phase
   
   - Deletes temporary files and optionally self-deletes.

---

Configuration

Key parameters can be modified in the source code:

- "SLEEP_SECONDS" – Delay before execution
- "C2_URL" – Remote server endpoint
- "AES_KEY_SIZE" – Encryption strength (default: 256-bit)
- Target file extensions list (inside "is_target_file()")

---

Compilation

Compile using a Windows C compiler (e.g., MinGW or MSVC):

gcc program.c -o program.exe -lwininet -ladvapi32 -lcrypt32

---

Hello matey, ill be deleting this code soon
