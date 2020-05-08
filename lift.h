#include <unistd.h>

#ifndef LIFT_REQUEST_H
#define LIFT_REQUEST_H

typedef struct LiftRequest {

    int src;
    int dst;
} LiftRequest;


typedef struct _liftStatus {

    int id;
    int pos;
    int total_move;
    int n_request;

} LiftStatus;

#endif
