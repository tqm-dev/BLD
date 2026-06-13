#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include "peer_event_loop.h"

int main(void) {

	pthread_t io_thread;
	if (pthread_create(&io_thread, NULL, peer_event_loop_main, NULL) != 0) {
		perror("Failed to create thread");
		return 1;
	}

	int result;
	pthread_join(io_thread, (void*)&result);

	printf("Result: %d\n", result);

	return 0;
}


