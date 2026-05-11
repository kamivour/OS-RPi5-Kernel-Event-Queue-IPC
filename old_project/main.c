#include <pthread.h>
#include "queue.h"
#include "producer.h"
#include "consumer.h"
#include "event.h"
#include <stdlib.h> 
#include <time.h>   

int main() 
{
    srand(time(NULL)); 
    program_start_time=time(NULL);
    
    BlockingQueue queue;
    queue_init(&queue);
    array_key_init();
    
    printf("CONFIGURATING..........\n");
    
    dataSendToProducer data_send_to_producer={.queue=&queue}; // Argument
    
    int mode=0;
    printf("MODE SELECTION\n");
    printf("  1 - Manual input (self-enter keys)\n");
    printf("  2 - Auto mode (enter string directly)\n");
    printf("  3 - File input (read from file)\n");
    printf("Enter mode (1/2/3) -> ");
    scanf("%d", &mode); 
    getchar();  
    while(mode < 1 || mode > 3) // Choose mode until valid
    {
        printf("That mode is not supported, choose again!\n");
        printf("Enter mode (1/2/3): ");
        scanf("%d", &mode); 
        getchar(); 
    }
    
    if(mode == 2)   // Create events from an input string
    {
        char* line = NULL; 
        size_t len = 0;
        printf("Enter input string -> ");
        ssize_t nread = getline(&line, &len, stdin);  
        if (nread > 0 && line[nread - 1] == '\n') line[nread - 1] = '\0'; 
        
        printf("Enter delay time for each events (s) -> ");  
        scanf("%d", &(data_send_to_producer.event_come_delay)); 
        getchar();
        
        data_send_to_producer.string_from_user = line;
    }
    else if(mode == 3)  // Read timed events from file, format: <timestamp> <char> <processing_time>
    {
        char filename[256];
        printf("Enter input filename: ");

        if (fgets(filename, sizeof(filename), stdin) == NULL) 
        {
            printf("Error: Failed to read filename\n");
            return 1;
        }

        /* Remove newline */
        size_t len = strlen(filename);
        if (len > 0 && filename[len - 1] == '\n')
            filename[len - 1] = '\0';

        /* Open file */
        FILE* file = fopen(filename, "r");
        if (file == NULL) 
        {
            printf("Error: Cannot open file '%s'\n", filename);
            return 1;
        }

        char buffer[256];
        int line_count = 0;

        /* First pass: count valid lines */
        while (fgets(buffer, sizeof(buffer), file))
        {
            if (buffer[0] == '\n' || buffer[0] == '#')
                continue;
            line_count++;
        }

        if (line_count == 0) 
        {
            printf("Error: No valid events found\n");
            fclose(file);
            return 1;
        }

        /* Allocate memory */
        TimedEvent* timed_events = (TimedEvent*)malloc(sizeof(TimedEvent) * line_count);

        if (!timed_events) 
        {
            printf("Error: Memory allocation failed\n");
            fclose(file);
            return 1;
        }

        /* Second pass: parse events */
        fseek(file, 0, SEEK_SET);

        int idx = 0;
        while (fgets(buffer, sizeof(buffer), file) && idx < line_count) 
        {
            if (buffer[0] == '\n' || buffer[0] == '#')
                continue;

            int timestamp;
            char event_char;
            int processing_time;

            /* Parse 3 columns */
            if (sscanf(buffer, "%d %c %d", &timestamp, &event_char, &processing_time) == 3) 
            {

                timed_events[idx].timestamp = timestamp;
                timed_events[idx].event_char = event_char;
                timed_events[idx].processing_time = processing_time;

                idx++;
            } 
            else 
            {
                printf("Warning: Invalid line format -> %s", buffer);
            }
        }

        fclose(file);

        /* Optional debug print */
        printf("Loaded %d events:\n", idx);
            
        data_send_to_producer.timed_events = timed_events;
        data_send_to_producer.num_timed_events = idx;
    }
    else 
    {
        data_send_to_producer.timed_events = NULL;
        data_send_to_producer.num_timed_events = 0;
    }
    
    data_send_to_producer.command_from_user=mode;
    
    // Create threads
    pthread_t prod, cons;
    pthread_create(&prod, NULL, producer_run, &data_send_to_producer);
    pthread_create(&cons, NULL, consumer_run, data_send_to_producer.queue);
  
    pthread_join(prod, NULL);    // wait for producer thread stop
    queue_stop(&queue);          // stop queue
    pthread_join(cons, NULL);    // wait for consumer thread stop
    queue_destroy(&queue);
    
    return 0;
}

