#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <signal.h> 
#include <time.h>

#define MORSE_UNIT 100
#define CPU_DIFF 1000
char * ALL_0 = "\0";
char * ALL_1 = "123\0";
static int running = 1;
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

int numToBinMask(int num, char * mask) {
    int i;

    if (num > 15 || num < 0) {
        return -1;
    }

    for (i = 0; i < 3; ++i) {
        if (num & (0b1 << i)) {
            mask[i] = i + '0' + 1;
        } else {
            mask[i] = ' ';
        }
    }
    mask[3] = '\0';

    return 0;
}

void intHandler(int signum) {
    running = 0;
}

int seq_battery(int argc, char * argv[]) {
    FILE * bat;
    FILE * modleds;
    char capacityStr[4];
    char mask[4];
    int capacity;
    int blink = 0;

    if ((bat = fopen("/sys/class/power_supply/BAT0/capacity", "r")) == NULL) {
    // if ((bat = fopen("capacity", "r")) == NULL) {
        fprintf(stderr, "Error 404: Battery not found\n");
        return EXIT_FAILURE;
    }

    if ((modleds = fopen("/dev/modleds", "w")) == NULL) {
        fprintf(stderr, "Can't open device /dev/modleds");
        return EXIT_FAILURE;
    }

    printf("Displaying battery level in binary little endian\n");
    signal(SIGINT, intHandler);
    running = 1;
    while (running) {
        fread(&capacityStr, sizeof(char), 4, bat);
        capacity = atoi(capacityStr);
        printf("Capacity: %3d, c/8: %2d\r", capacity, (capacity-1)*8/100);
        fflush(stdout);
        if (capacity < 13) {
            /* Parpadeo */
            numToBinMask((blink >= 1)?7:0, mask);
            if (blink++ >= 2) blink = 0;
        } else {
            numToBinMask((capacity-1)*8/100, mask);
        }

        fwrite(&mask, sizeof(char), 4, modleds);
        fflush(modleds);

        rewind(bat);
        fflush(bat);
        msleep(500);
    }
    printf("\n");

    fclose(modleds);
    fclose(bat);

    return EXIT_SUCCESS;
}

int numToPctMask(int num, char * mask) {
    if (num < 0 || num > 4)
        return EXIT_FAILURE;
    for (int i = 0; i < 3; ++i)
        mask[i] = ' ';

    if (num >= 1) mask[0] = '1';
    if (num >= 2) mask[1] = '2';
    if (num >= 3) mask[2] = '3';

    mask[3] = '\0';

    return EXIT_SUCCESS;
}

int pctToNum(float num) {
    if (num > 85) return 3;
    if (num > 50) return 2;
    if (num > 10) return 1;
    return 0;
}

int seq_cpu(int argc, char * argv[]) {
    FILE * modleds;
    FILE * uptime;
    char mask[4];
    int ncores = 8; /* TODO: Get number of codes */
    float lastidle = 0;
    float using = 0, idle = 0;
    float pct;
    
    if ((uptime = fopen("/proc/uptime", "r")) == NULL) {
        fprintf(stderr, "Can't open /proc/uptime\n");
        return EXIT_FAILURE;
    }

    if ((modleds = fopen("/dev/modleds", "w")) == NULL) {
        fprintf(stderr, "Can't open device /dev/modleds");
        return EXIT_FAILURE;
    }

    printf("Displaying cpu usage as number of leds lighted up\n");
    signal(SIGINT, intHandler);
    running = 1;

    fscanf(uptime, "%f %f\n", &using, &idle);
    msleep(CPU_DIFF);
    while (running) {
        lastidle = idle;

        fscanf(uptime, "%f %f\n", &using, &idle);
        fflush(uptime);
        rewind(uptime);

        pct = 100 - (idle-lastidle)/ncores*100;
        printf("CPU Usage: %3.0f%% (%2d)\r", pct, pctToNum(pct));
        fflush(stdout);

        numToPctMask(pctToNum(pct), mask);
        fwrite(&mask, sizeof(char), 4, modleds);
        fflush(modleds);

        msleep(CPU_DIFF);
    }
    printf("\n");

    fclose(modleds);
    fclose(uptime);

    return EXIT_SUCCESS;
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
        case 1: return seq_battery(argc, argv);
        case 2: return seq_cpu(argc, argv);
        default:
            fprintf(stderr, "Available sequences are:\n");
            fprintf(stderr, "0 [msg] - Message in morse code\n");
            fprintf(stderr, "1 - Display battery level\n");
            fprintf(stderr, "2 - Display CPU usage\n");
            exit(EXIT_FAILURE);
    }

    return 0;
}
