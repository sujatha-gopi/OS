/* Author: Sujatha Gopi
 * File Name: oss.c
 * Course: 4760 - Operating Systems
 * Project Num: 5
 * Purpose: This project will design and implement a resource management module and we will use the deadlock detection 
 * and recovery strategy to manage resources.
 * This file oss acts as Operating System Simulator oss */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/sem.h>
#include <sys/ipc.h>
#include <stdbool.h>
#include <sys/msg.h>
#include <time.h>

#include "oss.h"



struct shared_time *timeptr; // pointer to shared timer 
int timeid, pcbid, rdid, msgid, semid;
int total_process = 0, active_process = 0;
int *cpid; /* array to hold the PIDs*/
int lines = 0, num_term;
struct pcb *pcbptr = pcb_t; // pointer to pcb
struct resource_descriptor *rdptr; // pointer to resource descriptors in shared memory
FILE *fp;


int main (int argc, char *argv[])
{

       int i, k, result,  opt, eflag, available_pid, pid, terminated_process = 0;
       int availablepids[MAX_PCB];  // local bit vector to show available pids
       char *args[10], stnum[10];    /* Array to hold list of arguments for the child process. */
       unsigned int next_sec, next_ns;
       unsigned int current_sec, current_ns, start_sec, start_ns;
       int resource_id, semerror, init;
       struct mesg_buffer mesg;
       int total_grants = 0, verbose = FALSE;
       int num_deadlock_runs = 0, p_deadlocked_process = 0, old_copy = 0;

       /* constants that determine the max time process generation */
       const int maxTimeBetweenNewProcsNS = 500000000;
       const int maxTimeBetweenNewProcsSecs = 0;
 


       /* Signal Handlers for CTRL+C and Alarm
        * sighandler will send kill signal to all active children when a CTRL+C is detected
        * alarmhandler will send a kill signal to all active children when the program runs for more than 4 seconds */
        signal(SIGINT,sighandler);
        signal(SIGALRM, alarmhandler);
        alarm(10);  // alarm signal will stop program after 10 real clock seconds



       //  Below while loop uses getopt to parse command line arguments from the user
       while ((opt = getopt(argc, argv, ":hv")) != -1)
       {
           switch(opt)
           {
               case 'h':
 
                 printf("This executable %s will implement resource management.\n", argv[0]);
                 printf("*******************************************************************************\n");
                 printf("\n");
                 printf("\nOptions available to execute:\n");
                 printf("\n");
                 printf("*******************************************************************************\n");
                 printf("%s [-hv]\n", argv[0]);
                 printf("-h : Print help message and exit.\n");
                 printf("-v : Print all details in logfile.\n");
                 return EXIT_SUCCESS;

              case 'v':

                 verbose = TRUE;
                 break;

              case '?':
 
                 eflag = 1;
                 break;

          }

       }

       if (eflag == 1)
       {
           printf("%s: unknown options\n", argv[0]);
           printf("*******************************\n");
           printf("Try %s -h\n", argv[0] );
           printf("*******************************\n");
           exit(1);
       }

       // Allocate shared memory with the key defined by macro SHM_KEY for timer
       // If any problem with memory allocation throw error
       timeid = shmget(SHM_KEY, sizeof(struct shared_time), 0644|IPC_CREAT);
       if (timeid == -1) {
              perror("./oss: Shared memory creation failed");
              return 1;
       } else {
              printf("Master: Allocation of shared memory Success\n");
       }

       //Attach to the memory and get a pointer to it.
       timeptr = (struct shared_time *)shmat(timeid, NULL, 0);
       if (timeptr == (void *) -1) {
              perror("./oss: Shared memory attach failed");
              return 1;
       } else {
              printf("OSS : Attachment of shared memory Success\n");
       }

       // Allocate shared memory for resource descriptors with the key defined by macro RD_KEY
       rdid = shmget(RD_KEY, (sizeof(struct resource_descriptor)*(MAX_RESOURCES)), 0644|IPC_CREAT);
       if (rdid == -1) {
             perror("./oss: Shared memory creation failed");
             return 1;
       } else {
             printf("OSS: Allocation of shared memory Success\n");
       }

       //Attach to the memory and get a pointer to it.
       rdptr = (struct resource_descriptor *)shmat(rdid, NULL, 0);
       if (rdptr == (void *) -1) {
             perror("./oss: Shared memory attach failed");
             return 1;
       } else {
             printf("OSS : Attachment of shared memory Success\n");
       }

       // Allocate shared memory for process control block with the key defined by macro PCB_KEY
       pcbid = shmget(PCB_KEY, (sizeof(struct pcb)*(MAX_PCB)), 0644|IPC_CREAT);
       if (pcbid == -1) {
              perror("./oss: Shared memory creation failed");
              return 1;
       } else {
              printf("OSS: Allocation of shared memory Success\n");
       }

       //Attach to the memory and get a pointer to it.
       pcbptr = (struct pcb *)shmat(pcbid, NULL, 0);
       if (pcbptr == (void *) -1) {
              perror("./oss: Shared memory attach failed");
              return 1;
       } else {
              printf("OSS : Attachment of shared memory Success\n");
       }

       
       /* Create semaphores for shared processes with key defined by macro SEM_KEY */
       semid = semget(SEM_KEY,1,IPC_CREAT|0666);
       if (semid == -1) {
             perror("./OSS: Failed to create semaphore");
             exit(EXIT_FAILURE);
       }

       /* Initialize 0th element of semaphore to 1 */
       init = semctl(semid, 0, SETVAL, 1);
       if (init == -1) {
            perror("./master:Failed to initialize semaphore element to 1");
            exit(EXIT_FAILURE);
       }


       // msgget creates a message queue and returns identifier
       msgid = msgget(MSG_KEY, 0666 | IPC_CREAT);
       if (msgid == -1) {
             perror("./oss: Message queue creation failed");
             return 1;
       } else {
             printf("OSS: Creation of message queue success\n");
       }


       /* open output.log and write the actions done by this process */
       fp = fopen("output.log","w");
       if (fp == NULL){
           perror("Could not open file output.log\n");
           exit(EXIT_FAILURE);
       }
     
       printf("Log File Name: output.log\n" );
       if (verbose) {
           printf("Verbose Printing\n");
           fputs("\t\t\t Verbose Printing\n\n\n", fp);
      
       } else {
           printf("Non-Verbose Printing\n\n\n");
           fputs("\t\t\t Non - Verbose Printing\n", fp);
       }
 
       /* create a queue for adding the waiting process */
       struct queue* waiting = createQueue();

       // call malloc to allocate appropriate number of bytes for the array to store pids
       cpid = (int *) malloc (sizeof(int)*MAX_PROCESSES); // allocate ints
       
       srand (time(NULL)); //seeding 


       //Initialize shared memory attribute of clock
       timeptr->sec = 0;
       timeptr->nanosec = 0;
       

       // Loop through available pids to set all to 1(available)
       for (i = 0; i < MAX_PCB; i++) {
             availablepids[i] = 1;  // set all PIDs to be available at beginning
       }
 
       // Calculate the time to generate next process using rand()
       next_sec = (rand() % (maxTimeBetweenNewProcsSecs - 0 + 1)) + 0;
       next_ns = (rand() % (maxTimeBetweenNewProcsNS - 2000 + 1)) + 2000;

     
       start_sec = timeptr->sec;
       start_ns = timeptr->nanosec;
       current_sec = start_sec + 1;
       current_ns = start_ns;

       // Initialise resource descriptors in shared memory
       initialise_rd();  

       // Entering main resource management section. keep doing until all the processes created are terminated
       while(terminated_process < MAX_PROCESSES)
       {
      
 
                /* Get the next available PCB */
                available_pid = get_available_pid(availablepids, MAX_PCB);

                /* If it's time to generate next process and if there is a PCB available to hold the process then go ahead with generation */
                if(ready_to_spawn(available_pid, next_sec,next_ns))
                {

 	                   availablepids[available_pid] = 0; //Mark the PCB/PID as taken
                           /* Initialise the process for the available PCB slot. */
                           initialise_pcb(available_pid);

                           /* Get argument ready for child process in format ./user stnum
                            * where stnum is the starting PCB number which the process should use */
                           snprintf(stnum, sizeof(int), "%d", available_pid);
                           args[0] = "./user";
                           args[1] = stnum;
                           args[2] = NULL;    /* Indicates the end of arguments. */


                           // Fork a new process

                           printf("P%d forked at %d.%09dns\n",available_pid, timeptr->sec, timeptr->nanosec);

                           pid = fork();
               
                          /* Display error if fork fails */
                          if (pid == -1) {
                                perror("Failed to fork");
                          }
  


                          // On successful completion, fork() returns 0 to the child process and
                          // returns the process ID of the child process to the parent process.
                          if (pid == 0) {
                                // Child process will now execute the exec() command which runs the user.c program
                                int e = execvp(args[0], args);
                                // If execvp is successfull child process does not execute any commands below
                                if ( e )
                                      perror("Error and didn't execute execvp");
                     
                          }
                     
                     
                          /* all the below commands are executed by parent process
                           * parent will increment the num of spawned children and num of children currently active
                           *  The pid of the forked child is stored in cpid[] and the child launched time is printed in output file */
                           increment_timer(semid, 1000000);
                           cpid[available_pid] = pid;
                           printf("Pid %d = %d\n", total_process, pid);
                           active_process += 1;
                           total_process += 1;
                           next_sec = next_sec + (rand() % (maxTimeBetweenNewProcsSecs - 0 + 1)) + 0;  // time for next process generation
                           next_ns = next_ns + (rand() % (maxTimeBetweenNewProcsNS - 0 + 1)) + 0;

                }  // spawn endif

             
                // check for any messages from children
                result = msgrcv(msgid, &mesg, (sizeof(mesg)), 1, IPC_NOWAIT);
                if(result > 0)
                {
                           // get the details of the child which sent this message from its message parameter
                           available_pid = mesg.pid;
                           //resource_id = mesg.rid;
                           pid = pcbptr[available_pid].pid;

                           if(mesg.mesg_value == 2) // 2 means child wants to terminate
                           {
                           
                                   // increment the timer
                                   increment_timer(semid, 1000000);
                                   available_pid = mesg.pid;
                                   resource_id = mesg.rid;
                                   pid = pcbptr[available_pid].pid;
                                   // wait for the child process termination and incremented the num of terminated process
                                   k = wait(NULL);
                                   if ( k > 0 )
                                   {
                                           active_process = active_process - 1;
                                           terminated_process = terminated_process + 1;
                                           availablepids[available_pid] = 1;
                                           printf("Master: Real time process PID %d terminated normally at %ds:%dns\n",k, timeptr->sec, timeptr->nanosec );

                                   }
                   
                                   
                                   // Release all the resouces held by the terminated process and add them to the available resouces
                                   for(i = 0; i < MAX_RESOURCES; i++)
                                   {
                                           if(rdptr[i].is_shared_resource == 0) 
                                           {
                                                 rdptr[i].available = rdptr[i].available + pcbptr[available_pid].allocated[i];
                                           }
                                           pcbptr[available_pid].released[i] = 0;
                                           pcbptr[available_pid].allocated[i] = 0;
                                           pcbptr[available_pid].requested[i] = 0;
                     

                                   } 
                                   if ((verbose == TRUE) && (lines < 100000))
                                           fprintf(fp, "%-5d Master has detected p%d has terminated at time %d.%09dns\n", lines++, available_pid, timeptr->sec, timeptr->nanosec);


                           } else if(mesg.mesg_value == 0) // process is requesting for resources
                           {
                                   // increment the timer
                                   increment_timer(semid, 1000000); 
                                   available_pid = mesg.pid;
                                   resource_id = mesg.rid;
                                   pid = pcbptr[available_pid].pid;
   
                                   //  printf("****OSS: Process %d is asking for request***\n", pid);
                                   if ((verbose == TRUE) && (lines < 100000))
                                           fprintf(fp, "%-5d Master has detected p%d requests %d instance of resource %d at time %d.%09dns\n", lines++, available_pid, pcbptr[available_pid].requested[resource_id], mesg.rid, timeptr->sec, timeptr->nanosec);

                                  //OSS will grant resources only if the requested resources is less than the available resources                
                                  if((pcbptr[available_pid].requested[resource_id] <= rdptr[resource_id].available) && (pcbptr[available_pid].requested[resource_id] <= pcbptr[available_pid].maximum[resource_id]))
                                   {
                                           //  printf("Request Granted\n");
                        
                                           if(rdptr[resource_id].is_shared_resource == 0)  //availability reduces only for non sharable resources
                                           {
                                                   rdptr[resource_id].available = rdptr[resource_id].available - pcbptr[available_pid].requested[resource_id];
                                           }
 
                                           mesg.rid = resource_id;
                                           mesg.mesg_type = pid;
                                           mesg.mesg_value = 4;  //Granted
                                           if (msgsnd(msgid, &mesg, sizeof(mesg), 0) == -1) {
                                                   perror("./user: Error: msgsnd ");
                                                   exit(EXIT_FAILURE);
                                           }
                                           total_grants += 1; //total number of requests granted
                                           //Verbose logging
                                           if ((verbose == TRUE) && (lines < 100000)) 
                                           {
                                                   fprintf(fp, "%-5d Master granting p%d %d instance of resource %d at %d.%09dns\n", lines++,available_pid, pcbptr[available_pid].requested[resource_id], mesg.rid,  timeptr->sec, timeptr->nanosec);
                               
                                           }
                                           printf("Master granting p%d %d instance of resource %d at %d.%09dns\n", available_pid, pcbptr[available_pid].requested[resource_id], mesg.rid,  timeptr->sec, timeptr->nanosec);


                                  } else {   //this part comes when there are not enough resouces to be granted
                        
                                         // printf("Request denied\n");
                                         available_pid = mesg.pid;
                                         mesg.rid = resource_id;
                                         mesg.mesg_type = pid;
                                         mesg.mesg_value = 5;  //Denied
                                         if (msgsnd(msgid, &mesg, sizeof(mesg), 0) == -1) {
                                                  perror("./user: Error: msgsnd ");
                                                  exit(EXIT_FAILURE);
                                         } 
                                         if ((verbose == TRUE) && (lines < 100000))
                                                  fprintf(fp, "%-5d Master denied p%d resource %d at %d.%09dns\n", lines++, available_pid, mesg.rid, timeptr->sec, timeptr->nanosec);
                                 
                                         printf("Master denied p%d resource %d at %d.%09dns\n", available_pid, mesg.rid, timeptr->sec, timeptr->nanosec); 
     
                                }

                } else if(mesg.mesg_value == 1)    // Process wants to release resources
                {
                               increment_timer(semid, 1000000);
                               //  printf("****OSS: Process is releasing resources***\n");
                               // get the details of the child which sent this message from its message parameter
                               available_pid = mesg.pid;
                               resource_id = mesg.rid;
                               pid = pcbptr[available_pid].pid;

                               if(pcbptr[available_pid].released[resource_id] > 0)
                               {
 
                                       if(rdptr[resource_id].is_shared_resource == 0)
                                       {
                                              rdptr[resource_id].available = rdptr[resource_id].available + pcbptr[available_pid].released[resource_id];
                                       }
                                       mesg.mesg_type = pid;
                                       mesg.mesg_value = 4;  //Release Granted
                                       if (msgsnd(msgid, &mesg, sizeof(mesg), 0) == -1) {
                                              perror("./user: Error: msgsnd ");
                                              exit(EXIT_FAILURE);
                                       }
                                       if ((verbose == TRUE) && (lines < 100000))
                                             fprintf(fp, "%-5d Master detected p%d has released %d instance of resource %d at %d.%09dns\n", lines++,available_pid, pcbptr[available_pid].released[resource_id], mesg.rid,  timeptr->sec, timeptr->nanosec);
                                       printf("Master detected p%d has released %d instance of resource %d at %d.%09dns\n",available_pid, pcbptr[available_pid].released[resource_id], mesg.rid,  timeptr->sec, timeptr->nanosec);
                      
                              } else {  // the else part comes when oss is not able to accept the release due to some issues
                              
                                       printf("****OSS: Process release of resources failed***\n");
                                       printf("No sufficient resources to release. Check again\n");
                                       available_pid = mesg.pid;
                                       resource_id = mesg.rid;
                                       pid = pcbptr[available_pid].pid;
                                       mesg.mesg_type = pid;
                                       mesg.mesg_value = 5;  //Release Denied

                                       if (msgsnd(msgid, &mesg, sizeof(mesg), 0) == -1) {
                                             perror("./user: Error: msgsnd ");
                                             exit(EXIT_FAILURE);
                                       }
                             } // end if

                } 

             //int match_found = 0;
             if(isEmpty(waiting) == 0)
             { 
                 int q_len = waiting->length;
                 while(q_len > 0)
                 {
                         int simpid;
                        // match_found = 0;
                         simpid = dequeue(waiting);
                         for(i = 0; i < MAX_RESOURCES; i ++)
                         {
                              if((pcbptr[simpid].requested[i] > 0) && (pcbptr[simpid].requested[i] < rdptr[i].available))
                                {
                                            // match_found = 1;
                                             mesg.pid = simpid;
                                             mesg.rid = i;
                                             mesg.mesg_type = pcbptr[simpid].pid;
                                             mesg.mesg_value = 4;  //Granted
                                             if(rdptr[i].is_shared_resource == 0)
                                             {
                                                   rdptr[i].available = rdptr[i].available - pcbptr[simpid].requested[i];
                                             }
                                             if (msgsnd(msgid, &mesg, sizeof(mesg), 0) == -1) {
                                                         perror("./user: Error: msgsnd ");
                                                         exit(EXIT_FAILURE);
                                             }
                                             printf("OSS: Message sent to %d is %d\n", pcbptr[simpid].pid, mesg.mesg_value);
                                             total_grants += 1;
                              }
                         }
                    
                    q_len = q_len - 1;
                  //  if(match_found == 0) 
                        //enqueue(waiting, simpid);
                }
            }

                                    
                                              

             } // end result if
    

             increment_timer(semid, 1000000);

             // Run deadlock algorithm once every second to detect any deadlock and clear it
             if ((timeptr->sec > current_sec) || ((timeptr->sec >= current_sec) && (timeptr->nanosec > current_ns))) {

                      deadlock_detection(availablepids); 
                      current_sec = timeptr->sec + 1;   // time calculation for the next run
                      current_ns = timeptr->nanosec;
                      num_deadlock_runs += 1;   //number of times deadlock alg is run
            }
            if(total_grants > old_copy) {

                if((total_grants > 0) && ((total_grants % 20) == 0))
                { 
                     print_matrix();  //shows the allocation matrix of the processes
                     old_copy = total_grants;
                } 
           }
      
      } // end while

       printf("Terminated = %d \n", terminated_process);
       printf("Total = %d\n", total_process);

      p_deadlocked_process = ((float)num_term / (float)MAX_PROCESSES) * 100;

      printf("\n*******************************************************************************\n");
      printf("***************************** Final Results ***********************************\n");
      printf("*******************************************************************************\n");

      fputs("\n**********************************************************************************\n", fp);
      fputs("***************************** Final Results ***********************************\n", fp);
      fputs("************************************************************************************\n", fp);

      printf("Total number of proceses = %d\n", total_process);
      printf("Total process killed by deadlock algorithm = %d\n", num_term);
      printf("Total process that are terminated successfully = %d\n",(MAX_PROCESSES - num_term));
      printf("Number of times deadlock algorithm is run =  %d\n", num_deadlock_runs);
      printf("Percentage of processes terminated by deadlock algorithm = %d %%\n", p_deadlocked_process);
   
      if (lines < 100000) { 
                 fprintf(fp, "%-5d Total  number of proceses = %d\n", lines++, total_process);
		 fprintf(fp, "%-5d Total process killed by deadlock algorithm is %d\n", lines++, num_term);
	         fprintf(fp, "%-5d Total process that are terminated successfully %d\n",lines++, (MAX_PROCESSES - num_term));
	         fprintf(fp, "%-5d Number of times deadlock algorithm is run =  %d\n", lines++, num_deadlock_runs);
	         fprintf(fp, "%-5d Percentage of processes terminated by deadlock algorithm = %d %%\n", lines++, p_deadlocked_process);
     }

      printf("\n******************************************************************************\n");
      printf("*******************************************************************************\n");
      printf("*******************************************************************************\n");

      fputs("\n*********************************************************************************\n", fp);
      fputs("***********************************************************************************\n", fp);
      fputs("************************************************************************************\n", fp);

 
      free(cpid);
      fclose(fp);
        
      /* Detach and delete shared memory */

      /* destroy the message queue */
      if( msgctl(msgid, IPC_RMID, NULL) == -1) {

          perror("./oss:msgctl failed");
          return 1;
      } else {
          printf("OSS : Deletion of message queue success\n");
      }  

      /* Remove semaphore */
      semerror = semctl(semid, 0, IPC_RMID);
      if (semerror == -1) {
             perror("./OSS:Failed to remove semaphore");
      }


       if (shmdt(timeptr) == -1) {
            perror("shmdt failed");
            return 1;
       } else {
            printf("OSS : Detachment of shared memory Success\n");
       } 


       if (shmctl(timeid, IPC_RMID, 0) == -1) {
            perror("./oss:shmctl failed");
            return 1;
       } else {
            printf("OSS : Deletion of shared memory Success\n");
       }


       if (shmdt(rdptr) == -1) {
            perror("shmdt failed");
            return 1;
       } else {
            printf("OSS : Detachment of shared memory Success\n");
       }

       if (shmctl(rdid, IPC_RMID, 0) == -1) {
           perror("./oss: shmctl failed");
           return 1;
       } else {
            printf("OSS : Deletion of shared memory Success\n");
       }


       if (shmdt(pcbptr) == -1) {
            perror("shmdt failed");
            return 1;
      } else {
            printf("OSS : Detachment of shared memory Success\n");
      }


      if (shmctl(pcbid, IPC_RMID, 0) == -1) {
           perror("./oss: shmctl failed");
           return 1;
      } else {
            printf("OSS : Deletion of shared memory Success\n");
      } 

  

return 0;
}



