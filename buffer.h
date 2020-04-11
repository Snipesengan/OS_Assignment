
#include <stdbool.h>
#include "lift.h"

#ifndef BUFFER_H
#define BUFFER_H

typedef struct _buffer {
    
    LiftRequest* requests;
    size_t head;
    size_t tail;
    size_t size;
	size_t count;

} Buffer;

Buffer* createBuffer(size_t size);
void deallocateBuffer(Buffer*);
void addRequest(Buffer*, int, int);
LiftRequest getRequest(Buffer*);
bool isFull(Buffer*);
bool isEmpty(Buffer*);

#endif
