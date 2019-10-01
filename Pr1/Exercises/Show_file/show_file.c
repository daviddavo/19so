//David Cantador Piedras 51120722w
//David Davó Laviña 02581158Y
#include <stdio.h>
#include <stdlib.h>
#include <err.h>

#define BUFF_SIZE 512

int main(int argc, char* argv[]) {
	FILE* file=NULL;
	char buff[BUFF_SIZE];
	int c;

	if (argc!=2) {
		fprintf(stderr,"Usage: %s <file_name>\n",argv[0]);
		exit(1);
	}

	/* Open file */
	if ((file = fopen(argv[1], "r")) == NULL)
		err(2,"The input file %s could not be opened",argv[1]);

	/* Read file byte by byte */
	// while ((c = getc(file)) != EOF) {
	while ((c = fread(buff, sizeof(char), BUFF_SIZE, file))) {
		/* Print byte to stdout */
		if (fwrite(buff, sizeof(char), c, stdout) != c) {
			fclose(file);
			err(3, "putc() failed!!");
		}
	}

	fclose(file);
	return 0;
}
