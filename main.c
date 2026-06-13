#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <ccan/io/io.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include "peer.h"
#include "peer_event_handler.h"

/* 2. Data Read Callback (Called when data arrives from the peer) */
static struct io_plan *read_done(struct io_conn *conn, struct peer_context *ctx) {

	ctx->hdl->on_read_done(ctx);

	/* Register the next read plan to continue the async read loop */
	return io_read_partial(conn, ctx->buffer, BUFFER_SIZE, &ctx->len_read, read_done, ctx);
}

/* 1. Connection Initialization Callback */
static struct io_plan *init_conn(struct io_conn *conn, void *ctx_arg) {

	/* Allocate memory context for this specific peer using ccan/tal.
	 * Binding it to 'conn' ensures it auto-frees when the peer disconnects. */
	struct peer_context *ctx = tal(conn, struct peer_context);
	memset(ctx, 0, sizeof(*ctx));
	ctx->hdl = ctx_arg;
	ctx->hdl->on_init_conn(ctx);

	/* Set up the initial async read plan */
	return io_read_partial(conn, ctx->buffer, BUFFER_SIZE, &ctx->len_read, read_done, ctx);
}

static void* beacon_event_loop_thread(void *arg) {
	(void)arg;

	int server_fd;
	struct sockaddr_in addr;
	struct io_listener *listener;

	printf("Starting Beacon Light Daemon...\n");

	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd < 0) {
		perror("Failed to create socket");
		return NULL;
	}

	int opt = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
		perror("setsockopt failed");
		close(server_fd);
		return NULL;
	}

	/* Set up address flags for standard port 9735 */
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(9735);

	if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("Bind failed");
		close(server_fd);
		return NULL;
	}

	if (listen(server_fd, 64) < 0) {
		perror("Listen failed");
		close(server_fd);
		return NULL;
	}

	/* Pass ownership of the server_fd to ccan/io */
	struct conn_handlers* hdls = get_handlers();
	listener = io_new_listener(NULL, server_fd, init_conn, hdls);
	if (!listener) {
		fprintf(stderr, "Failed to create ccan/io listener\n");
		close(server_fd);
		return NULL;
	}

	printf("Listening on port 9735...\n");

	/* Start the non-blocking event multiplexer loop */
	io_loop(NULL, NULL);

	io_close_listener(listener);

	printf("Shutting down ebeacon node.\n");
	return NULL;
}

int main(void) {

	pthread_t io_thread;
	if (pthread_create(&io_thread, NULL, beacon_event_loop_thread, NULL) != 0) {
		perror("Failed to create thread");
		return 1;
	}

	int result;
	pthread_join(io_thread, (void*)&result);

	printf("Result: %d\n", result);

	return 0;
}


