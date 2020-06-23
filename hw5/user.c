/* Author: Sujatha Gopi
 * File Name: user.c
 * Course: 4760 - Operating Systems
 * Purpose: This file acts as process requesting and releasing resources at random times*/

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

#include "oss.h"



struct shared_time *u_timeptr; // pointer to shared memory
struct pcb *u_pcbptr = pcb_t;
struct resource_descriptor *u_rdptr;
int u_msgid, av_pid;

void sighandler(int SIG_CODE);
void inc_timer(int semid, int increment);


int main (int argc, char *argv[])
{
   
        /* signal handler for CTRLC and alarm */
        signal(SIGINT, sighandler);


        int u_timeid, u_pcbid, u_rdid, u_semid;
        int i, result, random_choice, resource_num, temp;
        int is_holding_resource, can_ask_resource, flag, shared_resource;
        int start_sec, start_ns, current_sec, current_ns;
        struct mesg_buffer mesg;


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
       u_pcbid = shmget(PCB_KEY, (sizeof(struct pcb)*(MAX_PCB)), 0644|IPC_CREAT);
       if (u_pcbid == -1) {
              perror("./user: Shared memory creation failed");
              return 1;
       } 

       //Attach to the memory and get a pointer to it.
       u_pcbptr = (struct pcb *)shmat(u_pcbid, NULL, 0);
       if (u_pcbptr == (void *) -1) {
              perror("./user: Shared memory attach failed");
              return 1;
       } 

       // Allocate shared memory for process control block with the key defined by macro RD_KEY
       u_rdid = shmget(RD_KEY, (sizeof(struct resource_descriptor)*(MAX_RESOURCES)), 0644|IPC_CREAT);
       if (u_rdid == -1) {
             perror("./user: Shared memory creation failed");
             return 1;
       }
 
       //Attach to the memory and get a pointer to it.
       u_rdptr = (struct resource_descriptor *)shmat(u_rdid, NULL, 0);
       if (u_rdptr == (void *) -1) {
             perror("./user: Shared memory attach failed");
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


       /* Time calculations for termination time */
       start_sec = u_timeptr->sec;
       start_ns = u_timeptr->nanosec;
       current_sec = start_sec + 1;
       current_ns = start_ns;
       av_pid = atoi(argv[1]);
       u_pcbptr[av_pid].pid = getpid();
       mesg.pid = av_pid;
       mesg.mesg_type = 1;

       while(1)
       {
             if ((u_timeptr->sec > current_sec) || ((u_timeptr->sec >= current_sec) && (u_timeptr->nanosec > current_ns))) 
             {

                   if ((rand() % 100) <= 50) //Process decides to terminate
                   {
                        inc_timer(u_semid, 1000000);
                      //  printf("Process %d will terminate\n", getpid());  // process will terminate
                        printf(" P%d with pid %d started at time = %d.%09dns and terminated at time = %d.%09dns\n", av_pid, getpid(), start_sec, start_ns, u_timeptr->sec, u_timeptr->nanosec);

                        mesg.pid = av_pid;
                        mesg.mesg_type = 1;
                        mesg.mesg_value = 2;  //Terminate
                        if (msgsnd(u_msgid, &mesg, sizeof(mesg), 0) == -1) {
            		      perror("./user: Error: msgsnd ");
                              exit(EXIT_FAILURE);
                        }

                        break;
                   } // endif rand()

            } // endif timer

            /* To decide if a process should request or release the resurces, we generate a random number between [0 - 1].
             * [0] to indicate that the process will request resources.
             * [1] to indicate that the process will release its acquired resources */

             random_choice = (rand() % 2);

             if(random_choice == 0)   //process decides to request resources
             {
                   inc_timer(u_semid, 1000000);
                   //  printf("Process %d will ask for resources\n", getpid());
                   can_ask_resource = 0; 
                   shared_resource = 0;
                   resource_num = rand() % MAX_RESOURCES;
              
                   // if the randomly selected resource cannot request resource then find another resource to request  
                   if(u_pcbptr[av_pid].maximum[resource_num] == 0) 
                   {
                         for(i = 0; i < MAX_RESOURCES; i ++)
                         {
                              if (u_pcbptr[av_pid].maximum[i] > 0)
                              {
                                    resource_num = i;
                                    break;
                              }
                         }  
                   }
        


                   if(u_pcbptr[av_pid].maximum[resource_num] > 0)
                   {
                         can_ask_resource = 1;
                   } else {
                   
                         printf("Process %d cannot ask any more resource\n", getpid());
                   }


                   if(u_rdptr[resource_num].is_shared_resource == 1) //check if its a sharable resource
                   {
                         shared_resource = 1;
                         printf("Process %d is asking for shared resource\n", getpid());
                   } 

       
                   if((can_ask_resource == 1)) 
                   {
                          temp = (u_pcbptr[av_pid].maximum[resource_num] - u_pcbptr[av_pid].allocated[resource_num]);                
                          u_pcbptr[av_pid].requested[resource_num] = temp;
                          if((shared_resource == 1) && (u_pcbptr[av_pid].allocated[resource_num] == 0))
                          {
                                 u_pcbptr[av_pid].requested[resource_num] = 1;
                          }
                          mesg.mesg_type = 1;
                          mesg.rid = resource_num;
                          mesg.mesg_value = 0;  //Request 
                          if (msgsnd(u_msgid, &mesg, sizeof(mesg), 0) == -1) {
                                   perror("./user: Error: msgsnd ");
                                   exit(EXIT_FAILURE);
                          }

                          flag = 0;
                          // wait for message from oss
                          while(1) {
                               result = msgrcv(u_msgid, &mesg, (sizeof(mesg)), getpid(), 0);
                               if(result != -1)
                               {

                                     if(mesg.mesg_value == 4) //Reply from oss that request is granted
                                     {
                                             // printf("USER HAPPY FOR GETTING RESOURCE\n");
                                              temp = u_pcbptr[av_pid].requested[resource_num];
                                              u_pcbptr[av_pid].maximum[resource_num] = u_pcbptr[av_pid].maximum[resource_num] - u_pcbptr[av_pid].requested[resource_num];
                                              u_pcbptr[av_pid].allocated[resource_num] = u_pcbptr[av_pid].allocated[resource_num] + u_pcbptr[av_pid].requested[resource_num];
                                              u_pcbptr[av_pid].requested[resource_num] = 0;
                      
                                              flag = 1;
                                     }
            
                                     if(mesg.mesg_value == 5)  //Reply from oss that request is denied
                                     {
                                            //  printf("USER SAD :(  FOR NOT GETTING RESOURCE\n");
                                              u_pcbptr[av_pid].requested[resource_num] = 0;
                                              flag = 1;
                                     }
                    }
             
                    if(flag == 1)
                         break; 
                 } // end while


                } // end can-ask_resource

             } // end random_choice 0 if
  

             if(random_choice == 1)   //process decides to release resources
             {
                    inc_timer(u_semid, 1000000); 
                    printf("Process %d will release resources\n", av_pid);
                    is_holding_resource = 0;
                    resource_num = rand() % MAX_RESOURCES;

                    if(u_pcbptr[av_pid].allocated[resource_num] == 0)
                    {
                         for(i = 0; i < MAX_RESOURCES; i++)
                         {
                             if(u_pcbptr[av_pid].allocated[i] > 0)
                             {
                                    resource_num = i;
                                    break;
                             }
                        }
                    }

              
                    if(u_pcbptr[av_pid].allocated[resource_num] > 0)
                    {
                          is_holding_resource = 1;
                    } else {
                          printf("Process %d is not holding any resource to release\n", av_pid);
                    }

      
                    if(is_holding_resource == 1)
                    {   
                          temp = u_pcbptr[av_pid].allocated[resource_num];
                          u_pcbptr[av_pid].released[resource_num] = ((rand() % temp) + 1);


                          if((u_rdptr[resource_num].is_shared_resource == 1) && (u_pcbptr[av_pid].allocated[resource_num] > 0))
                          {
                                 u_pcbptr[av_pid].released[resource_num] = 1;
                          }

             
        	          mesg.pid = av_pid;
                          mesg.mesg_type = 1;
                          mesg.rid = resource_num;
                          mesg.mesg_value = 1;  //Release
                          if (msgsnd(u_msgid, &mesg, sizeof(mesg), 0) == -1) {
                                perror("./user: Error: msgsnd ");
                                exit(EXIT_FAILURE);               
                          }
 

                          flag = 0;
                          // wait for message from oss
                          while(1) 
                          {
                                result = msgrcv(u_msgid, &mesg, (sizeof(mesg)), getpid(), 0);
                                if(result != -1)
                                {


                                        if(mesg.rid != resource_num)
                                        {
                                                  perror("Receiving message for wrong resource num\n");
                                                  flag = 1;
                                                  exit(EXIT_FAILURE);
                                        }

                                        if(mesg.mesg_value == 4) //Reply from oss that request is granted
                                        {
                                                //  printf("USER HAPPY FOR RELEASING RESOURCE\n");
                                                  temp = u_pcbptr[av_pid].released[resource_num];
                                                  u_pcbptr[av_pid].allocated[resource_num] = u_pcbptr[av_pid].allocated[resource_num] - u_pcbptr[av_pid].released[resource_num];
                                                  u_pcbptr[av_pid].released[resource_num] = 0;
        
                                                  if(u_rdptr[resource_num].is_shared_resource == 1)
                                                  {
                                                           u_pcbptr[av_pid].maximum[resource_num] = u_pcbptr[av_pid].maximum[resource_num] + temp;
                                                  }

                                                  flag = 1;
                                        }
                     
                                        if(mesg.mesg_value == 5) //Reply from oss that request is denied
                                        {
                                                  // printf("USER SAD :(  FOR ABLE TO RELEASE RESOURCE\n");
                                                   u_pcbptr[av_pid].released[resource_num] = 0;
                                                   flag = 1;
                                        }
                 
                                } // end if
                       
                                if (flag == 1)
                                     break;
                         } //end while

                } // end is holding if

            } // end random choice 1 if


            inc_timer(u_semid, 1000000);

      }

      /* Detach and delete shared memory */
      if (shmdt(u_timeptr) == -1) {
            perror("shmdt failed");
            return 1;
      } else {
           // printf("user : %d Detachment of shared memory Success\n", getpid());
      }


      if (shmdt(u_rdptr) == -1) {
            perror("shmdt failed");
            return 1;
      } else {
           // printf("user : %d Detachment of shared memory Success\n", getpid());
      }

      if (shmdt(u_pcbptr) == -1) {
            perror("shmdt failed");
            return 1;
      } else {
          //  printf("user : %d Detachment of shared memory Success\n", getpid());
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
           //  exit(EXIT_FAILURE);
    }

    
    u_timeptr->nanosec += increment;
    if(u_timeptr->nanosec >= 999999999)
    {
        u_timeptr->sec += 1;
        u_timeptr->nanosec = 0;
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



      if(signal == SIGINT)
      {

           printf("Process %d will terminate\n", getpid());  // process will terminate
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


           if (shmdt(u_rdptr) == -1) {
               perror("shmdt failed");
           }
    
           if (shmdt(u_pcbptr) == -1) {
                perror("shmdt failed");
           }
 
           printf("Process %d exiting due to interrupt signal.\n", getpid());
           exit(EXIT_SUCCESS);
   }
}



             
