/* Implementation using pthreads */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>

#include "lift.h"
#include "fifo_buf.h"

#define INPUT_FILE_PATH "./sim_input.txt"
#define OUTPUT_FILE_PATH "./sim_out.txt"


FILE* fout = NULL;
FILE* fin = NULL;
fifo_buf_t* buffer = NULL;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t hasSpace = PTHREAD_COND_INITIALIZER;
pthread_cond_t nonEmpty = PTHREAD_COND_INITIALIZER;
useconds_t t;


void* request(void* args){
    
    LiftRequest request;
    char line[512];
    int nRequest = 1;


    /* Read all the request from simulation file */
    while (fgets(line, 512, fin) != NULL){ /* Scanning until EOF */

        int src, dst;

        if( sscanf(line, "%d %d", &src, &dst) != 2 || src < 1 || dst < 1 ){
            fprintf(stderr, "Encountered an invalid lift request.\n");
            continue;
        }

        request.src = src;
        request.dst = dst;

        /* acquire the lock */
        pthread_mutex_lock(&lock);

        /* attempts to add to buffer. Wait for space upon failure. */
        while (fifo_buf_enqueue(buffer, request) == FIFO_BUF_ERR_FULLENQ){;
            pthread_cond_wait(&hasSpace, &lock);
        }

        fprintf(fout, "--------------------------------------------\n");
        fprintf(fout, "New Lift Request From Floor %d to Floor %d\n"\
                      "Request No: %d\n", request.src, request.dst, nRequest);
        fprintf(fout, "--------------------------------------------\n");

        /* Signal waiting threads and release the lock */
        pthread_cond_signal(&nonEmpty);
        pthread_mutex_unlock(&lock);

        /* Non-Critical Operations */
        nRequest ++;
    } 

    /* Signal other threads that there's no item left */
    request.src = -1;
    request.dst = -1;
    pthread_mutex_lock(&lock);
    while (fifo_buf_enqueue(buffer, request) == FIFO_BUF_ERR_FULLENQ){
        pthread_cond_wait(&hasSpace, &lock);
    }
    pthread_cond_signal(&nonEmpty); 
    pthread_mutex_unlock(&lock);



    return 0;
}


/* lift */
void* lift(void* args){

    LiftRequest request;
    int id = *((int*) args);
    int position = 1;
    int nMove = 0;
    int totalMove = 0;
    int nRequest = 0;

    
    for(;;){   
        /* Acquire the lock */
        pthread_mutex_lock(&lock);

        /* Block until theres item in the buffer */
        while (fifo_buf_dequeue(buffer, &request) == FIFO_BUF_ERR_EMPTYDEQ){
            pthread_cond_wait(&nonEmpty, &lock);
        }

        /* If this occurs then the producer has no more item to produce */
        if (request.src == -1 && request.dst == -1){
            fifo_buf_enqueue(buffer, request);
            pthread_cond_signal(&nonEmpty);
            pthread_mutex_unlock(&lock);
            break;
        }

        nRequest += 1;
        nMove = abs(position - request.src) + abs(request.src - request.dst);
        totalMove += nMove;

        /* Log Lift request */
        fprintf(fout, "--------------------------------------------\n");
        fprintf(fout, "Lift-%d Operation\n"\
                      "Previous position: Floor %d\n"\
                      "Request: Floor %d to Floor %d\n"\
                      "Detail operations:\n"\
                      "     Go from Floor %d to Floor %d\n"\
                      "     Go from floor %d to Floor %d\n"\
                      "     #movement for this request: %d\n"\
                      "     Total #movement: %d\n"\
                      "     Current position: Floor %d\n",
        id, position, request.src, request.dst, position, request.src, 
        request.src, request.dst, nMove, totalMove, request.dst);
        fprintf(fout, "--------------------------------------------\n");

        position = request.dst;

        /* Unlock */
        pthread_cond_signal(&hasSpace);
        pthread_mutex_unlock(&lock);

        /* Non-critical Operations */
        usleep(t * 1000);
   }


   return 0;
}


int main(int argc, char **argv){

    int m, i;
    pthread_t tLiftRequest;
    pthread_t tLift[3];


    if (argc != 3){
        printf("%s m t\n\n", argv[0]);
        printf("\tm = buffer size\n");
        printf("\tt = time per request in milliseconds\n");

        exit(EXIT_FAILURE);
       
    }

    m = atoi(argv[1]);
    t = atoi(argv[2]);

    if (m <= 0 || t < 0){
        fprintf(stderr, "Invalid arguments\n");
        
        exit(EXIT_FAILURE);
    }

    fout = fopen(OUTPUT_FILE_PATH, "w");
    fin = fopen(INPUT_FILE_PATH, "r");
    buffer = fifo_buf_init_malloc(m);

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

    /* Clean up  */

    fifo_buf_destroy_malloc(buffer);
    
    if (fclose(fout) != 0){
        perror("Error closing the file\n");
    }    

    if (fclose(fin) != 0){
        perror("Error closing input file");
    }


    return 0;
}

