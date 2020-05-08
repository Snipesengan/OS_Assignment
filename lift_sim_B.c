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
#include "fifo_buf.h"

#define INPUT_FILE_PATH "./sim_input.txt"
#define OUTPUT_FILE_PATH "./sim_out.txt"


#define N_LIFT 3
#define SEM_FULL_PATH "/sem_full"
#define SEM_EMPTY_PATH "/sem_empty"
#define SEM_MUTEX_PATH "/sem_mutex"
#define SEM_PERMS (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)

int fd_out;
FILE* fin;
fifo_buf_t* buffer = NULL;
sem_t* full;
sem_t* empty;
sem_t* mutex;
int t;

void init_sem(sem_t** sem, int initial){

    int protection = PROT_READ | PROT_WRITE;   
    int visibility = MAP_SHARED | MAP_ANONYMOUS;

    *sem = (sem_t*) mmap(NULL, 
                         sizeof(sem_t),
                         protection, visibility, -1, 0);     
    sem_init(*sem, -1, initial);
}


void* request(void* arg){
    
    LiftRequest request;
    char line[512];
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

        request.src = src;
        request.dst = dst;
        fifo_buf_enqueue(buffer, request);

        /* Write to file */
        sprintf(line,                "--------------------------------------------\n");
        sprintf(line + strlen(line), "New Lift Request From Floor %d to Floor %d\n"\
                                     "Request No: %d\n", request.src, request.dst, nRequest);
        sprintf(line + strlen(line), "--------------------------------------------\n");
        
        /* write to fd_out */
        lseek(fd_out, 0, SEEK_END);
        write(fd_out, line, strlen(line));

        sem_post(mutex);
        sem_post(full);

        /* Non-Critical Operations */
        nRequest ++;
    } 

    /* Append an EOF to the buffer to signify that the request process is finished */
    sem_wait(empty);
    sem_wait(mutex);
    request.src = EOF;
    request.dst = EOF;
    fifo_buf_enqueue(buffer, request);
    sem_post(mutex);
    sem_post(full);

    return 0;
}


/* lift */
void* lift(void* args){

    LiftRequest request;
    int id = *((int*) args);
    int position = 1;
    int totalMove = 0;
    int nMove = 0;
    int nRequest = 0;
    char line[512];


    for(;;){   

        /* Critical section */
        sem_wait(full);
        sem_wait(mutex);

        fifo_buf_dequeue(buffer, &request);

        /* if this request siginifes EOF then exits */
        if (request.src == EOF && request.dst == EOF){
            fifo_buf_enqueue(buffer, request);
            sem_post(mutex);
            sem_post(full);
            break;
        }

        nRequest += 1;
        nMove = abs(position - request.src) + abs(request.src - request.dst);
        totalMove += nMove;

        /* Log lift request */
        sprintf(line,                 "--------------------------------------------\n");
        sprintf(line + strlen(line),  "Lift-%d Operation\n"\
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
        sprintf(line + strlen(line) , "--------------------------------------------\n");

        /* Write to end of the file */
        lseek(fd_out, 0, SEEK_END);
        write(fd_out, line, strlen(line));

        position = request.dst; /* Updates the position */

        sem_post(mutex);
        sem_post(empty);

        /* Non-critical section */
        usleep(t * 1000);
   }

   return 0;
}


int main(int argc, char **argv){

    int m, i;
    int status = 0;
    pid_t pid[N_LIFT + 1];


    if (argc != 3){
        printf("%s m t\n\n", argv[0]);
        printf("\tm = buffer size\n");
        printf("\tt = time per request in milliseconds\n");
        exit(EXIT_FAILURE);
    }

    /* parse input */
    m = atoi(argv[1]);
    t = atoi(argv[2]);

    if (m <= 0 || t < 0){
        fprintf(stderr, "Invalid arguments\n");
        exit(EXIT_FAILURE);
    }

        
    /* Open sim out file */
    if((fd_out = open(OUTPUT_FILE_PATH, O_CREAT | O_RDWR | O_TRUNC | O_APPEND, 0644)) < 0){
        perror("error openning file");
        exit(EXIT_FAILURE);
    }

    /* Open sim in file */
    fin = fopen(INPUT_FILE_PATH, "r");
    
    /* Create shared buffer */
    buffer = fifo_buf_init_mmap(m); 

    /* initialise sem */
    init_sem(&full, 0);
    init_sem(&empty, m);
    init_sem(&mutex, 1);

    for( i = 0; i < N_LIFT + 1; i++){
        pid[i] = fork();
        if (pid[i] < 0){
            perror("fork failed");
            exit(EXIT_FAILURE);
        }
        else if (pid[i] == 0) {
            if (i == 0){
                request(NULL);
            }
            else{
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
    fifo_buf_destroy_mmap(buffer);

    /* remove semaphores */
    munmap(mutex, sizeof(sem_t));
    munmap(full, sizeof(sem_t));
    munmap(empty, sizeof(sem_t));

    /* close output file */
    if (close(fd_out) < 0){
        perror("error closing file");
    }
    
    /* Closing the input file */
    if (fclose(fin) != 0){
        perror("Error closing input file");
    }

    return status;
}
