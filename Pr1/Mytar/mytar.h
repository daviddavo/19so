#ifndef _MYTAR_H
#define _MYTAR_H

#include <limits.h>
#include <stdint.h>

#ifndef F_BUFFER
#define F_BUFFER 4096 
#endif

#define VERBOSE

typedef enum{
  NONE,
  ERROR,
  CREATE,
  EXTRACT,
  LIST,
  APPEND,
  REMOVE
} flags;

typedef enum {
    DEFAULT,
    DEBUG
} verbosity;

typedef struct {
  char* name;
  uint32_t size;
} stHeaderEntry;

void setVerbosity(verbosity v);
void debug(const char *fmt, ...);
int createTar(uint32_t nFiles, char *fileNames[], char tarName[]);
int extractTar(char tarName[]);
int listTar (char tarName[]);
int appendTar(uint32_t nFiles, char *fileNames[], char tarName[]);
int removeTar(uint32_t nFiles, char *fileNames[], char tarName[]);

#endif /* _MYTAR_H */
