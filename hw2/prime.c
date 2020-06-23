/* Author: Sujatha Gopi
 * File Name: prime.c
 * Course: 4760 - Operating Systems
 * Purpose: The goal of this project is to use shared memory and multiple processes to determine if a set of numbers are prime.This file will get inputs from
 * oss.c file and will find if the given number is prime or not */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>
#include "oss.h"
#include <string.h>

#define SHM_KEY 0x062019
#define MAX 50

void sighandler(int SIG_CODE);
struct shared_mem *ptr;
FILE *fp;
char filename[20];

int main (int argc, char *argv[])
{

    signal(SIGINT, sighandler);
    int i, shmid, flag = 1, timer = 0, num;	
    int  milliseconds;
    int cstart_time, logical_id;
    char str[40];

    // process inputs from OSS 
    num = atoi(argv[2]);
    logical_id = atoi(argv[4]);
    strcpy(filename,argv[5]);
    
    // Allocate shared memory with the key defined by macro SHM_KEY
    shmid = shmget(SHM_KEY, sizeof(struct shared_mem), 0644|IPC_CREAT);
    if (shmid == -1) {
       perror("Prime:Shared memory");
       return 1;
    } else {
      // printf("Child %d: Allocation of shared memory Success\n", logical_id);
    }

     //Attach to the memory and get a pointer to it.
    ptr = (struct shared_mem *)shmat(shmid, NULL, 0);
    if (ptr == (void *) -1) {
        perror("Prime:Shared memory attach");
        return 1;
    } else {
        printf("Child %d : Attachment of shared memory Success\n", logical_id);
       // cptr[logical_id] = ptr;
    }

     printf("Child %d execution time starts at %d sec and %u ns\n",logical_id, ptr->sec, ptr->nanosec);
     cstart_time = ptr->nanosec;
     
     /* after noting down the initial time in stimulated clock 
      * the children will now start to calculate primality
      * They will keep checking the clcok during every loop and 
      *  terminate if clock exceed 1 ms */
     for (i = 2; i <= (num / 2); i++)
     {
       milliseconds = abs(( (ptr->nanosec - cstart_time) / 1000000) );
       if (milliseconds > 1)
       {
           timer = 1;
           break;
       }
       if ((num % i) == 0)
       {
        flag = 0;
        break;
        }
     }

    milliseconds = abs(( (ptr->nanosec - cstart_time) / 1000000) );
    if (milliseconds > 1)
    {
           timer = 1;
    }

   
    /* Check the timet flag and prime flag and decide if the number is prime or not
     * write them in the shared memory */
    if ((flag == 1) && ( timer == 0))
    {
        printf("Child %d:  %d is prime\n",logical_id,num);
        ptr->prime[logical_id] = num;
    } else if ((flag == 0) && ( timer == 0))
    {
           num = -num;
           printf("Child %d:  %d is not prime and its factor is %d\n",logical_id,(-num), i);
           ptr->prime[logical_id] = num;
    } else if (timer == 1)
    {
           printf("Child %d: Execution time exceed 1 ms. Not able to determine the primality.\n Child Terminating\n", logical_id);
           ptr->prime[logical_id] = -1;

    }

    // Detacj from shared memory after all calculations are done
    if (shmdt(ptr) == -1) {
      perror("shmdt failed");
      return 1;
    } else {
      printf("Child %d : Detachment of shared memory Success\n", logical_id);
    }

return 0;

}


/* This function will get invoked when we receive CTRL+C and 
 * write the temination time of child in output file and detac the shared memory
 * and termnate the child */
void sighandler(int signal)
{
   int pid;
   char str[50];
   if(signal == SIGINT)
   {
	//shmdt(ptr);
	//
	fp = fopen(filename, "a");
        pid = getpid();	
        sprintf(str, " %d \t\t  Terminated \t\t%d sec and %u ns\n", pid,ptr->sec,ptr->nanosec);
        fputs("\n", fp);
        fputs(str,fp);
	if (shmdt(ptr) == -1) {
           perror("shmdt failed");
        } else {
            printf("Prime : Detachment of shared memory Success\n");
        }

        printf("Process exiting due to interrupt signal.\n");
        exit(EXIT_SUCCESS);
   }
}


