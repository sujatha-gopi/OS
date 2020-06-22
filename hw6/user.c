/* Author: Sujatha Gopi
 * File Name: user.c
 * Course: 4760 - Operating Systems
 * Purpose:  This file acts as process requesting to READ and WRITE from memory at random times 
*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <signal.h>
#include <sys/msg.h>
#include <stdbool.h>
#include <time.h>
#include <sys/sem.h>
#include <sys/ipc.h>
#include <string.h>
#include "oss.h"


struct shared_time *u_timeptr; // pointer to shared memory
struct pcb *u_pcbptr = pcb_t;
void inc_timer(int semid, int increment);
void sighandler(int signal);

int av_pid;

int main (int argc, char *argv[])
{

        /* signal handler for CTRLC and alarm */
        signal(SIGINT, sighandler);
     
        int u_timeid, u_pcbid, u_semid, u_msgid;
        int i, mem_ref, memory_address, page_num;
        struct mesg_buffer mesg;
        float page_weight[MAX_PAGE], random_number;
        int action_request, result;
        int no_of_memory_ref;

        srand (getpid()); //Seeding

        // Allocate shared memory with the key defined by macro SHM_KEY
        // If any problem with memory allocation throw error
        u_timeid = shmget(SHM_KEY, sizeof(struct shared_time), 0644|IPC_CREAT);
        if (u_timeid == -1) {
                 perror("./user: Shared memory creation failed");
                 return 1;
        }

        //Attach to the memory and get a pointer to it.
        u_timeptr = (struct shared_time *)shmat(u_timeid, NULL, 0);
        if (u_timeptr == (void *) -1) {
              perror("./user: Shared memory attach failed");
              return 1;
        }

        // Allocate shared memory for process control block with the key defined by macro PCB_KEY
        u_pcbid = shmget(PCB_KEY, (sizeof(struct pcb)*(MAX_CONCURRENT)), 0644|IPC_CREAT);
        if (u_pcbid == -1) {
              perror("./oss: Shared memory creation failed");
              return 1;
        }

        //Attach to the memory and get a pointer to it.
        u_pcbptr = (struct pcb *)shmat(u_pcbid, NULL, 0);
        if (u_pcbptr == (void *) -1) {
              perror("./oss: Shared memory attach failed");
              return 1;
        }
        /* Access semaphores that was created by master process*/
        u_semid = semget(SEM_KEY,1,IPC_CREAT|0666);
        if (u_semid == -1) {
             perror("./user: Failed to access semaphore");
             exit(EXIT_FAILURE);
        }

        u_msgid = msgget(MSG_KEY, 0600| IPC_CREAT);
        if (u_msgid == -1) {
            perror("./user: Message queue creation failed");
            return 1;
        }

        av_pid = atoi(argv[1]);
        mem_ref = atoi(argv[2]);
        u_pcbptr[av_pid].realpid = getpid();
        mesg.pid = av_pid;
        mesg.mesg_type = 1;

        while(1)
        {            
                  /* Check if process should terminate after 1100 memory reference request are generated */
                  if(no_of_memory_ref > 1100)
                  {

                       if ((rand() % 100) <= 40) //Process decides to terminate
                       {

                                 inc_timer(u_semid, 1000000);
                                 mesg.pid = av_pid;
                                 mesg.mesg_type = 1;
                                 mesg.mesg_value = 2;  //Terminate
                                 if (msgsnd(u_msgid, &mesg, sizeof(mesg), 0) == -1) {
                                           perror("./user: Error: msgsnd ");
                                           exit(EXIT_FAILURE);
                                 }
        
                                 break;
                       } // endif rand()
                 }

                 /* Memory reference type gets passed from the OSS */         
                 if(mem_ref == 0)
                 {

                       memory_address =  (rand() % 32768);
                       page_num = (memory_address / 1024);

                 } else {   // This type selects the page by having some weight to each page
                      for(i = 0; i < MAX_PAGE; i ++)
                      {
                             page_weight[i] = (float) 1 / (float) (i + 1);
                             //  printf("%f ", page_weight[i]);
                      }
                      for(i = 1; i <= MAX_PAGE; i ++)
                      {  
                             page_weight[i] = page_weight[i] + page_weight[i-1];
                            // printf("%f ", page_weight[i]);
                      }

                                
                      random_number = ((float)rand() / RAND_MAX) * (float)(4.0);
                      for(i = 0; i < MAX_PAGE; i ++)
                      {
                              if(page_weight[i] > random_number)
                              {
                                        page_num = i;
                                        break;
                              }
                      }

                      memory_address = page_num * 1024;
                      memory_address = (memory_address + (rand() % 1024));

                } // end memory ref if
                    
                //Selects READ / WRITE 
                if((rand() % 10) < 7)
                {
                      action_request =  0; //Process decides to READ from memory
                } else {
                      action_request =  1; //Process decides to WRITE to memory
                }

                   
               /* Send request to OSS */
                mesg.pid = av_pid;
                mesg.mesg_type = 1;
                mesg.page_num = page_num;
                mesg.mem_addr = memory_address;
                mesg.mesg_value = action_request;  //READ or WRITE Request

                if (msgsnd(u_msgid, &mesg, sizeof(mesg), 0) == -1) {
                          perror("./user: Error: msgsnd READ/WRITE");
                          exit(EXIT_FAILURE);
                }
      
                no_of_memory_ref++;

                // wait for message from oss
                while(1) {
                     int flag = 0;
                     result = msgrcv(u_msgid, &mesg, (sizeof(mesg)), getpid(), 0);
                     if(result != -1)
                     {

                           if(mesg.mesg_value == 4) //Reply from oss that request is granted
                           {

                                  inc_timer(u_semid, 1000000);
                                  //    printf("USER:%d User happy to get request granted \n", getpid());
                                   flag = 1;
                          
                           } else if(mesg.mesg_value == 5) //Reply from oss that request is denied due to some error
                           {

                                   inc_timer(u_semid, 1000000);
                                   //      printf("USER: P%d Request Denied to acces page %d: User Sad\n", av_pid, page_num);
                                    flag = 1;
                
                           }
                           if(flag == 1)
                               break;
                     }
                  } // end inner while
                 

        } //end while

        /* Detach and delete shared memory */
        if (shmdt(u_timeptr) == -1) {
            perror("shmdt failed");
            return 1;
        } else {
           // printf("user : %d Detachment of shared memory Success\n", getpid());
        }
      
    
        if (shmdt(u_pcbptr) == -1) {
            perror("shmdt failed");
            return 1;
        }
        return 0;

}



