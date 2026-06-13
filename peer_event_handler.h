#ifndef PEER_EVENT_HANDLER_H
#define PEER_EVENT_HANDLER_H


#include "peer.h"

int handle_connected_from_peer(struct peer_context* ctx);
int handle_message_from_peer(struct peer_context* ctx);

#endif //PEER_EVENT_HANDLER_H
