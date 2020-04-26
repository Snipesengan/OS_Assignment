/* Implementation using pthreads */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <semaphore.h>

#include "lift.h"
#include "buffer.h"

#define INPUT_FILE_PATH "./sim_input.txt"
#define OUTPUT_FILE_PATH "./sim_out.txt"

#define SEM_FULL_PATH "/sem_full"
#define SEM_EMPTY_PATH "/sem_empty"
#define SEM_MUTEX_PATH "/sem_mutex"
#define SEM_PERMS (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)

void *request(void*);
void *lift(void*);
void logLiftRequest(FILE*, LiftRequest, int, int, int, int);
void init_sem(int);


FILE* fout = NULL;
Buffer* buffer = NULL;
sem_t* full;
sem_t* empty;
sem_t* mutex;
int stopIssued;

int main(int argc, char **argv){

    int m, t;
    int status = 0;
    pid_t r_pid, l_pid;


    if (argc != 3){
        printf("./lift_sim_A m t\n\n");
        printf("\tm = buffer size\n");
        printf("\tt = time per request in milliseconds\n");
        exit(EXIT_FAILURE);
    }

    /* parse input */
    m = atoi(argv[1]);
    t = atoi(argv[2]);

    if (m <= 0 || t <= 0){
        fprintf(stderr, "Invalid arguments\n");
        exit(EXIT_SUCCESS);
    }

    /* Create shared buffer */
    buffer = createMMAPBuffer(m);
    
    /* initialise sem */
    init_sem(m);

    /* create the first child process */
    r_pid = fork();
    if (r_pid == 0)
    { /* child process */

        printf("start request. PID = %d, parent pid = %d.\n", getpid(), getppid());
        request(NULL);
    }
    else if (r_pid < 0)
    {   /* fork failed */
        perror("request fork failed");
        status = -1;
    }
    else
    {   /* parent process */

        #ifdef DEBUG
        printf("fork() main. PID = %d, child pid = %d\n", getpid(), r_pid); 
        #endif 
        
        l_pid = fork();
        if (l_pid == 0){
            printf("start lift. PID = %d, parent pid = %d.\n", getpid(), getppid());
            lift(NULL);
        }
        else if (l_pid < 0){
            perror("lift fork failed");
            status = -1;
        }
        else{
            waitpid(r_pid, &status, 0);
            waitpid(l_pid, &status, 0);
        }
    }

    return status;
}


void* request(void* arg){
    
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
       
        sem_wait(empty);
        sem_wait(mutex);

        #ifdef DEBUG
        printf("Attempting to add new request\n");
        #endif


        #ifdef DEBUG
        printf("\tAdding request { src = %d, dst = %d }.\n", src, dst);
        #endif 
            
        addRequest(buffer, src, dst);

        sem_post(mutex);
        sem_post(full);

        /* Non-Critical Operations */
        nRequest ++;
        sleep(1);
    } 

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
    int id = 1;
    int position = 0;
    int nMove = 0;
    int nRequest = 0;

    
    for(;;){   

        sem_wait(full);
        sem_wait(mutex);

        #ifdef DEBUG
        printf("Attempting to remove from buffer\n");
        #endif

        printf("buffer count %d.\n", buffer->count);
        request = getRequest(buffer);

        #ifdef DEBUG
        printf("\tLift-%d serving request { src = %d, dst = %d }\n", 
            id, request.src, request.dst);
        #endif

        sem_post(mutex);
        sem_post(empty);
        sleep(1);
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

void init_sem(int m){

    if((full = sem_open(SEM_FULL_PATH, O_CREAT, SEM_PERMS, 0)) == SEM_FAILED){
        perror("sem_open(3) error on full");
        exit(EXIT_FAILURE);
    }

    if((mutex = sem_open(SEM_MUTEX_PATH, O_CREAT, SEM_PERMS, 1)) == SEM_FAILED){
        perror("sem_open(3) error on mutex");
        exit(EXIT_FAILURE);
    }

    if((empty = sem_open(SEM_EMPTY_PATH, O_CREAT, SEM_PERMS, m)) == SEM_FAILED){
        perror("sem_open(3) error on ");
        exit(EXIT_FAILURE);
    }

    #ifdef DEBUG
    printf("Created semaphores {%s, %s, %s}.\n", SEM_EMPTY_PATH, SEM_FULL_PATH, SEM_MUTEX_PATH);
    #endif
}
