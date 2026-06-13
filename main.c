#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include "peer_event_loop.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <ccan/io/io.h>
#include <sys/eventfd.h>
#include <string.h>
#include "peer.h"
#include "peer_event_handler.h"

static void push_node_request(
    struct request_queue_ctx *ctx,
    request_type_t type,
    const char *ip,
    uint16_t port
) {
	struct node_request *req = malloc(sizeof(struct node_request));
	req->type = type;
	if (ip) {
		strncpy(req->ip_address, ip, sizeof(req->ip_address) - 1);
	}
	req->port = port;
	req->next = NULL;

	pthread_mutex_lock(&ctx->mutex);
	if (!ctx->tail) {
		ctx->head = req;
		ctx->tail = req;
	} else {
		ctx->tail->next = req;
		ctx->tail = req;
	}
	pthread_mutex_unlock(&ctx->mutex);

	uint64_t signal_val = 1;
	if (write(ctx->ev_fd, &signal_val, sizeof(signal_val)) < 0) {
		perror("[Queue] Failed to write eventfd");
	}
}

int main(void) {

	printf("Starting Beacon Light Daemon...\n");

	struct request_queue_ctx q_ctx;
	pthread_mutex_init(&q_ctx.mutex, NULL);
	q_ctx.head = NULL;
	q_ctx.tail = NULL;
	q_ctx.ev_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
	if (q_ctx.ev_fd < 0) {
		perror("Failed to create eventfd");
		return 1;
	}

	pthread_t io_read_thread;
	if (pthread_create(&io_read_thread, NULL, peer_io_loop, &q_ctx) != 0) {
		perror("Failed to create thread");
		return 1;
	}

	usleep(1000000);

	// for test
	push_node_request(&q_ctx, REQ_CONNECT, "127.0.0.1", 9735);

	int result;
	pthread_join(io_read_thread, (void*)&result);

	printf("Result: %d\n", result);

	printf("Stopped Beacon Light Daemon.\n");

	return 0;
}


