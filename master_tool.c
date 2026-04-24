#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Colors
#define RED   "\x1B[31m"
#define GRN   "\x1B[32m"
#define YEL   "\x1B[33m"
#define CYN   "\x1B[36m"
#define MAG   "\x1B[35m"
#define RESET "\x1B[0m"

void header() {
    system("clear");
    printf("%s       _-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_    %s\n", RED, RESET);
    printf("%s       _-_ _-.     ░███████▒        -_ _-_    %s\n", RED, RESET);
    printf("%s       _-_-   .  ▓████████▓█▓█░   . . -_-_    %s\n", RED, RESET);
    printf("%s       _-_      ▓███▓█▓██████▓█▒       _-_    %s\n", RED, RESET);
    printf("%s       _-_  .  ▓▓██▒█▓█▓▓▓██▒███░ .    _-_    %s\n", RED, RESET);
    printf("%s       _-_ .  ▒███▒░▒██▓███▒░▒██░      _-_    %s\n", RED, RESET);
    printf("%s       _-_    ▒▓▓▒░░░▒▒▓░▓▒░░░▒█░   .  _-_    %s\n", RED, RESET);
    printf("%s       _-_.   ▒▓▒░    ░▓█░    ░▒█      _-_    %s\n", RED, RESET);
    printf("%s       _-_     ▒▓▒▒░░▒▒▓██▒░░▒░█.      _-_    %s\n", RED, RESET);
    printf("%s       _-_   .   ▒▓░▒█▓▓░▓█▒░█         _-_    %s\n", RED, RESET);
    printf("%s       _-_ .       ▓█▓▒█▒█▒█        .  _-_    %s\n", RED, RESET);
    printf("%s       _-_-_     .           .  .    _-_-_    %s\n", RED, RESET);
    printf("%s       _-_-_-_-_-_-_skull box_-_-_-_-_-_-_    %s\n\n\n", RED, RESET);
}

void wait() {
    printf("\n%s[DONE]%s Press Enter to return to menu...", GRN, RESET);
    getchar(); getchar();
}