/* Func to increment timer by given nanosecond */
void increment_timer(int semid, int increment)
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
        perror("./OSS:Failed to lock semaphore");
   }


   // Increment timer by the given increment
   timeptr->nanosec += increment;
   if(timeptr->nanosec >= 999999999)
   {
        timeptr->sec += 1;
        timeptr->nanosec = 0;
   }

   
   /* Release semaphore after finishing with file update */
    semerror = semop(semid, semsignal, 1);
    if (semerror == -1) {
        perror("./OSS:Failed to unlock semaphore");
    }


}


/* This function initialises the resource descriptors with initial values */
void initialise_rd()
{

    int i;

    for (i = 0; i < MAX_RESOURCES; i++) {
       if ((rand() % 100) <= 20) {
           rdptr[i].resource_id = i;
           rdptr[i].no_of_instances = 1;
           rdptr[i].available = rdptr[i].no_of_instances;
           rdptr[i].is_shared_resource = 1;

      } else {
           rdptr[i].resource_id = i;
           rdptr[i].no_of_instances = rand() % 10 + 1;
           rdptr[i].available = rdptr[i].no_of_instances;
           rdptr[i].is_shared_resource = 0;
      }
       //printf("resource_id = %d,no_of_instances = %d, available = %d", rdptr[i].resource_id, rdptr[i].no_of_instances, rdptr[i].available);


    } // end for
  
}

