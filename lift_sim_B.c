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

#define N_LIFT 3
#define SEM_FULL_PATH "/sem_full"
#define SEM_EMPTY_PATH "/sem_empty"
#define SEM_MUTEX_PATH "/sem_mutex"
#define SEM_PERMS (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)

void *request(void*);
void *lift(void*);
void logLiftRequest(LiftRequest, int, int, int, int);
void init_sem(int);

int fd_out;
Buffer* buffer = NULL;
sem_t* full;
sem_t* empty;
sem_t* mutex;
int t;

int main(int argc, char **argv){

    int m, i;
    int status = 0;
    pid_t pid[N_LIFT + 1];


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
        exit(EXIT_FAILURE);
    }

        
    /* Open sim ouit file */
    if((fd_out = open(OUTPUT_FILE_PATH, O_CREAT | O_RDWR | O_TRUNC | O_APPEND, 0644)) < 0){
        perror("error openning file");
        exit(EXIT_FAILURE);
    }
    

    /* Create shared buffer */
    buffer = createMMAPBuffer(m);
     
    /* initialise sem */
    init_sem(m);

    #ifdef DEBUG
    printf("forking main... PID = %d.\n", getpid()); 
    #endif

    for( i = 0; i < N_LIFT + 1; i++){
        pid[i] = fork();
        if (pid[i] < 0){
            perror("fork failed");
            exit(EXIT_FAILURE);
        }
        else if (pid[i] == 0) {
            if (i == 0){
                #ifdef DEBUG
                printf("start request. PID = %d, PPID = %d.\n", getpid(), getppid());
                #endif
                request(NULL);
            }
            else{
                #ifdef DEBUG
                printf("start lift-%d. PID = %d, PPID = %d.\n", i, getpid(), getppid());
                #endif
                lift(&i);
            }
            exit(EXIT_SUCCESS); /*prevents child from creating another child */
        }
    }

    /* join processes */
    for ( i = 0; i < N_LIFT + 1; i++){
        waitpid(pid[i], &status, 0);
    }

    /* remove shared memory */
    destroyMMAPBuffer(buffer);

    /* remove semaphores */
    sem_close(mutex);
    sem_close(full);
    sem_close(empty);

    /* close the file */
    if (close(fd_out) < 0){
        perror("error closing file");
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
       
        /* Critical section */
        sem_wait(empty);
        sem_wait(mutex); /* for a 1-producer v. N-consumer, you do not need mutex */

        #ifdef DEBUG
        printf("Adding request { src = %d, dst = %d }.\n", src, dst);
        #endif 
        addRequest(buffer, src, dst);

        /* Write to file */
        sprintf(line, "New Lift Request From Floor %d to Floor %d\n", src, dst);
        sprintf(line + strlen(line), "Request No: %d\n", nRequest);
        sprintf(line + strlen(line), "--------------------------\n");
        
        /* write to fd_out */
        if (lseek(fd_out, 0, SEEK_END) < 0){
            perror("seek error");
        }

        if (write(fd_out, line, strlen(line)) <= 0){
            perror("write error");
        }

        sem_post(mutex);
        sem_post(full);

        /* Non-Critical Operations */
        nRequest ++;
    } 

    /* Closing the input file */
    if (fclose(fin) != 0){
        perror("Error closing input file");
    }

    #ifdef DEBUG
    printf("Request thread done\n");
    #endif

    /* Append an EOF to the buffer to signify that the request process is finished */
    sem_wait(empty);
    sem_wait(mutex);
    addRequest(buffer, EOF, EOF);
    sem_post(mutex);
    sem_post(full);

    return 0;
}


/* lift */
void* lift(void* args){

    LiftRequest req;
    int id = *((int*) args);
    int position = 0;
    int nMove = 0;
    int nRequest = 0;


    for(;;){   

        /* Critical section */
        sem_wait(full);
        sem_wait(mutex);

        req = getRequest(buffer);

        /* if this request siginifes EOF then exits */
        if (req.src == EOF && req.dst == EOF){
            addRequest(buffer, req.src, req.dst); /*add EOF back to the buffer */
            sem_post(mutex);
            sem_post(full);
            break;
        }

        #ifdef DEBUG
        printf("\tLift-%d serving req { src = %d, dst = %d }.\n", id, req.src, req.dst);
        #endif 
        logLiftRequest(req, id, position, nMove, nRequest);

        sem_post(mutex);
        sem_post(empty);

        /* Non-critical section */
        nRequest += 1;
        nMove += abs(position - req.src) + abs(req.src - req.dst);
        position = req.dst;
        usleep(t * 1000);
   }

   #ifdef DEBUG
   printf("Lift-%d thread done\n", id);
   #endif

   return 0;
}


void logLiftRequest(LiftRequest request, int id, int position, int nMove, int nRequest){

    char buffer[512];
    int move = abs(position - request.src) + abs(request.src - request.dst);

    sprintf(buffer, "Lift-%d Operation\n", id);
    sprintf(buffer + strlen(buffer), "Previous position: Floor %d\n", position);
    sprintf(buffer + strlen(buffer), "Request: Floor %d to Floor %d\n", request.src, request.dst);
    sprintf(buffer + strlen(buffer), "Detail operations:\n");
    
    if (position != request.src){
        sprintf(buffer + strlen(buffer), "\tGo from Floor %d to Floor %d\n", position, 
                                                                             request.src);
    }

    sprintf(buffer + strlen(buffer), "\tGo from floor %d to Floor %d\n", request.src, 
                                                                         request.dst);
    sprintf(buffer + strlen(buffer), "\t#movement for this request: %d\n", move);
    sprintf(buffer + strlen(buffer), "\t#request: %d\n", nRequest + 1);
    sprintf(buffer + strlen(buffer), "\tTotal #movement: %d\n", nMove + move);
    sprintf(buffer + strlen(buffer), "Current position: Floor %d\n", request.dst);
    sprintf(buffer + strlen(buffer), "--------------------------\n");

    /* write to fd_out */
    if (lseek(fd_out, 0, SEEK_END) < 0){
        perror("seek error");
    }

    if (write(fd_out, buffer, strlen(buffer)) <= 0){
        perror("write error");
    }
}

void init_sem(int m){

    if((full = sem_open(SEM_FULL_PATH, O_CREAT, SEM_PERMS, 0)) == SEM_FAILED){
        perror("sem_open(3) error on full");
        exit(EXIT_FAILURE);
    }
    sem_init(full, -1, 0);


    if((mutex = sem_open(SEM_MUTEX_PATH, O_CREAT, SEM_PERMS, 1)) == SEM_FAILED){
        perror("sem_open(3) error on mutex");
        exit(EXIT_FAILURE);
    }
    sem_init(mutex, -1, 1);

    if((empty = sem_open(SEM_EMPTY_PATH, O_CREAT, SEM_PERMS, m)) == SEM_FAILED){
        perror("sem_open(3) error on ");
        exit(EXIT_FAILURE);
    }
    sem_init(empty, -1, m);


    #ifdef DEBUG
    printf("Created semaphores {%s, %s, %s}.\n", SEM_EMPTY_PATH, SEM_FULL_PATH, SEM_MUTEX_PATH);
    #endif
}
