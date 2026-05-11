#include "event.h"

time_t program_start_time=0;
ButtonEvent button_event[26];
EventCount event_count=
{
    .sensor_event=0,
    .button_event=0,
    .time_event=0,
    .shutdown_event=0
};  // Total count of each event when start program

time_t get_timestamp_now(void) 
{
    return (time_t)(time(NULL) - program_start_time);
}

const char* event_to_string(Event* e, int* count) 
{
    switch (e->type) 
    {
        case EVENT_SENSOR: 
        {
            *count=e->number.sensor_event;
            return "SENSOR";
        }
        case EVENT_BUTTON: 
        {
            *count=e->number.button_event;
            return "BUTTON";
        }
        case EVENT_TIME:  
        {
            *count=e->number.time_event;
            return "TIME";
        }
        case EVENT_SHUTDOWN: 
        {
            *count=e->number.shutdown_event;
            return "SHUTDOWN";
        }
        default: return "UNKNOWN";
    }
} 

void enqueue_done(Event* e, int slots_have_event)   // successfully enqueue
{
    int count=0;    // numerical order of this event
    const char* event_string=event_to_string(e, &count);
    printf("\n---At %ld sec--- [ENQUEUE DONE: %s event, number %d]", get_timestamp_now(), event_string, count); 
    printf(" -> Rest %d slots in blocking queue\n", QUEUE_CAPACITY - slots_have_event);
}

void dequeue_done(Event* e, int slots_have_event)   // successfully dequeue
{
    int count=0;    // numerical order of this event
    const char* event_string=event_to_string(e, &count);
    printf("\n---At %ld sec--- [DEQUEUE DONE, HANDLING...: %s event, number %d]", get_timestamp_now(), event_string, count); 
    printf(" -> Rest %d slots in blocking queue\n", QUEUE_CAPACITY - slots_have_event);
}



