#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

char MORSE_TABLE[][6] = {
    ".-",       // A
    "-...",     // B
    "-.-.",     // C
    "-..",      // D
    ".",        // E
    "..-.",     // F
    "--.",      // G
    "....",     // H
    "..",       // I
    ".---",     // J
    "-.-",      // K
    ".-..",     // L
    "--",       // M
    "-.",       // N
    "---",      // O
    ".--.",     // P
    "--.-",     // Q
    ".-.",      // R
    "...",      // S
    "-",        // T
    "..-",      // U
    "...-",     // V
    ".--",      // W
    "-..-",     // X
    "-.--",     // Y
    "--..",     // Z
};

int seq_morse(int argc, char * argv[]) {
    char * msg;
    int i, mi;
    char c, mc; // mc = morsechar
    char * ms;  // ms = morsestring

    if (argc < 3) {
        // size hello world = 11 (+1 with \0)
        msg = calloc(sizeof(char), 11+1);
        strcpy(msg, "Hello World\0");
    } else {
        msg = argv[2];
    }

    printf("Trying to display %s in morse code\n", msg);

    for (c = msg[0], i = 0; c != '\0'; c = msg[++i]) {
        ms = MORSE_TABLE[toupper(c) - 'A'];
        for (mc = ms[0], mi = 0; mc != '\0'; mc = ms[++mi]) {
            printf("%c", mc);
        }

        printf("(%c)\n", toupper(c));
    }

    printf("\n");

    if (argc < 3) {
        free(msg);
    }

    return 0;
}

int main(int argc, char * argv[]) {
    int seq = -1;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s sequence_number\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    seq = atoi(argv[1]);

    switch (seq) {
        case 0: return seq_morse(argc, argv);
        default:
            fprintf(stderr, "Available sequences are:\n");
            fprintf(stderr, "0 [msg] - Message in morse code\n");
            exit(EXIT_FAILURE);
    }

    return 0;
}
