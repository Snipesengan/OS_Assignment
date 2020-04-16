/* Implementation using pthreads */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>

#include "lift.h"
#include "buffer.h"

#define INPUT_FILE_PATH "./sim_input.txt"
#define OUTPUT_FILE_PATH "./sim_out.txt"

void *request(void*);
void *lift(void*);
void logLiftRequest(FILE*, LiftRequest, int, int, int, int);
bool hasStopped(void);

/* Should we make shared data global? */
FILE* fout = NULL;
Buffer* buffer = NULL;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t hasSpace = PTHREAD_COND_INITIALIZER;
pthread_cond_t nonEmpty = PTHREAD_COND_INITIALIZER;
pthread_mutex_t stop = PTHREAD_MUTEX_INITIALIZER;
int stopIssued = 0;
useconds_t t;


int main(int argc, char **argv){

    int m, i;
    pthread_t tLiftRequest;
    pthread_t tLift[3];


    if (argc != 3){
        printf("./lift_sim_A m t\n\n");
        printf("\tm = buffer size\n");
        printf("\tt = time per request in milliseconds\n");
    }
    else {

        m = atoi(argv[1]);
        t = atoi(argv[2]);

        if (m <= 0 || t <= 0){
            fprintf(stderr, "Invalid arguments\n");
            return 0;
        }

        /* Open sim out file */
        fout = fopen(OUTPUT_FILE_PATH, "w");

        /* Initialize buffer */
        buffer = createBuffer(m);

        /* Create threads */
        pthread_create(&tLiftRequest, NULL, request, NULL);
        for (i = 0; i < 3; i ++){
            pthread_create(&tLift[i], NULL, lift, &i);
        }
        
        /* Join thread (Essentially wait for all thread to finish work)*/
        pthread_join(tLiftRequest, NULL);
        for (i =0; i< 3; i ++){
            pthread_join(tLift[i], NULL);
        }

        if (fclose(fout) != 0){
            perror("Error closing the file\n");
        }

        deallocateBuffer(buffer);
    }

    return 0;
}


void* request(void* args){
    
    char line[512];
    FILE* fin = fopen(INPUT_FILE_PATH, "r");
    int nRequest = 1;


    /* Read all the request from simulation file */
    while (fgets(line, 512, fin) != NULL){ /* Scanning until EOF */

        int src, dst;

        if( sscanf(line, "%d %d", &src, &dst) != 2 || src < 1 || dst < 1 ){
            fprintf(stderr, "Encountered an invalid lift request, choosing to ignore\n");
            continue;
        }

        /* acquire the lock */
        pthread_mutex_lock(&lock);

        #ifdef DEBUG
        printf("Attempting to add new request\n");
        #endif

        /* block until buffer has space */
        while (isFull(buffer)){
            #ifdef DEBUG
            printf("\tBuffer is full, waiting...\n");
            #endif

            pthread_cond_wait(&hasSpace, &lock);
        }

        #ifdef DEBUG
        printf("\tAdding request { src = %d, dst = %d }\n", src, dst);
        #endif
        
        addRequest(buffer, src, dst); 
        fprintf(fout, "New Lift Request From Floor %d to Floor %d\n", src, dst);
        fprintf(fout, "Request No: %d\n", nRequest);
        fprintf(fout, "--------------------------\n");

        /* Signal waiting threads and release the lock */
        pthread_cond_signal(&nonEmpty);
        pthread_mutex_unlock(&lock);

        /* Non-Critical Operations */
        nRequest ++;
    } 

    /* Signal other threads that there's no item left */
    pthread_mutex_lock(&stop);
    stopIssued = 1;
    pthread_cond_signal(&nonEmpty); /* This is sort of a hack */
    pthread_mutex_unlock(&stop);

    /* Closing the input file */
    if (fclose(fin) != 0){
        perror("Error closing input file");
    }

    #ifdef DEBUG
    printf("Request thread done\n");
    #endif

    return 0;
}


/* lift */
void* lift(void* args){


    LiftRequest request;
    int id = *((int*) args);
    int position = 0;
    int nMove = 0;
    int nRequest = 0;

    
    for(;;){   
        /* Acquire the lock */
        pthread_mutex_lock(&lock);

        #ifdef DEBUG
        printf("Attempting to remove from buffer\n");
        #endif

        /* Block until theres item in the buffer */
        while (isEmpty(buffer) && !hasStopped()) {
            #ifdef DEBUG
            printf("\tBuffer is empty, waiting...\n");
            #endif
            /* This releases the mutex which blocks calling thread on the condition variable */
            pthread_cond_wait(&nonEmpty, &lock);
        }

        /* If this occurs then the producer has no more item to produce */
        if (isEmpty(buffer)){
            pthread_mutex_unlock(&lock);
            break;
        }

        request = getRequest(buffer);

        #ifdef DEBUG
        printf("\tLift-%d serving request { src = %d, dst = %d }\n", 
            id, request.src, request.dst);
        #endif
        
        /* Log Lift request */
        logLiftRequest(fout, request, id, position, nMove, nRequest);

        /* Unlock */
        pthread_cond_signal(&hasSpace);
        pthread_mutex_unlock(&lock);

        /* Non-critical Operations */
        nRequest += 1;
        nMove += abs(position - request.src) + abs(request.src - request.dst);
        position = request.dst;
        usleep(t * 1000);
   }

   #ifdef DEBUG
   printf("Lift-%d thread done\n", id);
   #endif

   return 0;
}


void logLiftRequest(FILE* f, LiftRequest request, int id, int position, int nMove, int nRequest){

    int move = abs(position - request.src) + abs(request.src - request.dst);

    fprintf(f, "Lift-%d Operation\n", id);
    fprintf(f, "Previous position: Floor %d\n", position);
    fprintf(f, "Request: Floor %d to Floor %d\n", request.src, request.dst);
    fprintf(f, "Detail operations:\n");
    
    if (position != request.src){
        fprintf(f, "\tGo from Floor %d to Floor %d\n", position, request.src);
    }

    fprintf(f, "\tGo from floor %d to Floor %d\n", request.src, request.dst);
    fprintf(f, "\t#movement for this request: %d\n", move);
    fprintf(f, "\t#request: %d\n", nRequest + 1);
    fprintf(f, "\tTotal #movement: %d\n", nMove + move);
    fprintf(f, "Current position: Floor %d\n", request.dst);
    fprintf(f, "--------------------------\n");
}


bool hasStopped(void){

    int _tmp;
    pthread_mutex_lock(&stop);
    _tmp = stopIssued;
    pthread_mutex_unlock(&stop);

    return _tmp == 1;
}

