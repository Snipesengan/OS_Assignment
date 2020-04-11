/* Implementation using pthreads */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>

#include "lift.h"
#include "buffer.h"

#define OUTPUT_FILE_PATH "./sim_out.txt"


void *request(void*);
void *lift(void*);
void logLiftRequest(FILE*, LiftRequest, LiftStatus*);
bool hasStopped(void);

/* Should we make shared data global? */
FILE* fout = NULL;
Buffer* buffer = NULL;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t hasSpace = PTHREAD_COND_INITIALIZER;
pthread_cond_t nonEmpty = PTHREAD_COND_INITIALIZER;
pthread_mutex_t stop = PTHREAD_MUTEX_INITIALIZER;
int stopIssued = 0;


int main(int argc, char **argv){

    int m, i;
    useconds_t t;
    pthread_t tLiftRequest;
    pthread_t tLift[3];
    LiftStatus ls[3];


    if (argc != 3){

        printf("./lift_sim_A m t\n\n");
        printf("\tm = buffer size\n");
        printf("\tt = time per request in milliseconds\n");
    }
    else {

        m = atoi(argv[1]);
        t = atoi(argv[2]);

        if (m < 0 || t < 0){

            fprintf(stderr, "Invalid arguments\n");
            return 0;
        }

        /* Open sim out file */
        fout = fopen(OUTPUT_FILE_PATH, "w");

        /* Initialize buffer */
        buffer = createBuffer(m);

        /* Intialize LiftStatus */
        for (i = 0; i < 3; i ++){

            ls[i].id = i;
            ls[i].position = 0;
            ls[i].nMove = 0;
            ls[i].nRequest = 0;
            ls[i].t = t;
        }

        /* Create thread */
        pthread_create(&tLiftRequest, NULL, request, NULL);
        for (i = 0; i < 3; i ++){

            pthread_create(&tLift[i], NULL, lift, &ls[i]);
        }
        
        /* Join thread (Essentially wait for all thread to finish work)*/
        pthread_join(tLiftRequest, NULL);
        for (i =0; i< 3; i ++){

            pthread_join(tLift[i], NULL);
        }

        /* Close file */
        if (fclose(fout) != 0){

            perror("Error closing the file\n");
        }

        /* Deallocate buffer */
        deallocateBuffer(buffer);
    
    }


    return 0;
}


/* request */
void* request(void* args){
    
    char line[512];
    FILE* fin = fopen("./sim_input.txt", "r");
    int nRequest = 0;


    /* Read all the request from simulation file */
    while (fgets(line, 512, fin) != NULL){

        int src, dst;

        if( sscanf(line, "%d %d", &src, &dst) != 2 ){
            
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
    LiftStatus* liftStatus = (LiftStatus*) args;

    
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
            liftStatus->id, request.src, request.dst);
        #endif
        
        /* Log Lift request */
        logLiftRequest(fout, request, liftStatus);

        /* Unlock */
        pthread_cond_signal(&hasSpace);
        pthread_mutex_unlock(&lock);

        /* Non-critical Operations */
        liftStatus->nRequest += 1;
        liftStatus->nMove += abs(liftStatus->position - request.src);
        liftStatus->nMove += abs(request.src - request.dst);
        liftStatus->position = request.dst;
        usleep(liftStatus->t * 1000);
   }

   #ifdef DEBUG
   printf("Lift-%d thread done\n", liftStatus->id);
   #endif

   return 0;
}


void logLiftRequest(FILE* f, LiftRequest request, LiftStatus* liftStatus){

    int nMove = abs(liftStatus->position - request.src) + abs(request.src - request.dst);
    int nRequest = liftStatus->nRequest + 1;


    fprintf(f, "Lift-%d Operation\n", liftStatus->id);
    fprintf(f, "Previous position: Floor %d\n", liftStatus->position);
    fprintf(f, "Request: Floor %d to Floor %d\n", request.src, request.dst);
    fprintf(f, "Detail operations:\n");
    
    if (liftStatus->position != request.src){
        fprintf(f, "\tGo from Floor %d to Floor %d\n", liftStatus->position, request.src);
    }

    fprintf(f, "\tGo from floor %d to Floor %d\n", request.src, request.dst);
    fprintf(f, "\t#movement for this request: %d\n", nMove);
    fprintf(f, "\t#request: %d\n", nRequest);
    fprintf(f, "\tTotal #movement: %d\n", liftStatus->nMove + nMove);
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

