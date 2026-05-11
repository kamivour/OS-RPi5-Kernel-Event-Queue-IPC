#include "consumer.h"

void array_key_init()
{
    for(int i=0; i<26; i++)
    {
        button_event[i].key=i+97; // 'a' -> 'z'
        button_event[i].count=0;
    }
}

void handle_sensor(Event* e) 
{
    printf("\n---At %ld sec--- [HANDLER DONE: SENSOR event, number %d] Random sensor value: %d\n", get_timestamp_now(), e->number.sensor_event, e->data.sensor_value);
}

void handle_button(Event* e) 
{
    for(int i=0; i<26; i++)
    {
        if(e->data.key == button_event[i].key)  // find key in button_event[i] to increase count of that key to 1
        {
            button_event[i].count+=1;
            printf("\n---At %ld sec--- [HANDLER DONE: BUTTON event, number %d] Key just entered is '%c' with total count: %d\n", get_timestamp_now(), e->number.button_event, button_event[i].key, button_event[i].count);
            break;
        }
    }    
}

void handle_time(Event* e) 
{
    printf("\n---At %ld sec--- [HANDLER DONE: TIME event, number %d] Time & Date: %s\n", get_timestamp_now(), e->number.time_event, e->data.str);
}

void handle_event(Event* e)
{
    sleep(e->processing_time);   
    switch (e->type) 
    {
        case EVENT_SENSOR: handle_sensor(e); break;
        case EVENT_BUTTON: handle_button(e); break;
        case EVENT_TIME: handle_time(e); break;
        case EVENT_SHUTDOWN:
        {
            printf("\n---At %ld sec--- [SYSTEM] SHUTDOWN event received. System stopping...\n", get_timestamp_now());
            printf("\n--> [SUMMARY]: Event total counts: %d, with:\n", event_count.sensor_event + event_count.button_event + event_count.time_event + event_count.shutdown_event);
            printf("    Total SENSOR events: %d\n", event_count.sensor_event);
            printf("    Total BUTTON events: %d, with:\n", event_count.button_event);
              for(int i=0; i<26; i++) // print all keys of BUTTON events and their total count
              {
                  if(button_event[i].count!=0) printf("       Key '%c' with total count: %d\n", button_event[i].key, button_event[i].count); 
              }
            printf("    Total TIME events: %d\n", event_count.time_event);
            printf("    Total SHUTDOWN events: %d\n", event_count.shutdown_event);
            break;
        }
        default: break;
    }
}
void* consumer_run(void* arg) 
{
    
    BlockingQueue* queue = (BlockingQueue*)arg;

    while (1) 
    {
        Event* e = (Event*)queue_dequeue(queue);
        
        if (!e) break;
        
        if (e->type == EVENT_SHUTDOWN) 
        {
            handle_event(e);
            free(e);
            break; 
        }
        
        handle_event(e);
        free(e);
    }
    
    return NULL;
}

