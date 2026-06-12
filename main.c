#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <ccan/io/io.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>

#define BUFFER_SIZE 1024

/* Context structure allocated per connected peer */
struct peer_context {
	char buffer[BUFFER_SIZE];
	size_t len_read;
};

/* 2. Data Read Callback (Called when data arrives from the peer) */
static struct io_plan *read_done(struct io_conn *conn, struct peer_context *ctx) {
	printf("--- [Data Received] ---\n");

	/* Null-terminate the buffer safely to print as a string */
	if (ctx->len_read < BUFFER_SIZE) {
		ctx->buffer[ctx->len_read] = '\0';
	} else {
		ctx->buffer[BUFFER_SIZE - 1] = '\0';
	}

	printf("Payload: %s\n", ctx->buffer);
	printf("Bytes read: %zu\n", ctx->len_read);
	printf("-----------------------\n");

	/* Register the next read plan to continue the async read loop */
	return io_read_partial(conn, ctx->buffer, BUFFER_SIZE, &ctx->len_read, read_done, ctx);
}

/* 1. Connection Initialization Callback */
static struct io_plan *init_conn(struct io_conn *conn, void *ctx_arg) {
	(void)ctx_arg;
	printf("New peer connected successfully!\n");

	/* Allocate memory context for this specific peer using ccan/tal.
	 * Binding it to 'conn' ensures it auto-frees when the peer disconnects. */
	struct peer_context *ctx = tal(conn, struct peer_context);
	memset(ctx, 0, sizeof(*ctx));

	/* Set up the initial async read plan */
	return io_read_partial(conn, ctx->buffer, BUFFER_SIZE, &ctx->len_read, read_done, ctx);
}

static int beacon_event_loop_thread(void *arg) {

	int server_fd;
	struct sockaddr_in addr;
	struct io_listener *listener;

	printf("Starting Beacon Light Daemon...\n");

	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd < 0) {
		perror("Failed to create socket");
		return 1;
	}

	int opt = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
		perror("setsockopt failed");
		close(server_fd);
		return 1;
	}

	/* Set up address flags for standard port 9735 */
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(9735);

	if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("Bind failed");
		close(server_fd);
		return 1;
	}

	if (listen(server_fd, 64) < 0) {
		perror("Listen failed");
		close(server_fd);
		return 1;
	}

	/* Pass ownership of the server_fd to ccan/io */
	listener = io_new_listener(NULL, server_fd, init_conn, NULL);
	if (!listener) {
		fprintf(stderr, "Failed to create ccan/io listener\n");
		close(server_fd);
		return 1;
	}

	printf("Listening on port 9735...\n");

	/* Start the non-blocking event multiplexer loop */
	io_loop(NULL, NULL);

	printf("Shutting down ebeacon node.\n");
	return 0;
}

int main(void) {

	pthread_t io_thread;
	if (pthread_create(&io_thread, NULL, beacon_event_loop_thread, NULL) != 0) {
		perror("Failed to create thread");
		return 1;
	}

	int result;
	pthread_join(io_thread, &result);

	printf("Result: %d\n", result);

	return 0;
}


