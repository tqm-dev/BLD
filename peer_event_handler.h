#ifndef PEER_EVENT_HANDLER_H
#define PEER_EVENT_HANDLER_H


#include "peer.h"

struct conn_handlers {
	int (*on_init_conn)(struct peer_context* ctx);
	int (*on_read_done)(struct peer_context* ctx);
};

struct conn_handlers* get_handlers(void);


#endif //PEER_EVENT_HANDLER_H
