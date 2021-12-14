#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#define N 10
typedef struct output_data {
	long id;
} output_data;

typedef struct input_data {
	int number;
	output_data * output;
} input_data;


void *func (void*_data) {
	input_data* data = (input_data *)_data;
	long id = pthread_self();
	printf("tread's data %lu  %d\n", id, data -> number);
	pthread_exit((void *)id);
}

int main() {
	pthread_t thid[N];
	input_data input[N];
	output_data output[N];
	for (int i = 0; i < N; i++) {
		input[i].number = i;
		input[i].output = &output[i];
		if (pthread_create(&thid[i], NULL, func, (void*)(&input[i])) != 0) {
			perror ("create error");
			return errno;
		}
	}
	for (int j = 0; j < N; j++) {
		long id;
		if (pthread_join(thid[j], (void**)(&id)) != 0) {
			perror ("join fail");
			return errno;
		}
		//printf ("Master data %lu  %d\n", id, j);
	}
	


	return 0;
}
