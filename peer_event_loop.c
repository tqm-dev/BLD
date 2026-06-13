#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <ccan/io/io.h>
#include <sys/eventfd.h>
#include <errno.h>
#include <string.h>
#include "peer.h"
#include "peer_event_handler.h"
#include <pthread.h>

static struct io_plan *read_done(struct io_conn *conn, struct peer_context *ctx) {

	handle_message_from_peer(ctx);

	/* Register the next read plan to continue the async read loop */
	return io_read_partial(conn, ctx->buffer, BUFFER_SIZE, &ctx->len_read, read_done, ctx);
}

static struct io_plan *init_conn(struct io_conn *conn, void *ctx_arg) {
	(void)ctx_arg;

	/* Allocate memory context for this specific peer using ccan/tal.
	 * Binding it to 'conn' ensures it auto-frees when the peer disconnects. */
	struct peer_context *ctx = tal(conn, struct peer_context);
	memset(ctx, 0, sizeof(*ctx));

	handle_connected_from_peer(ctx);

	/* Set up the initial async read plan */
	return io_read_partial(conn, ctx->buffer, BUFFER_SIZE, &ctx->len_read, read_done, ctx);
}

static struct io_listener * peer_recv_event_loop_main(void *arg) {
	(void)arg;

	int server_fd;
	struct sockaddr_in addr;
	struct io_listener *listener;

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
	listener = io_new_listener(NULL, server_fd, init_conn, NULL);
	if (!listener) {
		fprintf(stderr, "Failed to create ccan/io listener\n");
		close(server_fd);
		return NULL;
	}

	printf("[Server LOG] Listening on port 9735...\n");

	return listener;

}

static struct io_plan *client_write_done(struct io_conn *conn, struct peer_context *ctx) {

	printf("[Client LOG] Data successfully written to the peer.\n");
	return io_close(conn);
}

static struct io_plan *init_outbound_conn(struct io_conn *conn, void *ctx_arg) {

	struct peer_context *ctx = tal(conn, struct peer_context);
	memset(ctx, 0, sizeof(*ctx));

	printf("[Client LOG] Successfully established connection to remote peer!\n");

	strcpy(ctx->buffer, "Hello from BLD outbound client!");
	size_t msg_len = strlen(ctx->buffer);

	return io_write(conn, ctx->buffer, msg_len, client_write_done, ctx);
}

static struct io_conn * peer_send_event_loop_main(void* arg) {

	const char *ip_address = "127.0.0.1";
	uint16_t port = 9735;

	int client_fd;
	struct sockaddr_in remote_addr;

	printf("[Client LOG] Processing request: Connect to %s:%d\n", ip_address, port);

	/* Create standard TCP Socket */
	client_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (client_fd < 0) {
		perror("[Client ERROR] Failed to create socket");
		return NULL;
	}

	/* Set socket to non-blocking mode (CRITICAL for ccan/io) */
	int flags = fcntl(client_fd, F_GETFL, 0);
	if (flags >= 0) {
		fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
	}

	/* Set up remote address structure */
	remote_addr.sin_family = AF_INET;
	remote_addr.sin_port = htons(port);
	if (inet_pton(AF_INET, ip_address, &remote_addr.sin_addr) <= 0) {
		fprintf(stderr, "[Client ERROR] Invalid IP address format: %s\n", ip_address);
		close(client_fd);
		return NULL;
	}

	/* Start the connection process.
	 * Since the socket is non-blocking, this will return immediately with -1 
	 * and errno set to EINPROGRESS. This is expected behavior. */
	if (connect(client_fd, (struct sockaddr *)&remote_addr, sizeof(remote_addr)) < 0) {
		if (errno != EINPROGRESS) {
			perror("[Client ERROR] Connection initiation failed");
			close(client_fd);
			return NULL;
		}
	}

	/* * Pass the non-blocking client_fd over to ccan/io.
	 * ccan/io internally monitors the file descriptor until the TCP 3-way handshake 
	 * completes, then fires the 'init_outbound_conn' callback.
	 */
	struct io_conn *conn = io_new_conn(NULL, client_fd, init_outbound_conn, NULL);
	if (!conn) {
		fprintf(stderr, "[Client ERROR] Failed to bind connection to ccan/io\n");
		close(client_fd);
		return NULL;
	}

	return conn;
}

void* peer_io_loop(void *arg) {

	struct io_listener * listener = peer_recv_event_loop_main(arg);
	if (listener == NULL)
		return NULL;

	struct io_conn * conn = peer_send_event_loop_main(arg);
	if (conn == NULL)
		return NULL;

	/* Start the non-blocking event multiplexer loop */
	io_loop(NULL, NULL);

	io_close_listener(listener);
	io_close(conn);

	return NULL;
}

