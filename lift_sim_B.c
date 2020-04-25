/* Implementation using pthreads */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/wait.h>

#include "lift.h"
#include "buffer.h"

/* name of shared memory object */
#define SHMCOUNT_PATH "/count_seg"
#define SHMHEAD_PATH "/head_seg"
#define SHMBUFF_PATH "/buffer_seg"
#define INPUT_FILE_PATH "./sim_input.txt"
#define OUTPUT_FILE_PATH "./sim_out.txt"

int request(int, int*, int*, LiftRequest*);
void *lift(void*);
void logLiftRequest(FILE*, LiftRequest, int, int, int, int);
bool hasStopped(void);


int main(int argc, char **argv){

    int m, t;
    size_t seg_size;
    int status = 0;
    pid_t pid;

    int shmfd_buffer, shmfd_count, shmfd_head;
    LiftRequest* buff_ptr;
    int *head_ptr, *count_ptr;
    Buffer* buffer;


    if (argc != 3){
        printf("./lift_sim_A m t\n\n");
        printf("\tm = buffer size\n");
        printf("\tt = time per request in milliseconds\n");
        exit(0);
    }

    /* parse input */
    m = atoi(argv[1]);
    t = atoi(argv[2]);

    if (m <= 0 || t <= 0){
        fprintf(stderr, "Invalid arguments\n");
        exit(1);
    }

    /* Creating the share memory objects */
    /* flags: 
        O_CREAT: Create the share memory objet if it does not exists.
        O_RWDR: Open the object for read-write access
        O_TRUNC: If the share memory object already exists, truncate it to zero bytes

        mode:
        S_IRWXU: User Read/Write/Execute priviliege
        S_IRWXG: Group Read/Write/Execute priviliege
    */
    shmfd_buffer = shm_open(SHMBUFF_PATH, O_CREAT | O_RDWR | O_TRUNC, S_IRWXU | S_IRWXG);
    shmfd_count = shm_open(SHMCOUNT_PATH, O_CREAT | O_RDWR | O_TRUNC, S_IRWXU | S_IRWXG);
    shmfd_head = shm_open(SHMHEAD_PATH, O_CREAT | O_RDWR | O_TRUNC, S_IRWXU | S_IRWXG);


    /* adjust the mapped file size */
    seg_size = m * sizeof(LiftRequest);
    ftruncate(shmfd_count, sizeof(int));
    ftruncate(shmfd_head, sizeof(int));
    ftruncate(shmfd_buffer, seg_size);
    
    #ifdef DEBUG
    printf("Created share memory buffer %s, and share count %s, and shared head %s.\n",
        SHMBUFF_PATH, SHMCOUNT_PATH, SHMHEAD_PATH);
    #endif

    /* requesting the share segment */
    buff_ptr = (LiftRequest*) mmap(NULL, 
                                   seg_size, 
                                   PROT_READ | PROT_WRITE, 
                                   MAP_SHARED | MAP_ANONYMOUS, shmfd_buffer, 0);

    head_ptr = (int*) mmap(NULL, 
                           sizeof(int), 
                           PROT_READ | PROT_WRITE, 
                           MAP_SHARED | MAP_ANONYMOUS, shmfd_head, 0);

    count_ptr = (int*) mmap(NULL, 
                            sizeof(int), 
                            PROT_READ | PROT_WRITE, 
                            MAP_SHARED | MAP_ANONYMOUS, shmfd_count, 0);

    /* initialize count and head of the buffer*/
    *head_ptr = 0;
    *count_ptr = 0;

    
    /* create the first child process */
    pid = fork();
    if (pid == 0)
    { /* child process */
        #ifdef DEBUG
        printf("child PID = %d, parent pid = %d\n", getpid(), getppid());
        #endif

        request(m, head_ptr, count_ptr, buff_ptr);

    }
    else if (pid < 0)
    {   /* fork failed */
        perror("fork failed");
        status = -1;
    }
    else
    {   /* parent process */

        #ifdef DEBUG
        printf("parent PID = %d, child pid = %d\n", getpid(), pid); 
        #endif

        if (waitpid(pid, &status, 0) != pid){ status = -1; }

        #ifdef DEBUG
        printf("Child exit code: %d\n", WEXITSTATUS(status));
        #endif

        /* print buffer for debugging */
        while (*count_ptr > 0){

            LiftRequest request = buff_ptr[*head_ptr]; 
            printf("src = %d, dst = %d\n", request.src, request.dst);

            *head_ptr = (*head_ptr + 1) % m;
            *count_ptr = *count_ptr - 1;
        }
    }

    return status;
}


int request(int m, int* head, int* count, LiftRequest* buffer){
    
    char line[512];
    FILE* fin = fopen(INPUT_FILE_PATH, "r");
    int nRequest = 1;

    /* Read all the request from simulation file */
    while (fgets(line, 512, fin) != NULL){ /* Scanning until EOF */

        int src, dst;
        int tail;

        if( sscanf(line, "%d %d", &src, &dst) != 2 || src < 1 || dst < 1 ){
            fprintf(stderr, "Encountered an invalid lift request, choosing to ignore\n");
            continue;
        }


        #ifdef DEBUG
        printf("Attempting to add new request\n");
        #endif
       
        tail = (*head + *count) % m;

        #ifdef DEBUG
        printf("\tAdding request { src = %d, dst = %d } to position %d in buffer.\n", 
            src, dst, tail);
        #endif 
        
        buffer[tail].src = src;
        buffer[tail].dst = dst;
        *count = *count + 1;

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

        #ifdef DEBUG
        printf("Attempting to remove from buffer\n");
        #endif


        #ifdef DEBUG
        printf("\tLift-%d serving request { src = %d, dst = %d }\n", 
            id, request.src, request.dst);
        #endif
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
