#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "mytar.h"

extern char *use;

/** Copy nBytes bytes from the origin file to the destination file.
 *
 * origin: pointer to the FILE descriptor associated with the origin file
 * destination:  pointer to the FILE descriptor associated with the destination file
 * nBytes: number of bytes to copy
 *
 * Returns the number of bytes actually copied or -1 if an error occured.
 */
int
copynFile(FILE * origin, FILE * destination, int nBytes)
{
    char buf[F_BUFFER];
    // En la división redondeamos hacia arriba para hacer el "ultimo paso"
    // (Con el bufer medio lleno). También podemos usar (a+b-1)/b
	int steps = nBytes / F_BUFFER;
    int remainder = nBytes % F_BUFFER;
    int bytesread;
    for (int i = 0; i < steps; ++i) {
        if ((bytesread = fread(buf, sizeof(char), F_BUFFER, origin)) != F_BUFFER) {
            return EXIT_FAILURE;
        }

        fwrite(buf, sizeof(char), bytesread, destination);
    }

    if (remainder) {
        if ((bytesread = fread(buf, sizeof(char), remainder, origin)) != remainder) {
            return EXIT_FAILURE;
        }

        fwrite(buf, sizeof(char), remainder, destination);
    }

	return EXIT_SUCCESS;
}

int copyInternalFile(FILE * f, int nBytes, int offset) {
    char ** buf = NULL;
    int bufsize;
    int *bytesread, steps, sumbytesread = 0, i, nBuffs = 2, chunks;

    if (offset == 0) return EXIT_SUCCESS;

    nBuffs = 1 + ((offset > 0)?(offset/F_BUFFER + 1):0);

    bufsize = F_BUFFER;

    vprintf("ftellini: %lX (%d), nBytes: %X (%d), offset: %X (%d)\n", 
            ftell(f), ftell(f), nBytes, nBytes, offset, offset);

    buf = malloc(sizeof(char*) * nBuffs);
    bytesread = malloc(sizeof(int*) * nBuffs);
    for (i = 0; i < nBuffs; i++)
        buf[i] = malloc(sizeof(char) * bufsize);

    chunks = nBytes / bufsize + ((nBytes%bufsize > 0)?1:0);
    steps = chunks + ((offset > 0)?(offset/F_BUFFER + 1):0);
    // printf("nBuffs: %d, nBytes: %d, offset: %d, chunks: %d, steps: %d, bufsize: %d\n", 
    //         nBuffs, nBytes, offset, chunks, steps, bufsize);
    
    vprintf("nBuffs: %d, chunks: %d, steps: %d\n", nBuffs, chunks, steps);
    for (i = 0; i < steps; ++i) {
        if (i <= steps - nBuffs) {
            vprintf("ftell: 0x%08lX\n", ftell(f));
            if ((bytesread[i%nBuffs] = fread(buf[i%nBuffs], sizeof(char), 
                    (i == steps - nBuffs)?(nBytes%bufsize):bufsize, f)) == 0) {
                fprintf(stderr, "Error while reading file at chunk %d/%d (Byte %d)\n", i, chunks, i*nBytes);
                return EXIT_FAILURE;
            }
            sumbytesread += bytesread[i%nBuffs];
        }

        vprintf("br: %d\n", bytesread[i%nBuffs]);
        
        if (i >= nBuffs - 1) {
            fseek(f, offset-sumbytesread, SEEK_CUR);
            // En lugar de i - (nBuffs + 1), gracias al algebra modular podemos
            // poner i+1
            fwrite(buf[(i+1)%nBuffs], sizeof(char), bytesread[(i+1)%nBuffs], f);
            sumbytesread -= bytesread[(i+1)%nBuffs];
            fseek(f, sumbytesread-offset, SEEK_CUR);
        }
    }

    vprintf("ftellfin: %lX (%d)\n", ftell(f), ftell(f));

    for (i = 0; i < 2; i++)
        free(buf[i]);
    free(buf);
    free(bytesread);

    return EXIT_SUCCESS; 
}

