/* Author: Sujatha Gopi
 * File Name: bin_adder.c
 * Course: 4760 - Operating Systems
 * Purpose: This project computes the summation of n integers in two different ways and assess their
 *  performance. This project uses semaphore and shared memory to impement this*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>
#include <sys/sem.h>
#include <sys/ipc.h>
#include <fcntl.h>
#include "master.h"

#define SHM_KEY 0x032011
#define SEM_KEY 0x061819

void sighandler(int SIG_CODE);
void file_update(int semid, int index, int increment, int sum);

struct shared_mem *ptr;

int main(int argc, char *argv[])
{

    /* signal handler for CTRLC and alarm */
    signal(SIGINT, sighandler);

    int shmid, i, semid;
    int index, increment, sum = 0;


    // process inputs from master
    index = atoi(argv[1]);
    increment = atoi(argv[2]);
    //printf("start num = %d and increment is %d\n", index, increment);


    // Allocate shared memory with the key defined by macro SHM_KEY
    shmid = shmget(SHM_KEY, sizeof(struct shared_mem), 0644|IPC_CREAT);
    if (shmid == -1) {
       perror("bin_adder:Shared memory");
       return 1;
    } else {
     // printf("Child : Allocation of shared memory Success\n");
    }


    //Attach to the memory and get a pointer to it.
    ptr = (struct shared_mem *)shmat(shmid, NULL, 0);
    if (ptr == (void *) -1) {
        perror("Bin_addr:Shared memory attach");
        return 1;
    } else {
        printf("Process %d: Attachment of shared memory Success\n", getpid());

    }

    /* Access semaphores that was created by master process*/
    semid = semget(SEM_KEY,1,IPC_CREAT|0666);
    if (semid == -1) {
        perror("./bin_adder: Failed to access semaphore");
        exit(EXIT_FAILURE);
    }


    /* Add numbers from index position till index + increment position 
     * store the result in index position 
     * Make the contents of other positions except index to 0*/
    for(i = 0; i < increment; i ++)
    {
       //printf("Adding %d to sum \n", ptr->array[index + i]);
       sum = sum + ptr->array[index + i];
       ptr->array[index + i] = 0;
       
    }
    
    ptr->array[index] = sum;
 
    //printf("Added value is  %d \n", ptr->array[index]);
   
    file_update(semid, index, increment, sum);
   
    // Detach from shared memory after all calculations are done
    if (shmdt(ptr) == -1) {
      perror("shmdt failed");
      return 1;
    } else {
      printf("Process %d: Detachment of shared memory Success\n", getpid());
    }

return 0;
}


/* This function uses semaphore to update the log file about the actions of child process*/
void file_update(int semid, int index, int increment, int sum)
{

    struct sembuf semsignal[1];
    struct sembuf semwait[1];
    int semerror;
    time_t current_time;
    char* c_time_string;
    FILE *fp;

    /* Initialize semwait and semsignal */
    semwait->sem_num = 0;
    semwait->sem_op = -1;
    semwait->sem_flg = 0;

    semsignal->sem_num = 0;
    semsignal->sem_op = 1;
    semsignal->sem_flg = 0;

    // Critical Section entry : Process sends semwait to acquire semaphore
    semerror = semop(semid, semwait, 1);
    if (semerror == -1) {
        perror("./bin_adder:Failed to lock semaphore");
      //  exit(EXIT_FAILURE);
    }
    current_time = time(NULL);
    c_time_string = ctime(&current_time);
    
    //Process enters critical section
    fprintf(stderr, "PID %d enters critical section at %s \n", getpid(),c_time_string);

    srand(time(0));

    #ifdef DEBUG 
       sleep((rand() % (3 - 1 + 1)) + 1);
    #endif
    //printf("PID %d acquired semaphore\n", getpid());
    
    /* open adder_log and write the actions done by this process */
    fp = fopen("adder_log","a");
    if (fp == NULL){
        perror("Could not open file adder_log\n");
         exit(EXIT_FAILURE);
    }
    current_time = time(NULL);
    c_time_string = ctime(&current_time);

    fprintf(fp, "\t%d\t\t %d\t\t %d\t\t %d\t\t\t\t %s\t\t\n", getpid(), index, increment, sum, c_time_string);
    fclose(fp); 
    #ifdef DEBUG
       sleep(1);
    #endif
  
    /* Release semaphore after finishing with file update */
    semerror = semop(semid, semsignal, 1);
    if (semerror == -1) {
        perror("./bin_adder:Failed to unlock semaphore");
    }    

    current_time = time(NULL);
    c_time_string = ctime(&current_time);

    //Process enters critical section
    fprintf(stderr, "PID %d leaves critical section at %s \n", getpid(),c_time_string);


}

/* This function will get invoked when we receive CTRL+C and
 * write the temination time of child in output file and detach the shared memory
 * and termnate the child */

void sighandler(int signal)
{
   if(signal == SIGINT)
   {
      if (shmdt(ptr) == -1) {
           perror("shmdt failed");
        } else {
            printf("Bin_adder: Process %d : Detachment of shared memory Success\n", getpid());
        }

        printf("Process %d exiting due to interrupt signal.\n", getpid());
        exit(EXIT_SUCCESS);
   }
}