/* This function initialises the process control block with initial values */
void initialise_pcb(int pid)
{
 
   int i;
   for(i = 0; i < MAX_RESOURCES; i++)
    {
        pcbptr[pid].pid = 0;
        pcbptr[pid].maximum[i] = rand() % (rdptr[i].no_of_instances + 1);
        pcbptr[pid].allocated[i] = 0;
        pcbptr[pid].requested[i] = 0;
        pcbptr[pid].released[i] = 0;


     //  printf("rd_max = %d, maximum = %d, allocated = %d, requested = %d, released = %d\n", rdptr[i].no_of_instances, pcbptr[pid].maximum[i], pcbptr[pid].allocated[i], pcbptr[pid].requested[i], pcbptr[pid].released[i]);

    }

}

/* Function returns the next PCB which is free */
int get_available_pid(int *pids, int num_pcb)
{
    int i;
    for (i = 0; i < num_pcb; i++) {
    if (pids[i]) {  // check if pid is available
      return i;     // return available pid
    }
  }
  return -1;  // no available pids
}
 

/* This function checks if all conditions are favourable for spawning a new process */
int ready_to_spawn(int pid, unsigned int next_sec, unsigned int next_ns)
{
    if ((pid < 0) || (active_process >= MAX_PCB))
      return 0;

    if ((next_sec > timeptr->sec) || ((next_sec >= timeptr->sec) && (next_ns > timeptr->nanosec)))
      return 0;

    if (total_process >= MAX_PROCESSES)
       return 0;

return 1;

}

