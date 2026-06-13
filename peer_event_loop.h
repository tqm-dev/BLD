#ifndef PEER_EVENT_LOOP_H
#define PEER_EVENT_LOOP_H

#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>

#include "peer.h"

typedef enum {
    REQ_CONNECT,
    REQ_WRITE,
    REQ_SHUTDOWN
} request_type_t;

struct node_request {
    request_type_t type;
    char ip_address[64];
    uint16_t port;
    struct node_request *next;
};

struct request_queue_ctx {
    int ev_fd;
    uint64_t ev_counter;
    struct node_request *head;
    struct node_request *tail;
    pthread_mutex_t mutex;
};

void* peer_io_loop(void *arg);


#endif //PEER_EVENT_LOOP_H
