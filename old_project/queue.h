#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>
#include <errno.h>
#include "event.h"
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#define QUEUE_CAPACITY 5

typedef struct {
    void* buffer[QUEUE_CAPACITY];
    int front;
    int rear;
    int count;
    bool running;

    pthread_mutex_t mutex;
    pthread_cond_t not_full;
    pthread_cond_t not_empty;
} BlockingQueue;

void queue_init(BlockingQueue* q);
void queue_destroy(BlockingQueue* q);

bool queue_is_full(BlockingQueue* q);
bool queue_is_empty(BlockingQueue* q);

void queue_enqueue(BlockingQueue* q, void* item);
void* queue_dequeue(BlockingQueue* q);

void queue_stop(BlockingQueue* q);

#endif