//Deadlock detection algorithm
// This algorithm is from the deadlock.pdf
void deadlock_detection(int *pids)
{
     int work[MAX_RESOURCES];
     int finish[MAX_PCB];
     int deadlocked[MAX_PCB];
     int deadlockCount = 0;

     int i, p = 0;

     fprintf(fp, "%-5d Master running deadlock detection at time%d.%09dns\n", lines++, timeptr->sec, timeptr->nanosec);
     printf("Master running deadlock detection at time%d.%09dns\n", timeptr->sec, timeptr->nanosec);

     for (i = 0; i < MAX_RESOURCES; i++) {
           work[i] = rdptr[i].available;
     }

     for (i = 0; i < MAX_PCB; i++) {
           finish[i] = FALSE;
     }

     

     for (p = 0; p < MAX_PCB; p++) {
          if (finish[p] == TRUE) {
              continue;
          }

          if ((req_lt_avail(pcbptr[p].requested, work, p, MAX_RESOURCES)) == TRUE) {
             finish[p] = TRUE;
             for (i = 0; i < MAX_RESOURCES; i++) {
                  work[i] += pcbptr[p].allocated[i];
             }
             p = -1;
         }
    }


    for (p = 0; p < MAX_PCB; p++) {
         if (finish[p] == FALSE) {
             deadlocked[deadlockCount] = p;
             deadlockCount += 1;
         }
    }

    if(deadlockCount > 0) {
      printf("**********************\n");
      printf("There is deadlock\n"); 
      printf("**********************\n");

      if (lines < 100000)
      {
           fprintf(fp, "%-5d Master detected deadlock at %d.%09dns\n", lines++, timeptr->sec, timeptr->nanosec);
           fprintf(fp, "%-5d \t Processes ", lines++);
           for(i = 0; i < deadlockCount; i++)
                fprintf(fp, "P%d ",deadlocked[i]);
           fprintf(fp, "deadlocked at %ds. %09dns\n",timeptr->sec, timeptr->nanosec);
           fprintf(fp, "%-5d \t Attempting to resolve deadlock... \n", lines++);
      }

      for (i = 0; i < deadlockCount; i++) {
      
           terminate_processes(deadlocked[i]);
      }

  
    } else { 
     if (lines < 100000)
            fprintf(fp, "%-5d System is no longer in deadlock\n", lines++);

     printf("System is nolonger in deadlock\n");
    }

}