/** Loads a string from a file.
 *
 * file: pointer to the FILE descriptor 
 * 
 * The loadstr() function must allocate memory from the heap to store 
 * the contents of the string read from the FILE. 
 * Once the string has been properly built in memory, the function returns
 * the starting address of the string (pointer returned by malloc()) 
 * 
 * Returns: !=NULL if success, NULL if error
 */
char*
loadstr(FILE * file)
{
    char * buff;
    char c;
    int i;

    i = 1;
    while ((c = fgetc(file)) != '\0') {
        i++;
    }

    fseek(file, -i, SEEK_CUR);
    buff = calloc(sizeof(char), i);
    i = 0;
    while((c = fgetc(file)) != '\0') {
        buff[i++] = c;
    }

	return buff; 
}

/** Read tarball header and store it in memory.
 *
 * tarFile: pointer to the tarball's FILE descriptor 
 * nFiles: output parameter. Used to return the number 
 * of files stored in the tarball archive (first 4 bytes of the header).
 *
 * On success it returns the starting memory address of an array that stores 
 * the (name,size) pairs read from the tar file. Upon failure, the function returns NULL.
 */
stHeaderEntry*
readHeaderandSize(FILE * tarFile, uint32_t *nFiles, size_t * headerSize)
{
	int i;
    stHeaderEntry * h = NULL;

    // Primero obtenemos nFiles
    if (fread(nFiles, sizeof(uint32_t), 1, tarFile) != 1) return NULL;
    *headerSize = sizeof(uint32_t) * (*nFiles + 1);

    h = malloc(sizeof(stHeaderEntry) * (*nFiles));

    for (i = 0; i < *nFiles; ++i) {
        // Leemos el nombre del fichero
        if ((h[i].name = loadstr(tarFile)) == NULL) {
            fprintf(stderr, "Failed to load filename %d\n", i);
        }

        // Añadimos el tamaño del string al tamaño del header
        *headerSize += strlen(h[i].name) + 1;

        // Leemos el tamaño del fichero
        if (!fread(&(h[i].size), sizeof(uint32_t), 1, tarFile)) {
            fprintf(stderr, "Failed to load header for file: %s\n", h[i].name);
            free(h);
            return NULL;
        }
    }

	return h;
}

stHeaderEntry* readHeader(FILE * tarFile, uint32_t *nFiles) {
    size_t _;
    return readHeaderandSize(tarFile, nFiles, &_);
}

/** Creates a tarball archive 
 *
 * nfiles: number of files to be stored in the tarball
 * filenames: array with the path names of the files to be included in the tarball
 * tarname: name of the tarball archive
 * 
 * On success, it returns EXIT_SUCCESS; upon error it returns EXIT_FAILURE. 
 * (macros defined in stdlib.h).
 *
 * HINTS: First reserve room in the file to store the tarball header.
 * Move the file's position indicator to the data section (skip the header)
 * and dump the contents of the source files (one by one) in the tarball archive. 
 * At the same time, build the representation of the tarball header in memory.
 * Finally, rewind the file's position indicator, write the number of files as well as 
 * the (file name,file size) pairs in the tar archive.
 *
 * Important reminder: to calculate the room needed for the header, a simple sizeof 
 * of stHeaderEntry will not work. Bear in mind that, on disk, file names found in (name,size) 
 * pairs occupy strlen(name)+1 bytes.
 *
 */
