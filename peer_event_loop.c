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
#include "peer_event_loop.h"
#include <pthread.h>

static struct io_plan *read_done(struct io_conn *conn, struct peer_context *ctx) {

	handle_message_from_peer(ctx);

	/* Register the next plan */
	return io_read_partial(conn, ctx->read_buffer, BUFFER_SIZE, &ctx->len_read, read_done, ctx);
}

static struct io_plan *init_inbound_conn(struct io_conn *conn, void *ctx_arg) {
	(void)ctx_arg;

	struct peer_context *ctx = tal(conn, struct peer_context);
	memset(ctx, 0, sizeof(*ctx));

	handle_connected_from_peer(ctx);

	/* Register the next plan */
	return io_read_partial(conn, ctx->read_buffer, BUFFER_SIZE, &ctx->len_read, read_done, ctx);
}

static struct io_listener * io_listen_peer(void *arg) {
	(void)arg;

	int fd_in;
	struct sockaddr_in addr;
	struct io_listener *listener;

	fd_in = socket(AF_INET, SOCK_STREAM, 0);
	if (fd_in < 0) {
		perror("Failed to create socket");
		return NULL;
	}

	int opt = 1;
	if (setsockopt(fd_in, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
		perror("setsockopt failed");
		close(fd_in);
		return NULL;
	}

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(9735);

	if (bind(fd_in, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("Bind failed");
		close(fd_in);
		return NULL;
	}

	if (listen(fd_in, 64) < 0) {
		perror("Listen failed");
		close(fd_in);
		return NULL;
	}

	listener = io_new_listener(NULL, fd_in, init_inbound_conn, NULL);
	if (!listener) {
		fprintf(stderr, "Failed to create ccan/io listener\n");
		close(fd_in);
		return NULL;
	}

	printf("[Server LOG] Listening on port 9735...\n");

	return listener;
}

static struct io_plan *idle_connection(struct io_conn *conn, void *b)
{
	return io_wait(conn, b, idle_connection, NULL);
}

static struct io_plan *close_connection(struct io_conn *conn, struct peer_context *ctx) {

	printf("[Client LOG] Data successfully written to the peer.\n");

	/* Register the next plan */
	return io_close(conn);
}

static struct io_plan *init_outbound_conn(struct io_conn *conn, void *ctx_arg) {

	struct peer_context *ctx = tal(conn, struct peer_context);
	memset(ctx, 0, sizeof(*ctx));

	printf("[Client LOG] Successfully established connection to remote peer!\n");

	strcpy(ctx->write_buffer, "Hello from BLD outbound client!");
	size_t msg_len = strlen(ctx->write_buffer);

	/* Register the next plan */
	return io_write(conn, ctx->write_buffer, msg_len, idle_connection, ctx);
}

static struct io_conn * io_connect_peer(struct node_request *req) {

	const char *ip_address = req->ip_address;
	uint16_t port = req->port;

	int fd_out;
	struct sockaddr_in remote_addr;

	printf("[Client LOG] Processing request: Connect to %s:%d\n", ip_address, port);

	fd_out = socket(AF_INET, SOCK_STREAM, 0);
	if (fd_out < 0) {
		perror("[Client ERROR] Failed to create socket");
		return NULL;
	}

	/* Set socket to non-blocking mode (CRITICAL for ccan/io) */
	int flags = fcntl(fd_out, F_GETFL, 0);
	if (flags >= 0) {
		fcntl(fd_out, F_SETFL, flags | O_NONBLOCK);
	}

	remote_addr.sin_family = AF_INET;
	remote_addr.sin_port = htons(port);
	if (inet_pton(AF_INET, ip_address, &remote_addr.sin_addr) <= 0) {
		fprintf(stderr, "[Client ERROR] Invalid IP address format: %s\n", ip_address);
		close(fd_out);
		return NULL;
	}

	/* Start the connection process.
	 * Since the socket is non-blocking, this will return immediately with -1 
	 * and errno set to EINPROGRESS. This is expected behavior. */
	if (connect(fd_out, (struct sockaddr *)&remote_addr, sizeof(remote_addr)) < 0) {
		if (errno != EINPROGRESS) {
			perror("[Client ERROR] Connection initiation failed");
			close(fd_out);
			return NULL;
		}
	}

	/* * Pass the non-blocking fd_out over to ccan/io.
	 * ccan/io internally monitors the file descriptor until the TCP 3-way handshake 
	 * completes, then fires the 'init_outbound_conn' callback.
	 */
	struct io_conn *conn = io_new_conn(NULL, fd_out, init_outbound_conn, NULL);
	if (!conn) {
		fprintf(stderr, "[Client ERROR] Failed to bind connection to ccan/io\n");
		close(fd_out);
		return NULL;
	}

	return conn;
}

static struct io_plan *process_request_from_local(struct io_conn *conn, struct request_queue_ctx *ctx) {

	pthread_mutex_lock(&ctx->mutex);

	struct node_request *req = ctx->head;
	ctx->head = NULL;
	ctx->tail = NULL;

	pthread_mutex_unlock(&ctx->mutex);

	while (req) {
		switch (req->type) {
			case REQ_CONNECT:
				io_connect_peer(req);
				break;
			default:
				io_break(NULL);
				break;
		}

		struct node_request *next = req->next;
		free(req);
		req = next;
	}

	printf("[Local  LOG] done process_shared_queue.\n");

	/* Register the next plan */
	return io_read(conn, &ctx->ev_counter, sizeof(ctx->ev_counter), process_request_from_local, ctx);
}

static struct io_plan *init_eventfd_conn(struct io_conn *conn, struct request_queue_ctx *ctx) {

	printf("[Local  LOG] eventfd monitoring started.\n");

	/* Register the next plan */
	return io_read(conn, &ctx->ev_counter, sizeof(ctx->ev_counter), process_request_from_local, ctx);
}

static struct io_conn * io_connect_eventfd(void *arg) {

	struct request_queue_ctx* q_ctx = (struct request_queue_ctx*)arg;

	/* Register the next plan */
	return io_new_conn(NULL, q_ctx->ev_fd, init_eventfd_conn, q_ctx);
}

void* peer_io_loop(void *arg) {

	// Listening connection from peer
	struct io_listener * listener = io_listen_peer(arg);
	if (listener == NULL)
		return NULL;

	// Listening request from local
	struct io_conn *ev_conn = io_connect_eventfd(arg);
	if (ev_conn == NULL) {
		io_close_listener(listener);
		return NULL;
	}

	/* Start the non-blocking event multiplexer loop */
	io_loop(NULL, NULL);

	io_close_listener(listener);
	io_close(ev_conn);

	return NULL;
}

