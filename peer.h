#ifndef PEER_H
#define PEER_H

#define BUFFER_SIZE 1024

struct conn_handlers;

/* Context structure allocated per connected peer */
struct peer_context {
	char buffer[BUFFER_SIZE];
	size_t len_read;
	struct conn_handlers* hdl;
};





#endif //PEER_H
