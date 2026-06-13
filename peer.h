#ifndef PEER_H
#define PEER_H

#define BUFFER_SIZE 1024

/* Context structure allocated per connected peer */
struct peer_context {
	char read_buffer[BUFFER_SIZE];
	char write_buffer[BUFFER_SIZE];
	size_t len_read;
};





#endif //PEER_H
