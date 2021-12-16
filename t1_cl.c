#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>

#define SIZE 4096
#define SERVER "./fifo.serv"

int main(int argc, char **argv, char **envp) {
    int fdin, fdout, rd = -1;
    pid_t pid;
	int len_pid = 0;
    char *CLIENT, *req, DEAD[] = "stop";

	//enough arguments
    if (argc != 2) {
        printf ("Error, incorrect number of arguments\n");
        return 0;
    }
    int len_file = strlen(argv[1]);

    if ((pid = getpid()) < 0) {
        perror ("getpid failed");
        return errno;
    }
	
	//pid length
    for (pid_t t = pid; t > 0; len_pid++) {
        t /= 10;
    }

    if ((CLIENT = malloc(len_pid + 7)) == NULL) {
        fprintf (stderr, "ERROR");
        return -1;
    }
    sprintf(CLIENT, "./fifo%d", pid);

	// generate request
    if ((req = malloc(len_pid + len_file + 8)) == NULL) {
        fprintf (stderr, "ERROR");
        return -1;
    }
    sprintf(req, "%s %s", CLIENT, argv[1]);

    umask(0);
	//create FIFO for input
    if (mkfifo(CLIENT, 0600) < 0) {
        perror ("client mkfifo failed");
        return errno;
    }

    if ((fdout = open(SERVER, O_WRONLY)) < 0) {
        perror ("open server FIFO failed");
        return errno;
    }
    if (write (fdout, req, SIZE) < 0) {
        perror ("request write failed");
        return errno;
    }
    close (fdout);

    if (strcmp(argv[1], DEAD) == 0) {
        return 0;
    }

    void *buf = NULL;
	
	//answer from server
    if ((buf = malloc(SIZE)) == NULL) {
        fprintf (stderr, "ERROR");
        return -1;
    }
    if ((fdin = open(CLIENT, O_RDONLY)) < 0) {
        perror ("open client FIFO failed");
        return errno;
    }
    do {
        if ((rd = read(fdin, buf, SIZE)) < 0) {
            perror ("FIFO read failed");
            return errno;
        }
        if (rd == 0) 
			break;
        if (write (1, buf, rd) < 0) {
            perror("out write failed");
            return errno;
        }
    } while (rd != 0);
	
    close(fdin);
    unlink(CLIENT);
    free(buf);

    return 0;
}