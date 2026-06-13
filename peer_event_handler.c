#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "peer.h"
#include "peer_event_handler.h"

#define BUFFER_SIZE 1024

static int connected_to_peer(struct peer_context* ctx) {
	(void)ctx;

	printf("New peer connected successfully!\n");

	return 0;
}

static int received_from_peer(struct peer_context* ctx) {

	/* Null-terminate the buffer safely to print as a string */
	if (ctx->len_read < BUFFER_SIZE) {
		ctx->buffer[ctx->len_read] = '\0';
	} else {
		ctx->buffer[BUFFER_SIZE - 1] = '\0';
	}

	printf("--- [Data Received] ---\n");
	printf("Payload: %s\n", ctx->buffer);
	printf("Bytes read: %zu\n", ctx->len_read);
	printf("-----------------------\n");

	return 0;
}

static struct conn_handlers s_handlers = { 
	.on_init_conn = connected_to_peer,
	.on_read_done = received_from_peer,
};

struct conn_handlers* get_handlers(void) {

	return &s_handlers;
}

