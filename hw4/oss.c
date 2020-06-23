/* Author: Sujatha Gopi
 * File Name: oss.c
 * Course: 4760 - Operating Systems
 * Project Num: 4
 * Purpose: This project simulates the process scheduling part of an operating system. This will implement time-based scheduling,
 * using message queues for synchronization. This file oss.c will serve as the OS which does the scheduling*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/ipc.h> 
#include <sys/msg.h>
#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <time.h> 
#include "oss.h"

#define USER_PROCESS 75

struct shared_time *timeptr; // pointer to shared memory
struct pcb *pcbptr = pcb_t;
struct pcb *copy = pcb_t;
int total_process = 0, active_process = 0;
int timeid, pcbid, msgid;
int quantum = 10000000;
int *cpid; /* array to hold the PIDs*/
int lines = 0;
FILE *fp;

void create_pcb(int pid, int priority);
int get_available_pid(int *pids, int num_pcb);
void increment_timer(int increment);
int set_priority();
int dispatch_process(int pid);
void release_shared_mem();
static void sighandler(int signo);
void alarmhandler();
int ready_to_spawn(int pid, unsigned int next_sec, unsigned int next_ns);
void process_waiting( int pid, unsigned int wait_time);
int blocked_process(int* blockedarray);
void update_process_time(int pid);
int is_cpuidle(int *pids, int* blockedarray,unsigned int next_sec, unsigned int next_ns, int len0, int len1, int len2, int len3);


