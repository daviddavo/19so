#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <time.h>

#define MORSE_UNIT 200
char * ALL_0 = "\0";
char * ALL_1 = "123\0";
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

int msleep(unsigned int miliseconds) {
    int ret;
    struct timespec t;

    t.tv_sec = miliseconds/1000;
    t.tv_nsec = (miliseconds%1000)*1000000;

    do {
        ret = nanosleep(&t, &t);
    } while(ret);

    return ret;
}

int seq_morse(int argc, char * argv[]) {
    // https://www.itu.int/rec/R-REC-M.1677-1-200910-I/
    char * msg;
    int i, mi;
    char c, mc; // mc = morsechar
    char * ms;  // ms = morsestring
    FILE * dev;

    if (argc < 3) {
        // size hello world = 11 (+1 with \0)
        msg = calloc(sizeof(char), 11+1);
        strcpy(msg, "Hello World\0");
    } else {
        msg = argv[2];
    }

    printf("Trying to display %s in morse code\n", msg);

    if ((dev = fopen("/dev/modleds", "w")) == NULL) {
        fprintf(stderr, "Can't open device /dev/modleds");
        return EXIT_FAILURE;
    }

    for (c = msg[0], i = 0; c != '\0'; c = msg[++i]) {
        if (isalpha(c)) {
            ms = MORSE_TABLE[toupper(c) - 'A'];
            for (mc = ms[0], mi = 0; mc != '\0'; mc = ms[++mi]) {
                fwrite(ALL_1, sizeof(char), 4, dev);
                fflush(dev);
                printf("%c", mc);
                fflush(stdout);
                msleep(MORSE_UNIT * ((mc=='-')?3:1));
                fwrite(ALL_0, sizeof(char), 1, dev);
                fflush(dev);
                msleep(MORSE_UNIT);
            }

            msleep(3*MORSE_UNIT);
            printf(" (%c)\n", toupper(c));
        } else {
            msleep(7*MORSE_UNIT);
            printf(" ( )\n");
        }
    }

    fclose(dev);
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