int
createTar(uint32_t nFiles, char *fileNames[], char tarName[])
{
    FILE * currentFile = NULL;
    FILE * tarFile = NULL;
    stHeaderEntry * header = NULL;
    int i, strSize;
    uint32_t fSize;  // Independiente del SO

    // 1 Abrimos el fichero mtar para escritura (fichero destino)
    // TODO: See man errno to select a proper exit error
    if ((tarFile = fopen(tarName, "wb")) == NULL) {
        fprintf(stderr, "Tarfile %s could not be opened\n", tarName);
        return EXIT_FAILURE;
    }

    // 2 Reservamos con malloc memoria para un array de stHeaderEntry
    header = malloc(sizeof(stHeaderEntry) * nFiles);

    // 3 Copiamos la cabecera al fichero mtar
    // Copiamos nfiles
    fwrite(&nFiles, sizeof(uint32_t), 1, tarFile);

    // Por cada fichero...
    for (i = 0; i < nFiles; ++i) {
        if ((currentFile = fopen(fileNames[i], "rb")) == NULL) {
            fprintf(stderr, "File %s could not be opened\n", fileNames[i]);
            return EXIT_FAILURE;
        }

        // Metemos el nombre de archivo en la cabecera
        strSize = strlen(fileNames[i]) + 1;
        header[i].name = malloc(sizeof(char) * strSize);
        strncpy(header[i].name, fileNames[i], strSize);
        fwrite(header[i].name, sizeof(char), strSize, tarFile);

        // Ahora el tamaño del fichero
        // También podríamos usar fstat
        // TODO: Return EFBIG when fSize > 2^32
        fseek(currentFile, 0, SEEK_END);
        fSize = ftell(currentFile);
        fseek(currentFile, 0, SEEK_SET);

        header[i].size = fSize; 
        fwrite(&fSize, sizeof(fSize), 1, tarFile);

        fclose(currentFile);
    }

    // TODO: Arreglar memory leak. Cuando retornamos en mitad del bucle
    // no liberamos la memoria ocupada por header
    for (i = 0; i < nFiles; ++i) {
        if ((currentFile = fopen(fileNames[i], "rb")) == NULL) {
            fprintf(stderr, "File %s could not be opened\n", fileNames[i]);
            return EXIT_FAILURE;
        }

        if (copynFile(currentFile, tarFile, header[i].size) != EXIT_SUCCESS) {
            fprintf(stderr, "Could not copy file %s\n", fileNames[i]);
            return EXIT_FAILURE;
        }

        free(header[i].name);

        fclose(currentFile);
    }

    fclose(tarFile);
    free(header);

	return EXIT_SUCCESS;
}

/** Extract files stored in a tarball archive
 *
 * tarName: tarball's pathname
 *
 * On success, it returns EXIT_SUCCESS; upon error it returns EXIT_FAILURE. 
 * (macros defined in stdlib.h).
 *
 * HINTS: First load the tarball's header into memory.
 * After reading the header, the file position indicator will be located at the 
 * tarball's data section. By using information from the 
 * header --number of files and (file name, file size) pairs--, extract files 
 * stored in the data section of the tarball.
 *
 */
int extractTar(char tarName[]){
    int i;
    uint32_t numFiles;
    stHeaderEntry * header;
    FILE * tarFile;
    FILE * currentFile;

    // Abrimos el fichero tar para lectura
    if ((tarFile = fopen(tarName, "rb")) == NULL) {
        fprintf(stderr, "Tarfile %s could not be opened\n", tarName);
        return EXIT_FAILURE;
    }

    // Leemos la cabecera del fichero
    header = readHeader(tarFile, &numFiles);

    // Y ahora vamos copiando los ficheros
    for (i = 0; i < numFiles; ++i) {
        if ((currentFile = fopen(header[i].name, "wb")) == NULL) {
            fprintf(stderr, "File %s could not be created\n", header[i].name);
            return EXIT_FAILURE;
        }

        printf("[%d]: Creando fichero %s, tamaño %d Bytes...", i, header[i].name, header[i].size);
        if (copynFile(tarFile, currentFile, header[i].size) == -1) {
            printf("Failed\n");
            fprintf(stderr, "File %s could not be extracted\n", header[i].name);
            return EXIT_FAILURE;
        }

        printf("OK\n");

        fclose(currentFile);
    }

    fclose(tarFile);
    for (i = 0; i < numFiles; ++i) free(header[i].name);
    free(header);

 return EXIT_SUCCESS;
}

