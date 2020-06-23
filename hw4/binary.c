/* Author: Sujatha Gopi
 * File Name: binary.c
 * Course: 4760 - Operating Systems
 * Purpose: This project simulates the process scheduling part of an operating system. This will implement time-based scheduling,
 * using message queues for synchronization. This file binary.c will act as process*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <signal.h>
#include <sys/msg.h>
#include <stdbool.h>
#include <time.h>
#include "oss.h"


struct shared_time *bn_timeptr; // pointer to shared memory
struct pcb *bn_pcbptr = pcb_t;
int bn_msgid;

void sighandler(int SIG_CODE);


int main (int argc, char *argv[])
{

    /* signal handler for CTRLC and alarm */
    signal(SIGINT, sighandler);

    int bn_timeid, bn_pcbid;
    int av_pid, result, quantum, random_choice, choice;
    unsigned int event_wait_time = 0, burst_time = 0, run_time = 0, preempt_remain_time = 0, r_sec = 0, r_ns = 0, sec, ns, balance = 0, run_time_dup;
    struct mesg_buffer mesg;
    int preempt_complete = 0;
    int preempt_percentage;

    srand(getpid()); //Seeding
    // Allocate shared memory with the key defined by macro SHM_KEY
    // If any problem with memory allocation throw error
    bn_timeid = shmget(SHM_KEY, sizeof(struct shared_time), 0644|IPC_CREAT);
    if (bn_timeid == -1) {
              perror("./binary: Shared memory creation failed");
              return 1;
    } else {
             // printf("Binary: Allocation of shared memory Success\n");
    }


    //Attach to the memory and get a pointer to it.
    bn_timeptr = (struct shared_time *)shmat(bn_timeid, NULL, 0);
    if (bn_timeptr == (void *) -1) {
              perror("./binary: Shared memory attach failed");
              return 1;
    } else {
             // printf("Binary : Attachment of shared memory Success\n");
    }

    // Allocate shared memory for process control block with the key defined by macro PCB_KEY
    bn_pcbid = shmget(PCB_KEY, (sizeof(struct pcb)*(NUM_PCB)), 0644|IPC_CREAT);
    if (bn_pcbid == -1) {
              perror("./binary: Shared memory creation failed");
              return 1;
    } else {
            //  printf("OSS: Allocation of shared memory Success\n");

    }

    bn_pcbptr = (struct pcb *)shmat(bn_pcbid, NULL, 0);
    if (bn_pcbptr == (void *) -1) {
              perror("./binary: Shared memory attach failed");
              return 1;
    } else {
            //  printf("OSS : Attachment of shared memory Success\n");
    }

    
    bn_msgid = msgget(MSG_KEY, 0600| IPC_CREAT);
    if (bn_msgid == -1) {
          perror("./binary: Message queue creation failed");
          return 1;
    } else {
          printf("Binary: Creation of message queue success\n");
    }


    av_pid = atoi(argv[1]);

    /* Hold the process so that they do not terminate. Process goies to termination only as scheduled options */
    while(1)
    {
       while(1)
       {

                preempt_complete = 0;
      		//Process checks if it is its turn to execute by checking the message from OSS
    		result = msgrcv(bn_msgid, &mesg, (sizeof(mesg.mesg_value)), (av_pid + 1), 0);
    		if(result != -1)
    		{
             		//printf("This message is for process %d\n", getpid());
             		quantum = mesg.mesg_value;
           		 //Decide if thw process will use the entire quantum, or only a part of it
             		//If it has to use only a part of the quantum, it will generate a random number in the range [0, quantum] to see how long it runs.
         		 if ((rand() % 2) == 0) 
		                  burst_time = quantum;  // process uses full quantum
		         else {
                		  printf("Process will use only a part of quantum\n");  // Process uses a part of the quantum
              			  burst_time = ((rand() % (quantum - 0 + 1)) + 0);
              	                  printf("Process will use time %d\n", burst_time);
             		}		


                        balance = bn_pcbptr[av_pid].preempt_balance_nanosecond;
                        bn_pcbptr[av_pid].preempt_complete = 0;
                        /* To decide how a process should be scheduled, we generate a random number between [0 - 3].
                         * [0] to indicate that the process will terminate.
                         * [1] to indicate that the process will use its entire time slice
                         * [2] to indicate that the process will be blocked on a event. The process will wait for an event that will last for r.s seconds, 
                         * where r and s are random numbers with range [0, 3] and [0, 1000] respectively.
                         * [3] to indicate that the process gets preempted after using p % of its assigned quantum, where p is a random number in the range [1, 99]. */
                         random_choice = (rand() % (3 - 0 + 1)) + 0;
                         if(random_choice == 0)   //process decides to terminate
	                 {
                               run_time = burst_time;
                               event_wait_time = 0;  
                               preempt_remain_time = 0;       
                               choice = 0;
	                 } 
                         
                         if ((random_choice == 1) || (balance > 0))  //process will use its entire time slice
                         {
                               run_time = quantum;
                               event_wait_time = 0;
                               burst_time = quantum;
                               preempt_remain_time = 0;
                               choice = 1;
                               if(balance > 0)    //preempted process reentry - to make them exhaust quantum
                                   preempt_complete = 1;
                              
                        }  

                        if (random_choice == 2)   //process will be blocked on a event
                        {

                               r_sec = (rand() % (3 - 0 + 1));
                               r_sec = r_sec * 1000000000;
		               r_ns = (rand() % (1000 - 0 + 1));

                               event_wait_time = r_sec + r_ns;
                               run_time = burst_time;
                               preempt_remain_time = 0;
                               choice = 2;

                        } 
            
                        if ((random_choice == 3) && (balance == 0))    //process gets preempted after using p % of its assigned quantum
                        {
	           	      preempt_percentage = (rand() % 100);
                              run_time = (preempt_percentage * burst_time) / 100;
                              event_wait_time = 0;
                              preempt_remain_time = burst_time - run_time;
                              choice = 3;
                        }	


                        //Run the process up to the given duration
                        
                        run_time_dup = run_time;
                        while(run_time_dup > 0)
                              run_time_dup = run_time_dup - 1;
                        
                         
                        /*Save the calculated time in pcb */
                        if(burst_time >= 1000000000)
                        {
                              sec = burst_time/1000000000;
                              ns = burst_time % 1000000000;
                        } else {
                              sec = 0;
                              ns = burst_time;
                        }
                        bn_pcbptr[av_pid].burst_second = sec;
                        bn_pcbptr[av_pid].burst_nanosecond = ns;


                        if(run_time >= 1000000000)
                        {
                              sec = run_time / 1000000000;
                              ns = run_time % 1000000000;
                        } else {
                              sec = 0;
                              ns = run_time;
                        }
                        bn_pcbptr[av_pid].run_second = sec;
                        bn_pcbptr[av_pid].run_nanosecond = ns;
                        bn_pcbptr[av_pid].childpid = getpid();


                        if(event_wait_time == 0)
                        {
                           bn_pcbptr[av_pid].event_wait_second = 0;
                           bn_pcbptr[av_pid].event_wait_nanosecond = 0;
                           bn_pcbptr[av_pid].block_second = 0;
                           bn_pcbptr[av_pid].block_nanosecond = 0;

                        } else {
                            if(event_wait_time >= 1000000000)
                            {
                              sec = event_wait_time / 1000000000;
                              ns = event_wait_time % 1000000000;
                            } else {
                              sec = 0;
                              ns = event_wait_time;
                            }

                            bn_pcbptr[av_pid].event_wait_second = sec;
                            bn_pcbptr[av_pid].event_wait_nanosecond = ns;
                            bn_pcbptr[av_pid].block_second = bn_timeptr->sec;
                            bn_pcbptr[av_pid].block_nanosecond = bn_timeptr->nanosec;

                        }


                        bn_pcbptr[av_pid].preempt_balance_nanosecond = preempt_remain_time;
                        if(preempt_complete == 1)
                           bn_pcbptr[av_pid].preempt_complete = 1;


                       // printf(".burst_second = %d,.burst_nanosecond = %d, run_second = %d, run_nanosecond = %d\n", bn_pcbptr[av_pid].burst_second, bn_pcbptr[av_pid].burst_nanosecond, bn_pcbptr[av_pid].run_second, bn_pcbptr[av_pid].run_nanosecond);
                         //printf(".wait_second = %d,.wait_nanosecond = %d, block_second = %d, block_nanosecond = %d\n", bn_pcbptr[av_pid].event_wait_second, bn_pcbptr[av_pid].event_wait_nanosecond, bn_pcbptr[av_pid].block_second, bn_pcbptr[av_pid].block_nanosecond);

                        
                        //Send a message to master about scheduling decision made by the process
                        mesg.mesg_type = av_pid + 100;
                        mesg.mesg_value = choice;

                        if (msgsnd(bn_msgid, &mesg, sizeof(mesg.mesg_value), 0) == -1) {
     			   perror("./binary: Error: msgsnd ");
     			   exit(EXIT_FAILURE);
    	                }
 
                 break;
          }  // end if

      }   // end innermost while
          
   
     if(choice == 0)

     {
           /*process goes to termination only if choice 0 is chosen */
           /* Detach and delete shared memory */ 
           if (shmdt(bn_timeptr) == -1) {
              perror("shmdt failed");
              return 1;
           } else {
               printf("Binary : Detachment of shared memory Success\n");
           }

          if (shmdt(bn_pcbptr) == -1) {
              perror("shmdt failed");
              return 1;
          } else {
              printf("Binary : Detachment of shared memory Success\n");
          } 

    }
 
    if (choice == 0 )
    {
           break;
    }
 
}  // end outer while   

return 0;
}


void sighandler(int signal)
{
   if(signal == SIGINT)
   {
      if (shmdt(bn_timeptr) == -1) {
           perror("shmdt failed");
        } else {
            printf("Binary: Process %d : Detachment of shared memory Success\n", getpid());
        }

      if (shmdt(bn_pcbptr) == -1) {
            perror("shmdt failed");
    } else {
            printf("Binary : Detachment of shared memory Success\n");
    }

        printf("Process %d exiting due to interrupt signal.\n", getpid());
        exit(EXIT_SUCCESS);
   }
}

