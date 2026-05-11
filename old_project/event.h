#ifndef EVENT_H
#define EVENT_H

#include <stdio.h>
#include <unistd.h>
#include "queue.h"
#include "time.h"

extern time_t program_start_time;
typedef struct
{
    char key;
    int count;
} ButtonEvent;  // define data storage for BUTTON event
extern ButtonEvent button_event[26];  // 26 key in english alphabet

typedef struct
{
    int sensor_event;
    int button_event;
    int time_event;
    int shutdown_event;
} EventCount; // define total count of each events
extern EventCount event_count;

typedef enum 
{
    EVENT_SENSOR,
    EVENT_BUTTON,
    EVENT_TIME,
    EVENT_SHUTDOWN
} EventType;  // define event type

typedef struct 
{
    char* str;        // for date-time string from TIME event
    char key;         // for button char from BUTTON event
    int sensor_value; // for sensor value from SENSOR event
} EventData;

typedef struct 
{
    EventType type;   // SENSOR/BUTTON/TIME/SHUTDOWN
    EventData data; 
    EventCount number;
    int processing_time;
} Event;

time_t get_timestamp_now(void);
const char* event_to_string(Event* e, int* count);
void enqueue_done(Event* e, int slots_have_event);
void dequeue_done(Event* e, int slots_have_event);

#endif


