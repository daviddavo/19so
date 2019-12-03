// David Cantador Piedras ***REMOVED***
// David Davó Laviña ***REMOVED***

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int system(const char * command) {
    int pid;
    int ret;

    if( (pid = fork()) == -1) {
        fprintf(stderr, "Error forking\n");
    } else if (pid == 0) {
        char * const parmList[] = {command, NULL};
        execvp(command, parmList);
        // These two lines should not execute, so if they are running, we have
        // a problem
        fprintf(stderr, "Error executing %s\n", command);
        exit(127);
    }

    while (wait(&ret) != -1);

    return ret;
}

int main(int argc, char* argv[])
{
	if (argc!=2){
		fprintf(stderr, "Usage: %s <command>\n", argv[0]);
		exit(1);
	}

	return system(argv[1]);
}

