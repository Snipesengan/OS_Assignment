#include <stdbool.h>
#include "lift.h"

#ifndef FIFO_BUF_H
#define FIFO_BUF_H

#define FIFO_BUF_ERR_FULLENQ -1
#define FIFO_BUF_ERR_EMPTYDEQ -2

typedef struct _fifo_buf_t {
    
    LiftRequest* requests;
    size_t head;
    size_t tail;
    size_t max;
    size_t count;

} fifo_buf_t;

fifo_buf_t* fifo_buf_init_malloc(size_t size);
fifo_buf_t* fifo_buf_init_mmap(size_t size);
bool fifo_buf_full(fifo_buf_t* buf);
bool fifo_buf_empty(fifo_buf_t* buf);
void fifo_buf_destroy_malloc(fifo_buf_t* buf);
void fifo_buf_destroy_mmap(fifo_buf_t* buf);
int fifo_buf_enqueue(fifo_buf_t* buf, LiftRequest request);
int fifo_buf_dequeue(fifo_buf_t* buf, LiftRequest* request);

#endif