int listTar (char tarName[]){//Argumento la ruta del fichero .mtar y muestra los ficheros contenidos (nombre y tamaño) devuelve un 0 si funciona -l de opcion al main
 int i;
    uint32_t numFiles;
    stHeaderEntry * header;
    FILE * tarFile;
    // Abrimos el fichero tar para lectura
    if ((tarFile = fopen(tarName, "rb")) == NULL) {
        fprintf(stderr, "Tarfile %s could not be opened\n", tarName);
        return EXIT_FAILURE;
    }
    // Leemos la cabecera del fichero
    header = readHeader(tarFile, &numFiles);
    for (i=0;i < numFiles; ++i){
	printf("Fichero: %s, Tam: %d Bytes \n", header[i].name, header[i].size);	
    }


    fclose(tarFile);
    for (i = 0; i < numFiles; ++i) free(header[i].name);
    free(header);
    return EXIT_SUCCESS;
}

// Se añade un nuevo fichero a un mtar existente con la opción -a
int appendTar(uint32_t nFiles, char *fileNames[], char tarName[]) {
    // ¿Como???
    // Opción A: Preguntar profe
    // Opción B: 
    // (Como esto no es Win2, podemos hacer cosas chulas...)
    // Calcular el tamaño de la nueva cabecera y desplazar todos los datos del
    // fichero N bytes a la derecha
    // Meter la info del fichero en ese espacio
    // Copiar el fichero al final
    stHeaderEntry * h;
    size_t prevHeaderSize, appendHeaderSize;
    int i, bytestocopy, startwritting = 0;
    uint32_t prevnFiles, newnFiles;
    uint32_t * filesizes;
    FILE * tarFile = NULL, *currentFile = NULL;

    // Abrimos el fichero para lectura y escritura con r+
    if ((tarFile = fopen(tarName, "rb+")) == NULL) {
        fprintf(stderr, "Tarfile %s could not be opened\n", tarName);
        return EXIT_FAILURE;
    }

    // Calculamos cuantos bytes hay que mover
    appendHeaderSize = sizeof(uint32_t) * nFiles;
    for (i = 0; i < nFiles; ++i) {
        appendHeaderSize += strlen(fileNames[i]) + 1;
    }

    // Leemos la cabecera anterior
    h = readHeaderandSize(tarFile, &prevnFiles, &prevHeaderSize);

    // Escribimos el nuevo numero de ficheros
    fseek(tarFile, 0, SEEK_SET);
    newnFiles = prevnFiles + nFiles; 
    fwrite(&newnFiles, sizeof(uint32_t), 1, tarFile);

    // Calculamos el n de bytes a copiar
    fseek(tarFile, 0, SEEK_END);
    bytestocopy = ftell(tarFile) - prevHeaderSize;

    // Y ahora hacemos el Moisés
    fseek(tarFile, prevHeaderSize, SEEK_SET);

    if (copyInternalFile(tarFile, bytestocopy, appendHeaderSize)) {
        fprintf(stderr, "Error moving internal file\n");
        return EXIT_FAILURE;
    }

    // Rellenamos el hueco con la cabecera
    fseek(tarFile, prevHeaderSize, SEEK_SET);
    filesizes = malloc(sizeof(uint32_t) * nFiles);
    for (i = 0; i < nFiles; ++i) {
        if ( (currentFile = fopen(fileNames[i], "rb")) == NULL) {
            fprintf(stderr, "Error while opening file %s\n", fileNames[i]);
            return EXIT_FAILURE;
        }

        fseek(currentFile, 0, SEEK_END);
        filesizes[i] = ftell(currentFile);
        fclose(currentFile);
    }

    for (i = 0; i < nFiles; ++i) {
        fwrite(fileNames[i], sizeof(char), strlen(fileNames[i]) + 1, tarFile);
        fwrite(&filesizes[i], sizeof(uint32_t), 1, tarFile);
    }

    // Y ahora, al final, metemos nuestros fichero
    startwritting = prevHeaderSize + appendHeaderSize;
    for (i = 0; i < prevnFiles; ++i)
        startwritting += h[i].size;

    fseek(tarFile, startwritting, SEEK_SET);
    for (i = 0; i < nFiles; ++i) {
        if ( (currentFile = fopen(fileNames[i], "rb")) == NULL) {
            fprintf(stderr, "Error while opening file %s\n", fileNames[i]);
            return EXIT_FAILURE;
        }

        copynFile(currentFile, tarFile, filesizes[i]);

        fclose(currentFile);
    }

    free(filesizes);
    for (i = 0; i < prevnFiles; ++i) free(h[i].name);
    free(h);
    fclose(tarFile);

    return EXIT_SUCCESS;
}

