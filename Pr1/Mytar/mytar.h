#ifndef _MYTAR_H
#define _MYTAR_H

#include <limits.h>
#include <stdint.h>

#ifndef F_BUFFER
#define F_BUFFER 1024
#endif

typedef enum{
  NONE,
  ERROR,
  CREATE,
  EXTRACT,
  LIST
} flags;

typedef struct {
  char* name;
  uint32_t size;
} stHeaderEntry;

int createTar(uint32_t nFiles, char *fileNames[], char tarName[]);
int extractTar(char tarName[]);


#endif /* _MYTAR_H */