/* This function checks if the requested resouce is less than the available resource for each process & each resource */
int req_lt_avail(int * requested, int * avail, int pnum, int num_res)
{

    int i;
    for (i = 0; i < num_res; i++) {
         if (pcbptr[pnum].requested[i] > avail[i])
               break;
    } 

    
    if (i == num_res) {
        return TRUE;
    } else {
        return FALSE;
    }
}

/* This function terminates the processes which are deadlocked as identified by the deadlock detection algorithm */
void terminate_processes(int available_pid)
{

     int j;
         if (cpid[available_pid] != 0)
         {

               fprintf(fp, "%-5d \t Killing process p%d: \n", lines++, available_pid);
               fprintf(fp, "%-5d \t\tResources released are as follows:", lines++);



               //release all the resources
               for (j = 0; j < MAX_RESOURCES; j++) {
                   
                    if((pcbptr[available_pid].allocated[j]) > 0)
                       fprintf(fp, " R%d:%d",j,pcbptr[available_pid].allocated[j]);

                  // resources will be released by sending terminate signal by user process to oss.
                  // So we are not releasing the resources here. we just send SIGINT to child
                              
               }


               num_term += 1;
               kill(cpid[available_pid],SIGINT);
               printf("Killing process %d\n", cpid[available_pid]);
               printf("Num_term = %d\n", num_term);
               fputs("\n", fp);
      
         }

}

