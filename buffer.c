#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

#include "buffer.h"

Buffer* createMMAPBuffer(size_t size){
    
    int protection = PROT_READ | PROT_WRITE;   
    int visibility = MAP_SHARED | MAP_ANONYMOUS;

    Buffer* buffer = (Buffer*) mmap(NULL, sizeof(Buffer), protection, visibility, -1, 0);     
    buffer->count = 0;
    buffer->head = 0;
    buffer->tail = 0;
    buffer->size = size;

    buffer->requests = (LiftRequest*) mmap(NULL, size * sizeof(LiftRequest),
                                            protection, visibility, -1, 0);

    return buffer;
}

Buffer* createBuffer(size_t size) {
	
    Buffer *buffer = malloc(sizeof(Buffer));
    
    buffer->size = size;
    buffer->count = 0;
    buffer->head = 0;
    buffer->tail = 0;
    buffer->requests = (LiftRequest*) malloc(sizeof(LiftRequest) * size);

    return buffer;
}


bool isFull(Buffer* buffer){

    return buffer->count == buffer->size;
}


bool isEmpty(Buffer* buffer){

    return buffer->count == 0;
}


void deallocateBuffer(Buffer* buffer){
    
    free(buffer->requests);
    free(buffer);
}


void addRequest(Buffer* buffer, int src, int dst){

    if (!isFull(buffer)){
        buffer->requests[buffer->tail].src = src;
        buffer->requests[buffer->tail].dst = dst;
        buffer->tail ++;
        buffer->tail %= buffer->size;
        buffer->count ++;
    }
}


LiftRequest getRequest(Buffer* buffer){
    
    LiftRequest req; 

    if (!isEmpty(buffer)){
        req = buffer->requests[buffer->head];
        buffer->head ++;
        buffer->head %= buffer->size;
        buffer->count --;
    }

    return req;
}

