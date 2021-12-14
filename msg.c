#include <stdio.h>
#include <stdlib.h>
#include <sys/msg.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#define N 3

int msgid = -1;

typedef struct input_data {
    	int number;
} input_data;

struct mbuf {
	long mtype;
};

void *func (void*_data) {
        input_data* data = (input_data *)_data;
	data->number++;
	struct mbuf buf;
	if (msgrcv(msgid, &buf, 0, data->number, 0) < 0) {
		perror("msgrcv 1 error");
		exit(errno);
	}
	sleep(rand() % N);
        printf("tread's data %d\n", data->number);
	buf.mtype = data->number + 1;
	if (msgsnd(msgid, &buf, 0, 0) < 0) {
		perror ("msgsnd 1 error");
		exit(errno);
	}
        pthread_exit(NULL);

}

int main() {
	struct mbuf buf;
        pthread_t thid[N];
        input_data input[N];

	if ((msgid = msgget(IPC_PRIVATE, IPC_CREAT | 0666)) < 0) {
		perror ("msgget error");
		return errno;
	}

	for (int i = 0; i < N; i++) {
               input[i].number = i;
               if (pthread_create(&thid[i], NULL, func, (void *)(&input[i])) != 0) {
                       perror ("create error");
                       return errno;
               }
        }
	buf.mtype = 1;
	if (msgsnd(msgid, &buf, 0, 0) < 0) {
		perror ("msgsnd 2 error");
		return errno;
	}
	if (msgrcv(msgid, &buf, 0, N + 1, 0) < 0) {
		perror ("msgrcv 2 error");
		return errno;
	}

	for (int j = 0; j < N; j++) {
		if (pthread_join(thid[j], NULL) != 0) {
			perror ("join failed");
			return errno;
		}
	}

	return 0;
}
