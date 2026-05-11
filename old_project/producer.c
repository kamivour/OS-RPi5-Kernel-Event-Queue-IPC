#include "producer.h"
#include "consumer.h"

void get_datetime(char *buf, size_t buf_size)
{
    time_t now = time(NULL);
    struct tm tm_now;
    localtime_r(&now, &tm_now);
    strftime(buf, buf_size, "%Y-%m-%d %H:%M:%S", &tm_now);
}
int create_events_mode_1_2(char c, Event* e)
{
    if (c == 's') 
    {
        event_count.sensor_event+=1;
        e->number.sensor_event=event_count.sensor_event;    // numerical order
        e->type = EVENT_SENSOR;
        e->data.sensor_value = rand() % 100;    // random value
        e->processing_time = sensor_prs_time;   // default
        printf("\n---At %ld sec--- [CREATED EVENT: SENSOR event, number %d]", get_timestamp_now(), e->number.sensor_event);
    } 
    else if (c == 't') 
    {
        event_count.time_event+=1;
        e->number.time_event=event_count.time_event;    // numerical order
        e->type = EVENT_TIME;
        char* datetime_buf = malloc(32);
        get_datetime(datetime_buf, 32);
        e->data.str = datetime_buf;
        e->processing_time = time_prs_time;             // default
        printf("\n---At %ld sec--- [CREATED EVENT: TIME event, number %d]", get_timestamp_now(), e->number.time_event);
    } 
    else if (c == 'q') 
    {
        event_count.shutdown_event+=1;          // numerical order
        e->number.shutdown_event=event_count.shutdown_event;
        e->type = EVENT_SHUTDOWN;
        e->processing_time = shutdown_prs_time; // default
        printf("\n---At %ld sec--- [CREATED EVENT: SHUTDOWN event, number %d]", get_timestamp_now(), e->number.shutdown_event);
        return -1;
    } 
    else if (c >= 'a' && c <= 'z') 
    {
        event_count.button_event+=1;            // numerical order
        e->number.button_event=event_count.button_event;
        e->type = EVENT_BUTTON;
        e->data.key = c;  
        e->processing_time = button_prs_time;   // default
        printf("\n---At %ld sec--- [CREATED EVENT: BUTTON event, number %d]", get_timestamp_now(), e->number.button_event);
    }
    else 
    { 
        free(e);
        return 0;
    } 
    return 1;
}
int create_events_mode_3(TimedEvent* te, Event* e)
{
    if (te->event_char == 's')    
    {
        event_count.sensor_event++;
        e->number.sensor_event = event_count.sensor_event;
        e->type = EVENT_SENSOR;
        e->data.sensor_value = rand() % 100;
        e->processing_time = te->processing_time;  // SENSOR: 1 second     
        printf("\n---At %ld sec--- [CREATED EVENT: SENSOR event, number %d]", get_timestamp_now(), e->number.sensor_event);
    } 
    else if (te->event_char == 't') 
    {
        event_count.time_event++;
        e->number.time_event = event_count.time_event;
        e->type = EVENT_TIME;
        char* datetime_buf = malloc(32);
        get_datetime(datetime_buf, 32);
        e->data.str = datetime_buf;
        e->processing_time = te->processing_time;  // TIME: 3 seconds     
        printf("\n---At %ld sec--- [CREATED EVENT: TIME event, number %d]", get_timestamp_now(), e->number.time_event);
    } 
    else if (te->event_char == 'q') 
    {
        event_count.shutdown_event++;
        e->number.shutdown_event = event_count.shutdown_event;
        e->type = EVENT_SHUTDOWN;
        e->processing_time = te->processing_time; 
        printf("\n---At %ld sec--- [CREATED EVENT: SHUTDOWN event, number %d]", get_timestamp_now(), e->number.shutdown_event);
        return -1;
    } 
    else if (te->event_char >= 'a' && te->event_char <= 'z') 
    {
        event_count.button_event++;
        e->number.button_event = event_count.button_event;
        e->type = EVENT_BUTTON;
        e->data.key = te->event_char;
        e->processing_time = te->processing_time;  // BUTTON: 2 seconds    
        printf("\n---At %ld sec--- [CREATED EVENT: BUTTON event, number %d]", get_timestamp_now(), e->number.button_event);
    }
    else 
    { 
        free(e);
        return 0;
    } 
    return 1;
}

void* producer_run(void* arg) 
{
    dataSendToProducer* data_rev_from_main=(dataSendToProducer*)arg;
    int ret;
    
    if(data_rev_from_main->command_from_user==1)    // Mode 1
    {
        char c;
        printf("\nPress any key from a-z (NOTE: s=Sensor, t=Time, q=Quit) to create event. You can spam them\n");
        while (1) 
        {
            c = getchar();
            if(c!='\n') getchar();  // if any key is put, clear '\n' from stdin buffer
            Event* e = malloc(sizeof(Event));
            ret=create_events_mode_1_2(c, e);
            if(ret==-1) // SHUTDOWN event
            {
                queue_enqueue(data_rev_from_main->queue, e);
                break;
            }
            else if(ret==1)
                queue_enqueue(data_rev_from_main->queue, e);
        }
    }
    else if(data_rev_from_main->command_from_user==2)   // Mode 2
    {
         for(int i=0; i<strlen(data_rev_from_main->string_from_user); i++)
         {
            Event* e = malloc(sizeof(Event));
            
            sleep(data_rev_from_main->event_come_delay);    // delay create event time
            ret=create_events_mode_1_2(data_rev_from_main->string_from_user[i], e); // create event
           
            if(ret==-1) // SHUTDOWN event
            {
                queue_enqueue(data_rev_from_main->queue, e);
                break;
            }
            else if(ret==1) 
                queue_enqueue(data_rev_from_main->queue, e);
        }
    }
    else if(data_rev_from_main->command_from_user == 3) // Mode 3
    {
        int total_events_count = data_rev_from_main->num_timed_events;
  
        time_t start_time = time(NULL);

        for (int i = 0; i < total_events_count; i++) 
        {

            TimedEvent* te = &data_rev_from_main->timed_events[i];

            time_t target_time = start_time + te->timestamp;

            /* wait until correct timestamp */
            while (1) 
            {
                time_t now = time(NULL);
                if (now >= target_time)
                    break;
                sleep(target_time - now);
            }

            Event* e = malloc(sizeof(Event));
            if (!e) {
                printf("Memory allocation failed\n");
                break;
            }

            ret = create_events_mode_3(te, e);  // create event

            if (ret == -1) 
            {
                queue_enqueue(data_rev_from_main->queue, e);
                break;
            }
            else if (ret == 1) 
                queue_enqueue(data_rev_from_main->queue, e);
        }
    }
    
    if(ret!=-1) // if have no SHUTDOWN event in input string or timed events file, auto create SHUTDOWN event at the end
    {
        Event* e = malloc(sizeof(Event));
        create_events_mode_1_2('q', e);
        queue_enqueue(data_rev_from_main->queue, e);
    }
  
    return NULL;
}

