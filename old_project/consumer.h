#ifndef CONSUMER_H
#define CONSUMER_H

#include <stdio.h>
#include <stdlib.h>
#include "event.h"
#include "queue.h"

void array_key_init();
void* consumer_run(void* arg);

#endif