int main (int argc, char *argv[])
{

        int i,k, available_pid, pid, priority, terminated_process = 0;
        int blockedpids[NUM_PCB];    //blocked "queue"
        int availablepids[NUM_PCB];  // local bit vector to show available pids
        char *args[10], stnum[10];     /* Array to hold list of arguments for the child process. */
        unsigned int nanosec_inc, temp_time, next_sec, next_ns;
        unsigned int total_run_nanosecond = 0, total_wait_nanosecond = 0, total_cpu_nanosecond = 0;
        unsigned int total_burst_nanosecond = 0, total_block_waitq_time = 0, total_cpu_idle_time = 0;
        unsigned int cpu_busy_time = 0, cpu_utilization = 0, average_waitq_time = 0, average_wait_time = 0, tot_s,tot_ns;        
        int decision, cpu_status, iy, opt, eflag, average_s, average_ns, pqueue;  
        char filename[20];

        /* constants that determine the max time process generation */ 
        const int maxTimeBetweenNewProcsNS = 5430000;
        const int maxTimeBetweenNewProcsSecs = 2;

        srand (time(NULL)); //Seeding the rand()

        /* Signal Handlers for CTRL+C and Alarm
         * sighandler will send kill signal to all active children when a CTRL+C is detected
         * alarmhandler will send a kill signal to all active children when the program runs for more than 15 seconds */
        signal(SIGINT,sighandler);
        signal(SIGALRM, alarmhandler);
        alarm(15);  // alarm signal will stop program after 15 real clock seconds
        strcpy(filename,"log.out");   // output log file

        //  Below while loop uses getopt to parse command line arguments from the user
        while ((opt = getopt(argc, argv, ":h")) != -1)
        {
           switch(opt)
           {
               case 'h':

                  printf("This executable %s will implement time-based scheduling.\n", argv[0]);
                  printf("*******************************************************************************\n");
                  printf("\n");
                  printf("\nOptions available to execute:\n");
                  printf("\n");
                  printf("*******************************************************************************\n");
                  printf("%s [-h]\n", argv[0]);
                  printf("-h : Print help message and exit.\n");
                  return EXIT_SUCCESS;

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

        // Allocate shared memory for process control block with the key defined by macro PCB_KEY
        pcbid = shmget(PCB_KEY, (sizeof(struct pcb)*(NUM_PCB)), 0644|IPC_CREAT);
        if (pcbid == -1) {
              perror("./oss: Shared memory creation failed");
              return 1;
        } else {
              printf("OSS: Allocation of shared memory Success\n");
        }

        pcbptr = (struct pcb *)shmat(pcbid, NULL, 0);
        if (pcbptr == (void *) -1) {
              perror("./oss: Shared memory attach failed");
              return 1;
        } else {
              printf("OSS : Attachment of shared memory Success\n");
        }
        
        // msgget creates a message queue and returns identifier
        msgid = msgget(MSG_KEY, 0666 | IPC_CREAT); 
        if (msgid == -1) {
              perror("./oss: Message queue creation failed");
              return 1;
        } else {
              printf("OSS: Creation of message queue success\n");
        }

        // call malloc to allocate appropriate number of bytes for the array to store pids
         cpid = (int *) malloc (sizeof(int)*MAX_PROCESSES); // allocate ints

        // Create a queue structure using createQueue() to add the directories
         struct queue* queue0 = createQueue();  //high priority
         struct queue* queue1 = createQueue();
         struct queue* queue2 = createQueue();
         struct queue* queue3 = createQueue();  // low priority
         
        //Initialize shared memory attribute of clock
        timeptr->sec = 0;
        timeptr->nanosec = 0;

        // Loop through available pids to set all to 1(available)
        for (i = 0; i < NUM_PCB; i++) {
             blockedpids[i] = 0;  //set blocked "queue" to empty
             availablepids[i] = 1;  // set all PIDs to be available at beginning
        }  

        // Open and initialis output log file
        fp = fopen(filename, "w");
        if (fp == NULL){
            printf("Could not open file %s",filename);
            exit(EXIT_FAILURE);
        }

        // Calculate the time to generate next process using rand()
        next_sec = (rand() % (maxTimeBetweenNewProcsSecs - 0 + 1)) + 0;
        next_ns = (rand() % (maxTimeBetweenNewProcsNS - 2000 + 1)) + 2000;

        // Entering main scheduling section. keep doing until all the processes created are terminated
        while(terminated_process < MAX_PROCESSES)
        {
               
             /* Get the next available PCB and set the priority of the process that is going to be generated next
              * Increment the timer to allow some overhead for doing all extra calculations */
             available_pid = get_available_pid(availablepids, NUM_PCB); 
             priority = set_priority(); //sets a process as real time or user process
             increment_timer(10000);

             /* If it's time to generate next process and if there is a PCB available to hold the process then go ahead with generation */
             if(ready_to_spawn(available_pid, next_sec,next_ns))
             {
  
       	          availablepids[available_pid] = 0; //Mark the PCB/PID as taken
                 
                  /* Initialise the PCB for the available PCB slot. If created process is a real time process then add it to high priority queue 
                   * If user process is generated then add it to queue 1 */
                  create_pcb(available_pid, priority);
 
                  if(priority == 0)
                      enqueue(queue1, available_pid);
                  else if (priority == 1)
                       enqueue(queue0, available_pid);

                  pqueue = 1 - priority;
                  fprintf(fp, "%-5d: OSS: priority 1 = Real time process and priority 0 = User process \n", lines++);
                  fprintf(fp, "%-5d: OSS: Generating process with PID %d and putting it in Queue %d at %ds:%09dns\n",lines++, available_pid, pqueue, timeptr->sec, timeptr->nanosec);
 
                  /* Get argument ready for child process in format ./binary stnum
                   * where stnum is the starting PCB number which the process should use */

                  snprintf(stnum, sizeof(int), "%d", available_pid);
                  args[0] = "./binary";
                  args[1] = stnum;
                  args[2] = NULL;    /* Indicates the end of arguments. */

                  // Fork a new process
                  printf("OSS ready to fork.. \n");
                  pid = fork();

                  /* Display error if fork fails */
                  if (pid == -1) {
                       perror("Failed to fork");
                  } 
  
                  // On successful completion, fork() returns 0 to the child process and
                  // returns the process ID of the child process to the parent process.
       
                  if (pid == 0) {
                     // Child process will now execute the exec() command which runs the prime.c program
                     int e = execvp(args[0], args);
                     // If execvp is successfull child process does not execute any commands below
                     if ( e )
                          perror("Error and didn't execute execvp");
 
                  }

                  /* all the below commands are executed by parent process
                   * parent will increment the num of spawned children and num of children currently active
                   *  The pid of the forked child is stored in cpid[] and the child launched time is printed in output file */
                   active_process += 1;
                   total_process += 1;
                   next_sec = next_sec + (rand() % (maxTimeBetweenNewProcsSecs - 0 + 1)) + 0;  // time for next process generation
                   next_ns = next_ns + (rand() % (maxTimeBetweenNewProcsNS - 0 + 1)) + 0;
                   if(next_ns > 1000000000)
                   {
                       next_sec = next_sec + 1;
                       next_ns = next_ns - 1000000000;
                   }
                   cpid[total_process - 1] = pid;

        }  // endif
      
        available_pid = get_available_pid(availablepids, NUM_PCB);
        iy = ready_to_spawn(available_pid, next_sec,next_ns);

        /* start scheduling if no other process can be generated at this moment. start from the real time process scheduling
         * Proceed to lower priority queues only if there are no more real time process to schedule */  
        if ((iy == 0) && (queue0->length > 0)) 
        {
                   increment_timer(10000); //scheduling overhead
                   available_pid = dequeue(queue0);
                   decision = dispatch_process(available_pid);  // send a message to child process indicating the time it should use
                   nanosec_inc = (rand() % (10000 - 100 + 1)) + 100;
                   increment_timer(nanosec_inc);  // dispatch overhead
                   fprintf(fp, "%-5d: OSS: Total time this dispatch was %u nanoseconds\n",lines++, nanosec_inc);

                   /* Depending on the decidion by the child process the scheduling will proceed to take any one of the branches below */
                   if ( decision == 0) // Process decides to terminate
                   {
                          // increment the timer by the duration the process run. since the process has decided to terminate 
                          // wait for the child process termination and incremented the num of terminated process 
                          temp_time = (pcbptr[available_pid].run_second * 1000000000) + pcbptr[available_pid].run_nanosecond;
                          increment_timer(temp_time);
                          fprintf(fp, "%-5d: OSS: Receiving that process with PID: %3d ran for %u nanoseconds\n", lines++, available_pid, temp_time);
                          k = wait(NULL);
                          if ( k > 0 )
                          {
                                active_process = active_process - 1;
                                terminated_process = terminated_process + 1;
                                availablepids[available_pid] = 1;
                                printf("Master: Real time process PID %d terminated normally at %ds:%dns\n",k, timeptr->sec, timeptr->nanosec );

                          }
                          fprintf(fp, "%-5d: OSS: Real time Terminated PID: %3d Used: %ds:%dns\n", lines++, available_pid, pcbptr[available_pid].run_second, pcbptr[available_pid].run_nanosecond);

                          /* Update each process with the cpu time used, wait time and run time and add it to total calculations*/
                          update_process_time(available_pid);
                          total_run_nanosecond = total_run_nanosecond + pcbptr[available_pid].process_run_time;
                          total_cpu_nanosecond = total_cpu_nanosecond + pcbptr[available_pid].process_cpu_time;
                          total_wait_nanosecond = total_wait_nanosecond + pcbptr[available_pid].process_wait_time;
                          total_block_waitq_time = total_block_waitq_time + pcbptr[available_pid].process_block_waitq_time;
                  }

                  if (decision == 1) // process decides to use its full time quantum
                  {
                          // increment the timer by the duration the process run.
                          // processing depends if the process reenters after being pre-empted or regular decision to use full quantum
                          temp_time = (pcbptr[available_pid].run_second * 1000000000) + pcbptr[available_pid].run_nanosecond;
                          increment_timer(temp_time);
                          fprintf(fp, "%-5d: OSS: Receiving that process with PID: %3d ran for %u nanoseconds\n", lines++, available_pid, temp_time);
                          if(pcbptr[available_pid].preempt_complete == 1) // process reenters after being preempted to exhaust its quantum
                          {
                               enqueue(queue0, available_pid);
                               pcbptr[available_pid].preempt_complete = 0;
                               fprintf(fp, "%-5d: OSS: Previously Preempted PID: %3d Completed its quantum - Used: :%9ds:%9dns\n", lines++, available_pid, pcbptr[available_pid].run_second,pcbptr[available_pid].run_nanosecond);
                               fprintf(fp, "%-5d: OSS: PID: %3d moved to tail of Queue 0\n", lines++, available_pid);
                               update_process_time(available_pid);

                          } else {   //regular decision to use full quantum
                               enqueue(queue0, available_pid);
                               fprintf(fp, "%-5d: OSS: Full Quantum PID: %3d Used: :%9ds:%9dns\n", lines++, available_pid, pcbptr[available_pid].run_second,pcbptr[available_pid].run_nanosecond);
                               fprintf(fp, "%-5d: OSS: PID: %3d moved to Queue 0\n", lines++, available_pid);
                               update_process_time(available_pid);


                          }
 
                   }
            
                   if (decision == 2) // process decides to get blocked on some event and wait for "event_wait_time"
                   {
                          // increment the timer by the duration the process run.
                          // Add the process to blocked queue
                          temp_time = (pcbptr[available_pid].run_second * 1000000000) + pcbptr[available_pid].run_nanosecond;
                          increment_timer(temp_time);
                          fprintf(fp, "%-5d: OSS: Receiving that process with PID: %3d ran for %u nanoseconds\n", lines++, available_pid, temp_time);
                          blockedpids[available_pid] = 1;
                          pcbptr[available_pid].block_waitq_second = timeptr->sec;
                          pcbptr[available_pid].block_waitq_nanosecond = timeptr->nanosec;
                          pcbptr[available_pid].ready = FALSE;
                          fprintf(fp, "%-5d: OSS: Blocked    PID: %3d Used: %9ds:%9dns\n", lines++, available_pid, pcbptr[available_pid].run_second, pcbptr[available_pid].run_nanosecond);
                          fprintf(fp, "%-5d: OSS: PID: %3d moved to Blocked queue at %9ds.%9dns\n", lines++, available_pid, timeptr->sec, timeptr->nanosec);
                          temp_time = (pcbptr[available_pid].event_wait_second * 1000000000) + pcbptr[available_pid].event_wait_nanosecond;
                          increment_timer(temp_time);
                          update_process_time(available_pid);


                  }

                  if (decision == 3)  //process decides to get preempted by p% of its quantum time
                  {
                          // increment the timer by the duration the process run.
                          temp_time = (pcbptr[available_pid].run_second * 1000000000) + pcbptr[available_pid].run_nanosecond;
                          increment_timer(temp_time);
                          fprintf(fp, "%-5d: OSS: Receiving that process with PID: %3d ran for %u nanoseconds\n", lines++, available_pid, temp_time);
                          fprintf(fp, "%-5d: OSS: Preempted    PID: %3d Used:  %9ds:%9dns\n", lines++, available_pid, pcbptr[available_pid].run_second, pcbptr[available_pid].run_nanosecond);

                          fprintf(fp, "%-5d: OSS: Preempted    PID: %3d moved to the head of Queue 0 at:  %9ds:%9dns\n", lines++, available_pid, timeptr->sec, timeptr->nanosec);
                          // process enters the head of the same queue after being preempted
                          enqueue_head(queue0, available_pid);
                          nanosec_inc = (rand() % (10000 - 100 + 1)) + 100;
                          increment_timer(nanosec_inc);
                          update_process_time(available_pid);
                       
                  }

        } // end if


        /* scheduler checks if any process in blocked queue is ready to proceed */
        if ((available_pid = blocked_process(blockedpids)) >= 0)
        {
                  increment_timer(10000); //overhead
                  blockedpids[available_pid] = 0; // process is removed from blocked queue
                  fprintf(fp, "%-5d: OSS: Unblocked. PID: %3d TIME: %ds%09dns\n", lines++, available_pid, timeptr->sec, timeptr->nanosec);
                  if (pcbptr[available_pid].priority == 1)
                  {
                         fprintf(fp, "%-5d: OSS: Real Time PID: %3d moved to Queue 0\n", lines++, available_pid);
                         enqueue(queue0, available_pid);  //real time process gets added to queue 0 
                  } else {
                         fprintf(fp, "%-5d: OSS: PID: %3d moved to Queue 1\n", lines++, available_pid);
                         enqueue(queue1, available_pid);   //user process gets added to queue 0
                         pcbptr[available_pid].queue_priority = 1;

                  }

        }  // endif

           
        /* If its not for next process and no process in queue 0 and no blacked process in ready state then proceed with queue 1*/
        if ((iy == 0) && (queue0->length <= 0) && (queue1->length > 0)) {
                  increment_timer(10000); //scheduling overhead
                  available_pid = dequeue(queue1);

                  decision = dispatch_process(available_pid); // send a message to child process indicating the time it should use
                  nanosec_inc = (rand() % (10000 - 100 + 1)) + 100;
                  increment_timer(nanosec_inc);
                  fprintf(fp, "%-5d: OSS: Total time this dispatch was %u nanoseconds\n",lines++, nanosec_inc);

                  /* Depending on the decidion by the child process the scheduling will proceed to take any one of the branches below */
                  if ( decision == 0) // Process decides to terminate
                  {
                        // increment the timer by the duration the process run. since the process has decided to terminate
                        // wait for the child process termination and incremented the num of terminated process
                        temp_time = (pcbptr[available_pid].run_second * 1000000000) + pcbptr[available_pid].run_nanosecond;
                        increment_timer(temp_time);
                        fprintf(fp, "%-5d: OSS: Receiving that process with PID: %3d ran for %u nanoseconds\n", lines++, available_pid, temp_time);
                        k = wait(NULL);
                        if ( k > 0 )
                        {
                                active_process = active_process - 1;
                                terminated_process = terminated_process + 1;
                                availablepids[available_pid] = 1;
                                printf("Master: The child PID %d terminated normally\n",k );

                        }
                        fprintf(fp, "%-5d: OSS: Terminated PID: %3d Used: %ds:%dns\n", lines++, available_pid, pcbptr[available_pid].run_second, pcbptr[available_pid].run_nanosecond);
                        /* Update each process with the cpu time used, wait time and run time and add it to total calculations*/
                        update_process_time(available_pid);
                        total_run_nanosecond = total_run_nanosecond + pcbptr[available_pid].process_run_time;
                        total_cpu_nanosecond = total_cpu_nanosecond + pcbptr[available_pid].process_cpu_time;
                        total_wait_nanosecond = total_wait_nanosecond + pcbptr[available_pid].process_wait_time;
                        total_burst_nanosecond = total_burst_nanosecond + pcbptr[available_pid].process_burst_time;
                        total_block_waitq_time = total_block_waitq_time + pcbptr[available_pid].process_block_waitq_time;
 
                  }
                      
                  if (decision == 1) // process decides to use its full time quantum
                  {
                        // increment the timer by the duration the process run.
                        // processing depends if the process reenters after being pre-empted or regular decision to use full quantum
                        temp_time = (pcbptr[available_pid].run_second * 1000000000) + pcbptr[available_pid].run_nanosecond;
                        increment_timer(temp_time);
                        fprintf(fp, "%-5d: OSS: Receiving that process with PID: %3d ran for %u nanoseconds\n", lines++, available_pid, temp_time);
                        if(pcbptr[available_pid].preempt_complete == 1) // process reenters after being preempted to exhaust its quantum
                        {
                              enqueue(queue1, available_pid);
                              pcbptr[available_pid].preempt_complete = 0;
                              fprintf(fp, "%-5d: OSS: Previously Preempted PID: %3d Completed its quantum - Used: :%9ds:%9dns\n", lines++, available_pid, pcbptr[available_pid].run_second,pcbptr[available_pid].run_nanosecond);
                              fprintf(fp, "%-5d: OSS: PID: %3d moved to tail of Queue 1\n", lines++, available_pid);
                              update_process_time(available_pid);
 

                        } else { //regular decision to use full quantum
                              enqueue(queue2, available_pid);
                              pcbptr[available_pid].queue_priority = 2;
                              fprintf(fp, "%-5d: OSS: Full Quantum PID: %3d Used: :%9ds:%9dns\n", lines++, available_pid, pcbptr[available_pid].run_second,pcbptr[available_pid].run_nanosecond);
                              fprintf(fp, "%-5d: OSS: PID: %3d moved to Queue 2\n", lines++, available_pid);
                              update_process_time(available_pid);

                       }
                  }

                  if (decision == 2) // process decides to get blocked on some event and wait for "event_wait_time"
                  {
                       // increment the timer by the duration the process run.
                       // Add the process to blocked queue
                       temp_time = (pcbptr[available_pid].run_second * 1000000000) + pcbptr[available_pid].run_nanosecond;
                       increment_timer(temp_time);
                       fprintf(fp, "%-5d: OSS: Receiving that process with PID: %3d ran for %u nanoseconds\n", lines++, available_pid, temp_time);
                       blockedpids[available_pid] = 1;
                       pcbptr[available_pid].block_waitq_second = timeptr->sec;
                       pcbptr[available_pid].block_waitq_nanosecond = timeptr->nanosec;      
                       pcbptr[available_pid].ready = FALSE;
                       fprintf(fp, "%-5d: OSS: Blocked    PID: %3d Used: %9ds:%9dns\n", lines++, available_pid, pcbptr[available_pid].run_second, pcbptr[available_pid].run_nanosecond);
                       fprintf(fp, "%-5d: OSS: PID: %3d moved to Blocked queue at %9ds.%9dns\n", lines++, available_pid, timeptr->sec, timeptr->nanosec);
                       temp_time = (pcbptr[available_pid].event_wait_second * 1000000000) + pcbptr[available_pid].event_wait_nanosecond;
                       increment_timer(temp_time);

                       update_process_time(available_pid);

                 }

                 if (decision == 3) //process decides to get preempted by p% of its quantum time
                 {
                       // increment the timer by the duration the process run.
                       temp_time = (pcbptr[available_pid].run_second * 1000000000) + pcbptr[available_pid].run_nanosecond;
                       increment_timer(temp_time);
                       fprintf(fp, "%-5d: OSS: Receiving that process with PID: %3d ran for %u nanoseconds\n", lines++, available_pid, temp_time);
                       fprintf(fp, "%-5d: OSS: Preempted    PID: %3d Used:  %9ds:%9dns\n", lines++, available_pid, pcbptr[available_pid].run_second, pcbptr[available_pid].run_nanosecond);
                       fprintf(fp, "%-5d: OSS: Preempted    PID: %3d moved to the head of Queue 1 at:  %9ds:%9dns\n", lines++, available_pid, timeptr->sec, timeptr->nanosec);
                       // process enters the head of the same queue after being preempted
                       enqueue_head(queue1, available_pid);
                       nanosec_inc = (rand() % (10000 - 100 + 1)) + 100;
                       increment_timer(nanosec_inc);
                       //update timing
                       update_process_time(available_pid);



                 }
 

        } // end if
        /* If its not for next process and no process in queue 0, queue1 and no blacked process in ready state then proceed with queue 2*/  
        if ((iy == 0) && (queue0->length <= 0) && (queue1->length <= 0) && (queue2->length > 0))
        {
                increment_timer(10000); //scheduling overhead
                available_pid = dequeue(queue2);
                decision = dispatch_process(available_pid); // send a message to child process indicating the time it should use
                nanosec_inc = (rand() % (10000 - 100 + 1)) + 100;
                increment_timer(nanosec_inc);
                fprintf(fp, "%-5d: OSS: Total time this dispatch was %u nanoseconds\n",lines++, nanosec_inc);

                /* Depending on the decidion by the child process the scheduling will proceed to take any one of the branches below */
                if ( decision == 0) // Process decides to terminate
                {
                     // increment the timer by the duration the process run. since the process has decided to terminate
                     // wait for the child process termination and incremented the num of terminated process
                     temp_time = (pcbptr[available_pid].run_second * 1000000000) + pcbptr[available_pid].run_nanosecond;
                     increment_timer(temp_time);
                     fprintf(fp, "%-5d: OSS: Receiving that process with PID: %3d ran for %u nanoseconds\n", lines++, available_pid, temp_time);

                     k = wait(NULL);
                     if ( k > 0 )
                     {
                            active_process = active_process - 1;
                            terminated_process = terminated_process + 1;
                            availablepids[available_pid] = 1;
                            printf("Master: The child PID %d terminated normally in Queue 2\n",k );

                     }
                     fprintf(fp, "%-5d: OSS: Terminated PID: %3d Used: %ds:%dns\n", lines++, available_pid, pcbptr[available_pid].run_second, pcbptr[available_pid].run_nanosecond);
                    /* Update each process with the cpu time used, wait time and run time and add it to total calculations*/
                     update_process_time(available_pid);
                     total_run_nanosecond = total_run_nanosecond + pcbptr[available_pid].process_run_time;
                     total_cpu_nanosecond = total_cpu_nanosecond + pcbptr[available_pid].process_cpu_time;
                     total_wait_nanosecond = total_wait_nanosecond + pcbptr[available_pid].process_wait_time;
                     total_burst_nanosecond = total_burst_nanosecond + pcbptr[available_pid].process_burst_time;
                     total_block_waitq_time = total_block_waitq_time + pcbptr[available_pid].process_block_waitq_time;

                }

                if (decision == 1) // process decides to use its full time quantum
                {
                     // increment the timer by the duration the process run.
                     // processing depends if the process reenters after being pre-empted or regular decision to use full quantum
                     temp_time = (pcbptr[available_pid].run_second * 1000000000) + pcbptr[available_pid].run_nanosecond;

                     increment_timer(temp_time);
                     fprintf(fp, "%-5d: OSS: Receiving that process with PID: %3d ran for %u nanoseconds\n", lines++, available_pid, temp_time);
                     if(pcbptr[available_pid].preempt_complete == 1) // process reenters after being preempted to exhaust its quantum
                     {
                           enqueue(queue2, available_pid);
                           pcbptr[available_pid].preempt_complete = 0;

                           fprintf(fp, "%-5d: OSS: Previously Preempted PID: %3d Completed its quantum - Used: :%9ds:%9dns\n", lines++, available_pid, pcbptr[available_pid].run_second,pcbptr[available_pid].run_nanosecond);
                           fprintf(fp, "%-5d: OSS: PID: %3d moved to tail of Queue 2\n", lines++, available_pid);
                           update_process_time(available_pid);

                     } else {          //regular decision to use full quantum
                           enqueue(queue3, available_pid);
                           pcbptr[available_pid].queue_priority = 3;

                           fprintf(fp, "%-5d: OSS: Full Quantum PID: %3d Used: :%9ds:%9dns\n", lines++, available_pid, pcbptr[available_pid].run_second,pcbptr[available_pid].run_nanosecond);
                           fprintf(fp, "%-5d: OSS: PID: %3d moved to Queue 3\n", lines++, available_pid);
                           update_process_time(available_pid);

                     }

               }

               if (decision == 2)           // process decides to get blocked on some event and wait for "event_wait_time"
               {
                     // increment the timer by the duration the process run.
                     // Add the process to blocked queue
                     temp_time = (pcbptr[available_pid].run_second * 1000000000) + pcbptr[available_pid].run_nanosecond;
                     increment_timer(temp_time);
                     fprintf(fp, "%-5d: OSS: Receiving that process with PID: %3d ran for %u nanoseconds\n", lines++, available_pid, temp_time);
                     blockedpids[available_pid] = 1;
                     pcbptr[available_pid].block_waitq_second = timeptr->sec;    // Record time at which the process went to blocked
                     pcbptr[available_pid].block_waitq_nanosecond = timeptr->nanosec;
                     pcbptr[available_pid].ready = FALSE;
                     fprintf(fp, "%-5d: OSS: Blocked    PID: %3d Used: %9ds: %9dns\n", lines++, available_pid, pcbptr[available_pid].run_second, pcbptr[available_pid].run_nanosecond);
                     fprintf(fp, "%-5d: OSS: PID: %3d moved to Blocked queue at %9ds: %9dns\n", lines++, available_pid, timeptr->sec, timeptr->nanosec);
                     temp_time = (pcbptr[available_pid].event_wait_second * 1000000000) + pcbptr[available_pid].event_wait_nanosecond;
                     increment_timer(temp_time);
                     update_process_time(available_pid);

               }

               if (decision == 3)          //process decides to get preempted by p% of its quantum time
               {
                     // increment the timer by the duration the process run.
                     temp_time = (pcbptr[available_pid].run_second * 1000000000) + pcbptr[available_pid].run_nanosecond;
                     increment_timer(temp_time);
                     fprintf(fp, "%-5d: OSS: Receiving that process with PID: %3d ran for %u nanoseconds\n", lines++, available_pid, temp_time);
                     fprintf(fp, "%-5d: OSS: Preempted    PID: %3d Used:  %9ds:%9dns\n", lines++, available_pid, pcbptr[available_pid].run_second, pcbptr[available_pid].run_nanosecond);
                     fprintf(fp, "%-5d: OSS: Preempted    PID: %3d moved to the head of Queue 2 at:  %9ds:%9dns\n", lines++, available_pid, timeptr->sec, timeptr->nanosec);
                     // process enters the head of the same queue after being preempted
                     enqueue_head(queue2, available_pid);
                     nanosec_inc = (rand() % (10000 - 100 + 1)) + 100;
                     increment_timer(nanosec_inc);

                     update_process_time(available_pid);


               } 

        } // end if 

        /* If its not for next process and no process in queue 0, 1, 2 and no blacked process in ready state then proceed with queue 3*/
        if ((iy == 0) && (queue0->length <= 0) && (queue1->length <= 0) && (queue2->length <= 0) && (queue3->length > 0))
        {

               increment_timer(10000);    //scheduling overhead
               available_pid = dequeue(queue3);
               decision = dispatch_process(available_pid);  // send a message to child process indicating the time it should use
               nanosec_inc = (rand() % (10000 - 100 + 1)) + 100;
               increment_timer(nanosec_inc);
               fprintf(fp, "%-5d: OSS: Total time this dispatch was %u nanoseconds\n",lines, nanosec_inc);
              
               /* Depending on the decidion by the child process the scheduling will proceed to take any one of the branches below */
               if ( decision == 0) // Process decides to terminate
               {
                    // increment the timer by the duration the process run. since the process has decided to terminate
                    // wait for the child process termination and incremented the num of terminated process
                    temp_time = (pcbptr[available_pid].run_second * 1000000000) + pcbptr[available_pid].run_nanosecond;
                    increment_timer(temp_time);
                    fprintf(fp, "%-5d: OSS: Receiving that process with PID: %3d ran for %u nanoseconds\n", lines++, available_pid, temp_time);
                    k = wait(NULL);
                    if ( k > 0 )
                    {
                         active_process = active_process - 1;
                         terminated_process = terminated_process + 1;
                         availablepids[available_pid] = 1;
                         printf("Master: The child PID %d terminated normally in Low Q\n",k );
                    }
                    fprintf(fp, "%-5d: OSS: Terminated PID: %3d Used: %ds:%dns\n", lines++, available_pid, pcbptr[available_pid].run_second, pcbptr[available_pid].run_nanosecond);
                    /* Update each process with the cpu time used, wait time and run time and add it to total calculations*/
                    update_process_time(available_pid);
                    total_run_nanosecond = total_run_nanosecond + pcbptr[available_pid].process_run_time;
                    total_cpu_nanosecond = total_cpu_nanosecond + pcbptr[available_pid].process_cpu_time;
                    total_wait_nanosecond = total_wait_nanosecond + pcbptr[available_pid].process_wait_time;
                    total_burst_nanosecond = total_burst_nanosecond + pcbptr[available_pid].process_burst_time;
                    total_block_waitq_time = total_block_waitq_time + pcbptr[available_pid].process_block_waitq_time;

               }
 
               if (decision == 1) // process decides to use its full time quantum
               {
                   // increment the timer by the duration the process run.
                   // processing depends if the process reenters after being pre-empted or regular decision to use full quantum
                   temp_time = (pcbptr[available_pid].run_second * 1000000000) + pcbptr[available_pid].run_nanosecond;
                   increment_timer(temp_time);
                   fprintf(fp, "%-5d: OSS: Receiving that process with PID: %3d ran for %u nanoseconds\n", lines++, available_pid, temp_time);
                   if(pcbptr[available_pid].preempt_complete == 1)    // process reenters after being preempted to exhaust its quantum
                   {
                        enqueue(queue3, available_pid);
                        pcbptr[available_pid].preempt_complete = 0;

                        fprintf(fp, "%-5d: OSS: Previously Preempted PID: %3d Completed its quantum - Used: :%9ds:%9dns\n", lines++, available_pid, pcbptr[available_pid].run_second,pcbptr[available_pid].run_nanosecond);
                        fprintf(fp, "%-5d: OSS: PID: %3d moved to tail of Queue 3\n", lines++, available_pid);
                        update_process_time(available_pid);

                   } else {                  //regular decision to use full quantum
                        enqueue(queue3, available_pid);
                        pcbptr[available_pid].queue_priority = 3;

                        fprintf(fp, "%-5d: OSS: Full Quantum PID: %3d Used: :%9ds:%9dns\n", lines++, available_pid, pcbptr[available_pid].run_second,pcbptr[available_pid].run_nanosecond);
                        fprintf(fp, "%-5d: OSS: PID: %3d Remains in Queue 3\n", lines++, available_pid);
                        update_process_time(available_pid);

                  }
   
               }

               if (decision == 2)    // process decides to get blocked on some event and wait for "event_wait_time"
               {
                   // increment the timer by the duration the process run.
                   // Add the process to blocked queue
                   temp_time = (pcbptr[available_pid].run_second * 1000000000) + pcbptr[available_pid].run_nanosecond;
                   increment_timer(temp_time);
                   fprintf(fp, "%-5d: OSS: Receiving that process with PID: %3d ran for %u nanoseconds\n", lines++, available_pid, temp_time);
                   blockedpids[available_pid] = 1;
                   pcbptr[available_pid].block_waitq_second = timeptr->sec;   // Record time at which the process went to blocked
                   pcbptr[available_pid].block_waitq_nanosecond = timeptr->nanosec;
                   pcbptr[available_pid].ready = FALSE;
                   fprintf(fp, "%-5d: OSS: Blocked    PID: %3d Used:  %9ds:%9dns\n", lines++, available_pid, pcbptr[available_pid].run_second, pcbptr[available_pid].run_nanosecond);
                   fprintf(fp, "%-5d: OSS: PID: %3d moved to Blocked queue at %9ds.%9dns\n", lines++, available_pid, timeptr->sec, timeptr->nanosec);
                   temp_time = (pcbptr[available_pid].event_wait_second * 1000000000) + pcbptr[available_pid].event_wait_nanosecond;
                   increment_timer(temp_time);
                   update_process_time(available_pid);

               }

               if (decision == 3)     //process decides to get preempted by p% of its quantum time
               {
                   // increment the timer by the duration the process run.
                   temp_time = (pcbptr[available_pid].run_second * 1000000000) + pcbptr[available_pid].run_nanosecond;
                   increment_timer(temp_time);
                   fprintf(fp, "%-5d: OSS: Receiving that process with PID: %3d ran for %u nanoseconds\n", lines++, available_pid, temp_time);          
                   fprintf(fp, "%-5d: OSS: Preempted    PID: %3d Used:  %9ds:%9dns\n", lines++, available_pid, pcbptr[available_pid].run_second, pcbptr[available_pid].run_nanosecond);

                   fprintf(fp, "%-5d: OSS: Preempted    PID: %3d moved to the head of Queue 3 at:  %9ds:%9dns\n", lines++, available_pid, timeptr->sec, timeptr->nanosec);
                   // process enters the head of the same queue after being preempted
                   enqueue_head(queue3, available_pid);
                   nanosec_inc = (rand() % (10000 - 100 + 1)) + 100;
                   increment_timer(nanosec_inc);

                   update_process_time(available_pid);

               }


          }  // endif spawn 
   
          /* If its not for next process and no process is ready in any queues then CPU is idle. Record the idle time*/
          if((cpu_status = is_cpuidle(availablepids, blockedpids, next_sec, next_ns, queue0->length, queue1->length, queue2->length, queue3->length)) == 1)
          {
             nanosec_inc = (rand() % (100000000 - 1000 + 1)) + 1000;
             increment_timer(nanosec_inc);
             total_cpu_idle_time = total_cpu_idle_time + nanosec_inc;
             
          }
     

   /* increase the overhead for every loop by 1.xx seconds */
   nanosec_inc = 1000000000 + (rand() % (1000 - 0 + 1)) + 0; 
   increment_timer(nanosec_inc);

  } // end for loop or while loop


   /* All timing calculations */
   cpu_busy_time = total_cpu_nanosecond - total_cpu_idle_time; 
   cpu_utilization = ((float)cpu_busy_time / (float)total_cpu_nanosecond) * 100;
   printf("\n\n\n ****************************************************************************\n");
   printf("***************************** Final Results ***********************************\n");
   printf("*******************************************************************************\n");

   fputs("\n\n\n ****************************************************************************\n", fp);
   fputs("***************************** Final Results ***********************************\n", fp);
   fputs("************************************************************************************\n", fp);

   // printf("cpu busy time = %u, total_cpu_nanosecond = %u, total_cpu_idle_time = %u\n", cpu_busy_time, total_cpu_nanosecond, total_cpu_idle_time);
   // printf("cpu_utilization = %d\n", cpu_utilization);


   printf("Total number of processes generated = %d\n", total_process);
   fprintf(fp, "%-5d: OSS: Total number of processes generated =%d\n", lines++, total_process);
   tot_s = total_cpu_nanosecond/1000000000;
   tot_ns = total_cpu_nanosecond % 1000000000;
   printf("Total CPU time =  %us.%uns \n", tot_s, tot_ns);
   fprintf(fp, "%-5d: OSS: Total CPU time = %us.%uns \n", lines++, tot_s, tot_ns);


   tot_s = total_wait_nanosecond/1000000000;
   tot_ns = total_wait_nanosecond % 1000000000;
   printf("Total wait time = %us.%uns\n", tot_s, tot_ns);
   fprintf(fp, "%-5d: OSS: Total wait time = %us.%uns \n", lines++, tot_s, tot_ns);


   tot_s = cpu_busy_time/1000000000;
   tot_ns = cpu_busy_time % 1000000000;
   //printf("Total CPU busy is %ds.%dns \n", tot_s, tot_ns);
   
   tot_s = total_cpu_idle_time/1000000000;
   tot_ns = total_cpu_idle_time % 1000000000;
   printf("Total CPU idle time = %us. %uns \n", tot_s, tot_ns);
   fprintf(fp, "%-5d: OSS: Total CPU idle time = %us.%uns \n", lines++, tot_s, tot_ns);

   printf("CPU utilization = %u %% \n", cpu_utilization);
   fprintf(fp, "%-5d: OSS: CPU utilization = %u %% \n", lines++, cpu_utilization);


   average_wait_time = total_wait_nanosecond / total_process;
   average_s = average_wait_time/1000000000;
   average_ns = average_wait_time % 1000000000;   
   printf("Average wait time = %us.%uns \n", average_s, average_ns);
   fprintf(fp, "%-5d: OSS: Average wait time = %us.%uns \n", lines++, average_s, average_ns);


   average_waitq_time = total_block_waitq_time / total_process;
   average_s = average_waitq_time/1000000000;
   average_ns = average_waitq_time % 1000000000;
   printf("Average time a process waited in block queue = %us.%uns \n", average_s, average_ns);
   fprintf(fp, "%-5d: OSS: Average time a process waited in block queue = %us.%uns \n", lines++, average_s, average_ns);


   printf("\n ****************************************************************************\n");
   printf("*******************************************************************************\n");
   printf("*******************************************************************************\n");

 


   /* destroy the message queue */
   if( msgctl(msgid, IPC_RMID, NULL) == -1) {

          perror("./oss:msgctl failed");
          return 1;
   } else {
          printf("OSS : Deletion of message queue success\n");
   } 
    

   /* Detach and delete shared memory */
   if (shmdt(timeptr) == -1) {
            perror("shmdt failed");
            return 1;
   } else {
            printf("OSS : Detachment of shared memory Success\n");
   }

   if (shmdt(pcbptr) == -1) {
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

   if (shmctl(pcbid, IPC_RMID, 0) == -1) {
           perror("./oss: shmctl failed");
           return 1;
   } else {
            printf("OSS : Deletion of shared memory Success\n");
   }

   /* Send kill signal to all active children if any abnormal termination */
   for(i = 0; i < total_process; i++)
   {
          if (cpid[i] != 0)
          {
               kill(cpid[i],SIGINT);

          }
   }
       

return 0;

}

/* Func to increment timer by given nanosecond */
void increment_timer(int increment)
{
   
   timeptr->nanosec += increment;
   if(timeptr->nanosec >= 999999999)
   {
        timeptr->sec += 1;
        timeptr->nanosec = 0;
   }

}

/* function to create and initialise PCB */
void create_pcb(int pid, int priority)
{

     pcbptr[pid].pid = pid;
     pcbptr[pid].priority = priority;
     pcbptr[pid].ready = TRUE;
     pcbptr[pid].preempt_complete = 0;
     pcbptr[pid].age = 0;

     if (priority == 1)
       pcbptr[pid].queue_priority = 0;
     else 
       pcbptr[pid].queue_priority = 1;

    pcbptr[pid].burst_second = 0;
    pcbptr[pid].burst_nanosecond = 0;
    pcbptr[pid].event_wait_second = 0;
    pcbptr[pid].event_wait_nanosecond = 0;
    pcbptr[pid].run_second = 0;
    pcbptr[pid].run_nanosecond = 0;
    pcbptr[pid].preempt_balance_nanosecond = 0;
    pcbptr[pid].process_cpu_time = 0;
    pcbptr[pid].process_wait_time = 0;
    pcbptr[pid].process_run_time = 0;
    pcbptr[pid].process_block_waitq_time = 0;

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

/* Returns 0 75 % of the time on average. returns 1 25% of time*/
int set_priority() {

  //srand (time(NULL));
  if ((rand() % 100) <= 75)
    return 0;  // user process
  else 
    return 1;  // real - time processbool TrueFalse = (rand() % 100) < 75;
  
}

/* This function will dispatch the process */
int dispatch_process(int pid)
{
    int priority;
    struct mesg_buffer mesg;
    int quantum1 = 10000000;  
    priority =  pcbptr[pid].queue_priority;
    pcbptr[pid].age = pcbptr[pid].age + 1;

    if(pcbptr[pid].age > 20)
       priority = 0;   // CPU starved process queue priority is increased to give them more CPU time
    quantum = (quantum1 / pow(2.0,(double)priority));  // i = queue #, slice = quantum / 2^i

    if(pcbptr[pid].preempt_balance_nanosecond > 0)
        quantum =  pcbptr[pid].preempt_balance_nanosecond;  //incase of preemption, the remaining quantum time is assigned to exhaust it

    mesg.mesg_type = pid + 1;
    mesg.mesg_value = quantum;

    msgsnd(msgid, &mesg, sizeof(mesg.mesg_value), 0);  // send message to child process

   //printf("Data send is : %d \n", mesg.mesg_value); 

   fprintf(fp, "%-5d: OSS: signaling process with PID: %3d from queue %3d to dispatch\n",lines++, pid, pcbptr[pid].queue_priority);
  

   // msgrcv to receive message
   if((msgrcv(msgid, &mesg, sizeof(mesg.mesg_value), pid + 100, 0)) == -1) 
      perror("./oss: Error: msgrcv ");
   
   
   //printf("Data received is : %d \n", mesg.mesg_value);

 
   fprintf(fp, "%-5d: OSS: dispatching process with PID: %3d from queue %3d at time %9us:%9uns\n", lines++, pid, pcbptr[pid].queue_priority, timeptr->sec, timeptr->nanosec); 
   return mesg.mesg_value;
}


int ready_to_spawn(int pid, unsigned int next_sec, unsigned int next_ns)
{
    if ((pid < 0) || (active_process >= NUM_PCB)) 
      return 0;
    
    if ((next_sec > timeptr->sec) || ((next_sec >= timeptr->sec) && (next_ns > timeptr->nanosec)))
      return 0;

    if (total_process >= MAX_PROCESSES)
       return 0;
  
return 1;

}

/* the function returns if any blocked process is ready to execute */
int blocked_process(int* blockedarray)
{
  int i;
  unsigned int sec = 0, ns = 0, bsec = 0, bns = 0;
  for(i = 0; i < NUM_PCB; i++) 
  {
     sec = 0;
     ns = 0;
     bsec = 0;
     bns = 0;
     if(blockedarray[i] == 1)   //if any process is blocked then calculate if its time to make it ready  
     {
         sec = pcbptr[i].block_second + pcbptr[i].event_wait_second;          
         ns = pcbptr[i].block_nanosecond + pcbptr[i].event_wait_nanosecond;
         if(ns > 1000000000)
         {
             sec = sec + 1;
             ns = ns - 1000000000;
         }

      
         if((sec < timeptr->sec) || ((sec <= timeptr->sec) && (ns < timeptr->nanosec))) //if event_wait_time is passed then make process ready
         {
               pcbptr[i].ready = TRUE;
               bsec = timeptr->sec - pcbptr[i].block_waitq_second;
               bns = abs(timeptr->nanosec - pcbptr[i].block_waitq_nanosecond);   
               pcbptr[i].process_block_waitq_time = pcbptr[i].process_block_waitq_time + ((bsec * 1000000000) + bns);
               return i;
         }
     } 
  }
  return -1;
}  


void update_process_time(int pid)
{
    unsigned long int pburst_ns, pwait_ns, prun_ns;
    pburst_ns = (pcbptr[pid].burst_second * 1000000000) + pcbptr[pid].burst_nanosecond;
    pwait_ns = (pcbptr[pid].event_wait_second * 1000000000) + pcbptr[pid].event_wait_nanosecond;
    prun_ns = (pcbptr[pid].run_second * 1000000000) + pcbptr[pid].run_nanosecond;

    pcbptr[pid].process_cpu_time = pcbptr[pid].process_cpu_time + pburst_ns;
    pcbptr[pid].process_wait_time = pcbptr[pid].process_wait_time + pwait_ns;
    pcbptr[pid].process_run_time = pcbptr[pid].process_run_time + prun_ns;

    //printf("childpid = %d\n", getpid());
    //printf("pburst_ns = %d, pwait_ns = %d, prun_ns = %d\n", pcbptr[pid].burst_nanosecond, pcbptr[pid].event_wait_nanosecond,  pcbptr[pid].run_nanosecond);

} 


int is_cpuidle(int *pids, int* blockedarray,unsigned int next_sec, unsigned int next_ns, int len0, int len1, int len2, int len3)
{
    int available_pid, result, blocked; 
    available_pid = get_available_pid(pids, NUM_PCB);
    result = (ready_to_spawn(available_pid, next_sec,next_ns));
         
    blocked = blocked_process(blockedarray);
    if ((result == 1) || (blocked >= 0) || (len0 > 0) || (len1 > 0) || (len2 > 0) || (len3 > 0))
      return -1;
    else 
      return 1;
   
}

void release_shared_mem()
{
   free(cpid);
   //semctl(semid, 0, IPC_RMID);
   msgctl(msgid, IPC_RMID, NULL);
   shmdt(timeptr);
   shmctl(timeid, IPC_RMID, 0);
   shmdt(pcbptr);
   shmctl(pcbid, IPC_RMID, 0);
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
 *  * It will send kill signal to all active children */
void alarmhandler()
{
    int i;

    printf("\n**************************************************\n");
    printf("Total execution time of program exceeded 100 seconds\n");
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
   
