#ifndef _MYTAR_H
#define _MYTAR_H

#include <limits.h>
#include <stdint.h>

#ifndef F_BUFFER
#define F_BUFFER 2048 
#endif

#define VERBOSE

#ifdef VERBOSE
#define vprintf(...) printf(__VA_ARGS__)
#endif

typedef enum{
  NONE,
  ERROR,
  CREATE,
  EXTRACT,
  LIST,
  APPEND,
  REMOVE
} flags;

typedef struct {
  char* name;
  uint32_t size;
} stHeaderEntry;

int createTar(uint32_t nFiles, char *fileNames[], char tarName[]);
int extractTar(char tarName[]);
int listTar (char tarName[]);
int appendTar(uint32_t nFiles, char *fileNames[], char tarName[]);
int removeTar(uint32_t nFiles, char *fileNames[], char tarName[]);

#endif /* _MYTAR_H */
