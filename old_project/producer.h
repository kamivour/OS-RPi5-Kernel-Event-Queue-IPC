#ifndef PRODUCER_H
#define PRODUCER_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "queue.h"
#include "time.h"
#include "event.h"

typedef enum
{
    sensor_prs_time=1,
    time_prs_time=3,
    button_prs_time=2,
    shutdown_prs_time=0
} DefaultProcessingTime;    // Default processing time use for mode 1 & 2

typedef struct 
{
    int timestamp;      // When to create event (seconds)
    char event_char;    // Event character (s, t, a-z, q)
    int processing_time;// Processing time
} TimedEvent;   // Structure for timed events in mode 3

typedef struct
{
    BlockingQueue* queue;
    
    int command_from_user;    // for what mode is used (1 or 2 or 3)
    
    char* string_from_user;   // for input string from user in mode 2
    int event_come_delay;     // for delay create time event in mode 2
    
    TimedEvent* timed_events; // for timed events in mode 3
    int num_timed_events;     // for count of timed events in mode 3
} dataSendToProducer;

void* producer_run(void* arg);

#endif