// Quitamos un fichero de un mtar existente con la opción -r
int removeTar(uint32_t nFiles, char *fileNames[], char tarName[]) {
    // ¿Como??
    // Modificamos la cabecera
    // Re-escribimos la cabecera
    // Movemos los archivos "hacia la izquierda" para rellenar el hueco dejado
    // en la cabecera, pero nos saltamos el fichero que queremos borrar
    // (Hacemos fseek hasta el principio, y entonces llamamos a copyInternalFile
    // con offset -nBytesFicheroaborrar)
    int i, headeroffset, rmfsize, startwritting = 0, remaining = 0, j;
	uint32_t numFiles;
    stHeaderEntry * header;
	FILE * tarFile;

    if (nFiles != 1) {
        fprintf(stderr, "Solo se puede borrar un fichero\n");
        return EXIT_FAILURE;
    }

	// Abrimos el fichero tar para lectura
	if ((tarFile = fopen(tarName, "rb+")) == NULL) {
	   fprintf(stderr, "Tarfile %s could not be opened\n", tarName);
	   return EXIT_FAILURE;
	}
	// Leemos la cabecera del fichero
	header = readHeader(tarFile, &numFiles);
    i = 0;
    startwritting = 0;
    while (i < numFiles && strcmp(header[i].name, fileNames[0]) != 0) {
        startwritting += header[i].size;
        ++i;
    }

    if (i == numFiles) {
        fprintf(stderr, "El fichero %s no está en el tarball %s\n", fileNames[0], tarName);
        return EXIT_FAILURE;
    } else {
        for (j = i + 1, remaining = 0; j < numFiles; ++j) {
            remaining += header[j].size;
        }

        headeroffset = sizeof(uint32_t) + strlen(header[i].name) + 1;
        free(header[i].name);
        rmfsize = header[i].size;
        for (;i < numFiles - 1; ++i) {
            header[i] = header[i+1];
        }
        numFiles--;
    }

    // Volvemos a escribir la cabecera
    rewind(tarFile);
    fwrite(&numFiles, sizeof(uint32_t), 1, tarFile);
    for (i = 0; i < numFiles; i++) {
        fwrite(header[i].name, sizeof(char), strlen(header[i].name) + 1, tarFile);
        fwrite(&(header[i].size), sizeof(uint32_t), 1, tarFile);
    }

    // Movemos los ficheros anteriores al que borramos
    fseek(tarFile, headeroffset, SEEK_CUR);
    copyInternalFile(tarFile, startwritting,-headeroffset);

    printf("ftell: %lX (%ld)\n", ftell(tarFile), ftell(tarFile));
    printf("rmfsize: %d\n", rmfsize);
    printf("start: %X (%d)\n", startwritting, startwritting);
    printf("remaining: %d\n", remaining);
    printf("headeroffset: %d\n", headeroffset);
    // Movemos los ficheros de después
    i = fseek(tarFile, headeroffset + rmfsize, SEEK_CUR);
    printf("fseek result: %d\n", i);
    printf("ftell: %lX\n", ftell(tarFile));
    i = copyInternalFile(tarFile, remaining, -headeroffset-rmfsize);
    printf("%d\n", i);

    fclose(tarFile);

    return EXIT_SUCCESS;
}
