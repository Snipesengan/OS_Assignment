/* File  : fifo_buf.c
 * Date  : 20-04-2020
 * Author: Nhan Dao <nhan.dao@outlook.com>
 */ 

#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

#include "fifo_buf.h"

fifo_buf_t* fifo_buf_init_mmap(size_t size){
    
    int protection = PROT_READ | PROT_WRITE;   
    int visibility = MAP_SHARED | MAP_ANONYMOUS;

    fifo_buf_t* buffer = (fifo_buf_t*) mmap(NULL, 
                                            sizeof(fifo_buf_t), 
                                            protection, visibility, -1, 0);     
    buffer->count = 0;
    buffer->head = 0;
    buffer->tail = 0;
    buffer->max = size;

    buffer->requests = (LiftRequest*) mmap(NULL, size * sizeof(LiftRequest),
                                            protection, visibility, -1, 0);

    return buffer;
}

fifo_buf_t* fifo_buf_init_malloc(size_t size) {
	
    fifo_buf_t* buffer = (fifo_buf_t*) malloc(sizeof(fifo_buf_t));
    
    buffer->max = size;
    buffer->count = 0;
    buffer->head = 0;
    buffer->tail = 0;
    buffer->requests = (LiftRequest*) malloc(sizeof(LiftRequest) * size);

    return buffer;
}


bool fifo_buf_full(fifo_buf_t* buffer){

    return buffer->count == buffer->max;
}


bool fifo_buf_empty(fifo_buf_t* buffer){

    return buffer->count == 0;
}


void fifo_buf_destroy_malloc(fifo_buf_t* buffer){

    free(buffer->requests);
    free(buffer);
}


void fifo_buf_destroy_mmap(fifo_buf_t* buffer){
    
    if (munmap(buffer->requests, buffer->max * sizeof(LiftRequest)) < 0 ||
        munmap(buffer, sizeof(fifo_buf_t))){
        perror("error while unmapping buffer");
    }
}


int fifo_buf_enqueue(fifo_buf_t* buffer, LiftRequest request){

    if (fifo_buf_full(buffer)){
        return FIFO_BUF_ERR_FULLENQ;
    }

    buffer->requests[buffer->tail]= request;
    buffer->tail = (buffer->tail + 1) % buffer-> max;
    buffer->count ++;

    return 0;
}


int fifo_buf_dequeue(fifo_buf_t* buffer, LiftRequest* request){

    if (fifo_buf_empty(buffer)) {
        return FIFO_BUF_ERR_EMPTYDEQ;
    }

    *request = buffer->requests[buffer->head];
    buffer->head =(buffer->head + 1) % buffer->max;
    buffer->count --;

    return 0;
}
