#include <stdio.h>
#include <string.h>

void process(char* text, int key, int mode) {
    for (int i = 0; text[i] != '\0' && text[i] != '\n'; i++) {
        if (mode == 1) {
            
            text[i] = text[i] + key;
        } else {
            
            text[i] = text[i] - key;
        }
    }
}

int main() {
    char message[1000];
    int choice;
    int secret_key = 12;

    printf("1. Encrypt (Send)\n2. Decrypt (Receive)\nChoice: ");
    scanf("%d", &choice);
    getchar();

    printf("Enter message: ");
    fgets(message, sizeof(message), stdin);
    message[strcspn(message, "\n")] = 0;

    process(message, secret_key, choice);

    if (choice == 1) {
        printf("\nENCRYPTED (Copy this):\n%s\n", message);
    } else {
        printf("\nDECRYPTED (Original):\n%s\n", message);
    }

    return 0;
}
