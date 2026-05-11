#include "queue.h"

void queue_init(BlockingQueue* q) 
{
    q->front = q->rear = q->count = 0;
    q->running = true;

    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->not_full, NULL);
    pthread_cond_init(&q->not_empty, NULL);
}

void queue_destroy(BlockingQueue* q) 
{
    pthread_mutex_destroy(&q->mutex);
    pthread_cond_destroy(&q->not_full);
    pthread_cond_destroy(&q->not_empty);
}

bool queue_is_full(BlockingQueue* q) 
{
    return q->count == QUEUE_CAPACITY;
}

bool queue_is_empty(BlockingQueue* q) 
{
    return q->count == 0;
}

void queue_enqueue(BlockingQueue* q, void* item) 
{
    pthread_mutex_lock(&q->mutex);  
    
    while (queue_is_full(q) && q->running) 
    {
        printf("\nProducer thread is blocked!\n");
        pthread_cond_wait(&q->not_full, &q->mutex);
    }
    
    if (!q->running) 
    {
        pthread_mutex_unlock(&q->mutex);
        return;
    }

    q->buffer[q->rear] = item;
    q->rear = (q->rear + 1) % QUEUE_CAPACITY;
    q->count++;
    
    enqueue_done(item, q->count);
    
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->mutex);
}

void* queue_dequeue(BlockingQueue* q) 
{
    pthread_mutex_lock(&q->mutex);
    
    while (queue_is_empty(q) && q->running) 
    {
        printf("\nConsumer thread is blocked!\n");
        pthread_cond_wait(&q->not_empty, &q->mutex);
    }

    if (!q->running && queue_is_empty(q)) 
    {
        pthread_mutex_unlock(&q->mutex);
        return NULL;
    }

    void* item = q->buffer[q->front];
    q->front = (q->front + 1) % QUEUE_CAPACITY;
    q->count--;
    
    dequeue_done(item, q->count);
    
    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->mutex);
    return item;
}

void queue_stop(BlockingQueue* q) 
{
    pthread_mutex_lock(&q->mutex);
    q->running = false;
    pthread_cond_broadcast(&q->not_empty);
    pthread_cond_broadcast(&q->not_full);
    pthread_mutex_unlock(&q->mutex);
}


