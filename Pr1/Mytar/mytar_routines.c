#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
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
    int *bytesread, steps, sumbytesread = 0, i, nBuffs = 2;

    printf("nBytes: %d, Offset %d\n", nBytes, offset);
    printf("ftell: %ld\n", ftell(f));
    if (offset < 0 || offset >= F_BUFFER) {
        fprintf(stderr, "Offset must be below 0 and %d\n", F_BUFFER);
        return EXIT_FAILURE;
    }

    bufsize = F_BUFFER;

    buf = malloc(sizeof(char*) * nBuffs);
    bytesread = malloc(sizeof(char*) * nBuffs);
    for (i = 0; i < nBuffs; i++)
        buf[i] = malloc(sizeof(char) * bufsize);

    steps = nBytes / bufsize + ((nBytes%bufsize > 0)?1:0) + 1;
    printf("s: %d, bufsize: %d, %d\n", steps, bufsize, nBytes/bufsize);
    
    for (i = 0; i <= steps; ++i) {
        printf("i: %d, s: %d\n", i, steps);
        if (i < steps) {
            if ((bytesread[i%nBuffs] = fread(buf[i%nBuffs], sizeof(char), bufsize, f)) == 0) {
                fprintf(stderr, "Error while reading file at chunk %d/%d (Byte %d)\n", i, steps, i*nBytes/steps);
                return EXIT_FAILURE;
            }
            sumbytesread += bytesread[i%nBuffs];
        }
        
        if (i >= nBuffs - 1) {
            fseek(f, offset-sumbytesread, SEEK_CUR);
            fwrite(buf[(i-1)%nBuffs], sizeof(char), bytesread[(i-1)%nBuffs], f);
            sumbytesread -= bytesread[(i-1)%nBuffs];
            fseek(f, sumbytesread-offset, SEEK_CUR);
        }
    }

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

    buff = malloc(sizeof(char) * NAME_MAX); // Defined in limit.h
    i = 0;
    while ((c = fgetc(file)) != '\0') {
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

        if (copynFile(currentFile, tarFile, header[i].size) == -1) {
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
    size_t prevHeaderSize, appendHeaderSize;
    int i, bytestocopy;
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
    readHeaderandSize(tarFile, &prevnFiles, &prevHeaderSize);

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
    // TODO: ¿Why -3?
    fseek(tarFile, -3, SEEK_END);
    for (i = 0; i < nFiles; ++i) {
        if ( (currentFile = fopen(fileNames[i], "rb")) == NULL) {
            fprintf(stderr, "Error while opening file %s\n", fileNames[i]);
            return EXIT_FAILURE;
        }

        copynFile(currentFile, tarFile, filesizes[i]);

        fclose(currentFile);
    }

    free(filesizes);
    fclose(tarFile);

    return EXIT_SUCCESS;
}

// Quitamos un fichero de un mtar existente con la opción -r
int removeTar(uint32_t nFiles, char *fileNames[], char tarName[]) {
    // ¿Como??
    // ¿Abrimos el fichero dos veces?
    // Modificamos la cabecera
    // Re-escribimos la cabecera
    // Movemos los archivos "hacia la izquierda" para rellenar el hueco dejado
    // en la cabecera, pero nos saltamos el fichero que hemos borrado

    return EXIT_FAILURE;
}
