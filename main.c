#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include "peer_event_loop.h"

int main(void) {

	printf("Starting Beacon Light Daemon...\n");

	pthread_t io_read_thread;
	if (pthread_create(&io_read_thread, NULL, peer_io_loop, NULL) != 0) {
		perror("Failed to create thread");
		return 1;
	}

	int result;
	pthread_join(io_read_thread, (void*)&result);

	printf("Result: %d\n", result);

	printf("Stopped Beacon Light Daemon.\n");

	return 0;
}


