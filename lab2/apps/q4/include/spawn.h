#ifndef __USERPROG__
#define __USERPROG__

#include "lab2-api.h"

typedef struct circular_buffer {
  int head;
  int tail;
  char buff[BUFFERSIZE]; 
} circular_buffer;

#define PRODUCER_TO_RUN "producer.dlx.obj"
#define CONSUMER_TO_RUN "consumer.dlx.obj"

#endif
