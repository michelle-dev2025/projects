#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    char command[1024];

    if (argc < 3) {
        printf("How to use: %s -e inputfile | -d inputfile [outputfile]\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "-e") == 0) {
        // Encrypt: output = inputfile.grey
        snprintf(command, sizeof(command),
                 "openssl enc -aes-256-cbc -salt -pbkdf2 -in \"%s\" -out \"%s.grey\"",
                 argv[2], argv[2]);

        return system(command);

    } else if (strcmp(argv[1], "-d") == 0) {
        char out[512];

        if (argc < 4) {
            printf("Output file: ");
            if (!fgets(out, sizeof(out), stdin)) {
                fprintf(stderr, "Failed to read output file\n");
                return 1;
            }
            // remove newline
            out[strcspn(out, "\n")] = 0;
        } else {
            strncpy(out, argv[3], sizeof(out));
            out[sizeof(out) - 1] = '\0';
        }

        snprintf(command, sizeof(command),
                 "openssl enc -aes-256-cbc -d -pbkdf2 -in \"%s\" -out \"%s\"",
                 argv[2], out);

        return system(command);

    } else {
        printf("How to use: %s -e inputfile | -d inputfile [outputfile]\n", argv[0]);
        return 1;
    }
}