int main() {
    int cat, tool;
    char target[1024];
    char cmd[2048];

    while(1) {
        header();
        printf("%s-<[1]>-%s Information Gathering\n", RED, RESET);
        printf("%s-<[2]>-%s Vulnerability Analysis\n", RED, RESET);
        printf("%s-<[3]>-%s Exploitation Tools\n", RED, RESET);
        printf("%s-<[4]>-%s Password Attacks\n", RED, RESET);
        printf("%s-<[5]>-%s Wireless/WiFi\n", RED, RESET);
        printf("%s-<[6]>-%s Sniffing/Spoofing\n", RED, RESET);
        printf("%s-<[7]>-%s Forensics/Reverse\n", RED, RESET);
        printf("%s-<[0]>-%s Exit\n\n\n", RED, RESET);
        printf("skull-box > ");
        
        if (scanf("%d", &cat) != 1 || cat == 0) break;

        header();
        switch(cat) {
            case 1:
                printf("    %s___________\n__--information--__%s\n", RED, RESET);
		        printf("%s[1]%s Nmap (Quick Scan)\n", RED, RESET);
		        printf("%s[2]%s Nmap (Aggressive)\n", RED, RESET);
		        printf("%s[3]%s Masscan\n", RED, RESET);
		        printf("%s[4]%s Sherlock\n", RED, RESET);
                printf("%s[5]%s TheHarvester\n", RED, RESET);
                printf("%s[0]%s Back\n\n", RED, RESET);
                printf("choose tool > ");
                scanf("%d", &tool);
                if (tool == 1 || tool == 2) {
                    printf("Target IP: "); scanf("%s", target);
                    sprintf(cmd, tool == 1 ? "nmap %s" : "sudo nmap -A -T4 %s", target);
                    system(cmd); wait();
                } else if (tool == 4) {
                    printf("Username: "); scanf("%s", target);
                    sprintf(cmd, "sherlock %s", target);
                    system(cmd); wait();
                }
                break;

            case 2: //web
                printf("%s[1]%s Nikto\n", RED, RESET);
		        printf("%s[2]%s Sqlmap\n", RED, RESET);
		        printf("%s[3]%s Gobuster\n", RED, RESET);
		        printf("%s[4]%s Ffuf\n", RED, RESET);
		        printf("%s[5]%s Burpsuite\n", RED, RESET);
		        printf("%s[0]%s Back\n\n", RED, RESET);
		        printf("choose tool > ");
                scanf("%d", &tool);
                if (tool == 2) {
                    printf("URL: "); scanf("%s", target);
                    sprintf(cmd, "sqlmap -u %s --batch", target);
                    system(cmd); wait();
                } else if (tool == 5) system("burpsuite &");
                break;

            case 3: // EXPLOIT
                printf("%s[1]%s Metasploit (msfconsole)\n", RED, RESET);
		            printf("%s[2]%s Searchsploit\n", RED, RESET);
		            printf("%s[3]%s Commix\n", RED, RESET);
		            printf("%s[0]%s Back\n\n", RED, RESET);
		            printf("choose tool > ");
                scanf("%d", &tool);
                if (tool == 1) system("msfconsole");
                else if (tool == 2) {
                    printf("Search for: "); scanf("%s", target);
                    sprintf(cmd, "searchsploit %s", target);
                    system(cmd); wait();
                }
                break;

            case 4: // PASSWORDS
                printf("%s[1]%s Hashcat\n", RED, RESET);
		            printf("%s[2]%s John The Ripper\n", RED, RESET);
		            printf("%s[3]%s Hydra\n", RED, RESET);
		            printf("%s[4]%s Medusa\n", RED, RESET);
		            printf("%s[5]%s Crunch\n", RED, RESET);
		            printf("%s[0]%s Back\n\n", RED, RESET);
		            printf("choose tool > ");
                scanf("%d", &tool);
                if (tool == 1) system("hashcat --help"); 
                else if (tool == 3) {
                    printf("Target IP: "); scanf("%s", target);
                    sprintf(cmd, "hydra -l admin -P /usr/share/wordlists/rockyou.txt %s ssh", target);
                    system(cmd); wait();
                }
                break;

            case 5: // WIFI
                printf("%s[1]%s Wifite\n", RED, RESET);
		            printf("%s[2]%s Aircrack-ng\n", RED, RESET);
		            printf("%s[3]%s Pixiewps\n", RED, RESET);
		            printf("%s[4]%s Bully\n", RED, RESET);
		            printf("%s[0]%s Back\n\n", RED, RESET);
		            printf("choose tool > ");
                scanf("%d", &tool);
                if (tool == 1) system("sudo wifite");
                break;

            case 6: // SNIFFING
                printf("%s[1]%s Bettercap\n", RED, RESET);
		            printf("%s[2]%s Wireshark\n", RED, RESET);
		            printf("%s[3]%s Tcpdump\n", RED, RESET);
		            printf("%s[0]%s Back\n\n", RED, RESET);
		            printf("choose tool > ");
                scanf("%d", &tool);
                if (tool == 1) system("sudo bettercap");
                else if (tool == 2) system("wireshark &");
                break;

            case 7: // FORENSICS
                printf("%s[1]%s Ghidra\n", RED, RESET);
		            printf("%s[2]%s Binwalk\n", RED, RESET);
		            printf("%s[3]%s Exiftool\n", RED, RESET);
		            printf("%s[0]%s Back\n\n", RED, RESET);
		            printf("choose tool > ");
                scanf("%d", &tool);
                if (tool == 1) system("ghidra");
                else if (tool == 3) {
                    printf("File: "); scanf("%s", target);
                    sprintf(cmd, "exiftool %s", target);
                    system(cmd); wait();
                }
                break;
        }
    }
    return 0;
}