/* This function uses semaphore to update the shared clock*/
void inc_timer(int semid, int increment)
{

   struct sembuf semsignal[1];
   struct sembuf semwait[1];
   int semerror;

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
            
   }
           
   
   u_timeptr->nanosec += increment;
   if(u_timeptr->nanosec >= 999999999)
   {
        u_timeptr->sec += 1;
        u_timeptr->nanosec = u_timeptr->nanosec - 999999999;
   }

   
   /* Release semaphore after finishing with file update */
    semerror = semop(semid, semsignal, 1);
    if (semerror == -1) {
        perror("./bin_adder:Failed to unlock semaphore");
    }


}

void sighandler(int signal)
{

      int su_msgid;
      struct mesg_buffer mesg;
      su_msgid = msgget(MSG_KEY, 0600| IPC_CREAT);
      if (su_msgid == -1) {
          perror("./user: Message queue creation failed in sighandler");
      }

  //    printf("Process %d will terminate\n", getpid());  // process will terminate
      mesg.pid = av_pid;
      mesg.mesg_type = 1;
      mesg.mesg_value = 2;  //Terminate
      if (msgsnd(su_msgid, &mesg, sizeof(mesg), 0) == -1) {
               perror("./user: Error: msgsnd ");
               exit(EXIT_FAILURE);
      }

      if (shmdt(u_timeptr) == -1) {
            perror("shmdt failed");
      }

      if (shmdt(u_pcbptr) == -1) {
            perror("shmdt failed");
      }

      msgctl(su_msgid, IPC_RMID, NULL);


       printf("Process %d exiting due to interrupt signal.\n", getpid());
       exit(EXIT_SUCCESS);
}