/* This function prints the allocation matrix */
void print_matrix()
{

       int row, column;
 
       fputs("   ", fp);      
       printf("%*c", 3, ' ');
       for (column = 0; column < MAX_RESOURCES; column++) {
            fprintf(fp, "R%-2d ", column);
            printf("R%-2d ", column);
       }
       printf("\n");
       fprintf(fp, "\n");
       for (row = 0; row < MAX_PCB; row++) 
       {
           fprintf(fp, "P%-2d ", row);
           printf("P%-2d ", row);
           for (column = 0; column < MAX_RESOURCES; column++)
           {
                if (pcbptr[row].allocated[column] == 0) 
                {
                    fprintf(fp, "--  ");
                    printf("--  ");
                } else {
                    fprintf(fp, "%-3d ", pcbptr[row].allocated[column]);
                    printf("%-3d ", pcbptr[row].allocated[column]);
                }
           }
           printf("\n");
           fprintf(fp, "\n");

       }

}       

/* This function cleans up all the shared memory */    
void release_shared_mem()
{
   free(cpid);
   fclose(fp);
   msgctl(msgid, IPC_RMID, NULL);
   semctl(semid, 0, IPC_RMID);
   shmdt(timeptr);
   shmctl(timeid, IPC_RMID, 0);
   shmdt(pcbptr);
   shmctl(pcbid, IPC_RMID, 0);
   shmdt(rdptr);
   shmctl(rdid, IPC_RMID, 0);
   exit(0);
}

static void sighandler(int signo)
{

    int i;
    printf("\n***************************\n");
    printf("\n\tReceived CTRL+C\n");
    printf("\n***************************\n");

    /* Send kill signal to all active children */
     for(i = 0; i < total_process; i++)
    {
         if (cpid[i] != 0)
         {
               kill(cpid[i],SIGINT);

         }
    }

    // cleanup semaphores and shared memory
    release_shared_mem();
   
   
}
   


/* alarmhandler will get invoked if program runs for more than 20 seconds
 *   It will send kill signal to all active children */
 
void alarmhandler()
{
    int i;

    printf("\n**************************************************\n");
    printf("Total execution time of program exceeded 4 seconds\n");
    printf("Terminating the program\n");
    printf("\n**************************************************\n");

    for(i = 0; i < total_process; i++)
    {
         if (cpid[i] != 0)
         {
               kill(cpid[i],SIGINT);

         }
    }

    // Cleanup semaphores and shared memory
     release_shared_mem();

}

