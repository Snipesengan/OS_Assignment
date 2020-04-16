#include <unistd.h>

#ifndef LIFT_REQUEST_H
#define LIFT_REQUEST_H

typedef struct LiftRequest {

    int src;
    int dst;
} LiftRequest;


/* Not used */
typedef struct _liftStatus {

    int id;
    int position;
    int nMove; 
    int nRequest;
    useconds_t t;
} LiftStatus;

#endif
