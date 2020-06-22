/* Author: Sujatha Gopi
 * File Name: oss.c
 * Course: 4760 - Operating Systems
 * Project Num: 5
 * Purpose: To design and implement a memory management module for our Operating System Simulator oss by using second-chance (clock) page replacement 
 * algorithm.
 *  */

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
struct pcb *pcbptr; // pointer to pcb
struct frame_table *ftptr; // pointer to frame table in shared memory
int *cpid; /* array to hold the PIDs*/
int total_process = 0, active_process = 0, point = 0, lines = 0;
FILE *fp;
int timeid, semid, pcbid, msgid, ftid;



int main (int argc, char *argv[])
{

        int i, k, m = 0;
        char *args[10], stnum[10], memnum[10];    /* Array to hold list of arguments for the child process. */
        int availablepids[MAX_CONCURRENT];  // local bit vector to show available pids
        int devicewaitpids[MAX_CONCURRENT];    //process that wait for device
        int address[MAX_CONCURRENT], action[MAX_CONCURRENT];
        unsigned int next_sec, next_ns, current_ns, display_sec, display_ns;
        int semerror, init, frame_num, pagefaultcounter, frame_pos;
        int opt, eflag, result = -1, pid, available_pid, page, memory_address, terminated_process = 0;
        struct mesg_buffer mesg;
        int action_req, old_owner, old_pid;
        int num_mem_access = 0, page_fault_per_mem_access = 0, mem_access_per_sec = 0;
        float avg_mem_access_speed;


        /* constants that determine the max time process generation */
        const int maxTimeBetweenNewProcsNS = 500000000;
        const int maxTimeBetweenNewProcsSecs = 0;

       

         /* Signal Handlers for CTRL+C and Alarm
          * sighandler will send kill signal to all active children when a CTRL+C is detected
          * alarmhandler will send a kill signal to all active children when the program runs for more than 4 seconds */
        signal(SIGINT,sighandler);
        signal(SIGALRM, alarmhandler);
        alarm(12);  // alarm signal will stop program after 10 real clock seconds

         //  Below while loop uses getopt to parse command line arguments from the user
         while ((opt = getopt(argc, argv, ":hm:")) != -1)
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
                      printf("%s [-hm]\n", argv[0]);
                      printf("-h : Print help message and exit.\n");
                      printf("-m [0 / 1]: Memory access type for child process. -m option takes only 0 or 1 as input\n");
                      return EXIT_SUCCESS;
               
               case 'm':

                      m = atoi(optarg);
                      if(( m < 0) || (m > 1))
                      {
                           printf("m accepts only 0 or 1 as inputs\n");
                           exit(1);
                      } else {
                           printf("Memory reference type selected is %d\n", m);
                      }
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
              printf("OSS: Allocation of shared memory Success\n");
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
       pcbid = shmget(PCB_KEY, (sizeof(struct pcb)*(MAX_CONCURRENT)), 0644|IPC_CREAT);
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


       // Allocate shared memory for frame table with the key defined by macro FT_KEY
       ftid = shmget(FT_KEY, (sizeof(struct frame_table)*(TOTAL_MEMORY)), 0644|IPC_CREAT);
       if (ftid == -1) {
             perror("./oss: Shared memory creation failed");
             return 1;
       } else {
             printf("OSS: Allocation of shared memory Success\n");
       }


       //Attach to the memory and get a pointer to it.
       ftptr = (struct frame_table *)shmat(ftid, NULL, 0);
       if (ftptr == (void *) -1) {
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

       // call malloc to allocate appropriate number of bytes for the array to store pids
       cpid = (int *) malloc (sizeof(int)*MAX_PROCESSES); // allocate ints


       srand (time(NULL)); //seeding
  
       //Initialize shared memory attribute of clock
       timeptr->sec = 0;
       timeptr->nanosec = 0;
       display_sec = timeptr->sec + 1;
       display_ns = timeptr->nanosec;


       // Calculate the time to generate next process using rand()
       next_sec = (rand() % (maxTimeBetweenNewProcsSecs - 0 + 1)) + 0;
       next_ns = (rand() % (maxTimeBetweenNewProcsNS - 1000000 + 1)) + 1000000;
     
       initialise_frame();   //Initialise frame table
 

       // Loop through available pids to set all to 1(available)
       for (i = 0; i < MAX_CONCURRENT; i++) {
             availablepids[i] = 1;  // set all PIDs to be available at beginning
             devicewaitpids[i] = 0;  //set blocked "queue" to empty
             action[i] = -1;
             address[i] = -1;

       }

       // Entering main memory management section. keep doing until all the processes created are terminated
       while(terminated_process < MAX_PROCESSES)
       {

                   /* Get the next available PCB */
                   available_pid = get_available_pid(availablepids, MAX_CONCURRENT);
                  
                 
                   /* If it's time to generate next process and if there is a PCB available to hold the process then go ahead with generation */
                   if(ready_to_spawn(available_pid, next_sec,next_ns))
                   {

                              availablepids[available_pid] = 0; //Mark the PCB/PID as taken
                              /* Initialise the process for the available PCB slot. */
                              initialise_pcb(available_pid);

                              /* Get argument ready for child process in format ./user stnum
                               * where stnum is the starting PCB number which the process should use */
                               snprintf(stnum, sizeof(int), "%d", available_pid);
                               snprintf(memnum, sizeof(int), "%d", m);
                               args[0] = "./user";
                               args[1] = stnum;
                               args[2] = memnum; 
                               args[3] = NULL;   /* Indicates the end of arguments. */


                               // Fork a new process
                               printf("P%d forked at %d.%09dns\n",available_pid, timeptr->sec, timeptr->nanosec);
                               fprintf(fp, "%-5d: OSS: P%d forked at %3ds.%3dns\n", lines++, available_pid,timeptr->sec, timeptr->nanosec);
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
                               active_process += 1;
                               total_process += 1;
                               next_sec = next_sec + (rand() % (maxTimeBetweenNewProcsSecs - 0 + 1)) + 0;  // time for next process generation
                               next_ns = next_ns + (rand() % (maxTimeBetweenNewProcsNS - 1000000 + 1)) + 1000000;
                               if(next_ns >= 1000000000)
                               {
                                      next_sec += 1;
                                      next_ns = next_ns - 1000000000;
                               }


                   }  // spawn endif
                            

                   // check for any messages from children
                   result = msgrcv(msgid, &mesg, (sizeof(mesg)), 1, IPC_NOWAIT);
                   if(result > 0)
                   {
                               available_pid = mesg.pid;

                               if(mesg.mesg_value == 2) // 2 means child wants to terminate
                               {
                                     // increment the timer
                                     increment_timer(semid, 1000000);
                                     available_pid = mesg.pid;
                                     pid = pcbptr[available_pid].realpid;
                                     fprintf(fp, "%-5d: OSS: P%d with PID: %3d requesting to terminate at %3ds.%3dns\n", lines++, available_pid, pid, timeptr->sec, timeptr->nanosec);

                                     // wait for the child process termination and incremented the num of terminated process
                                     k = wait(NULL);
                                     if ( k > 0 )
                                     {
                                            active_process = active_process - 1;
                                            terminated_process = terminated_process + 1;
                                            availablepids[available_pid] = 1;
                                            printf("OSS: Real time process PID %d terminated normally at %ds:%dns\n",k, timeptr->sec, timeptr->nanosec );
                                            fprintf(fp, "%-5d: OSS: P%d with PID: %3d terminated normally at %3ds.%3dns\n", lines++, available_pid, pid, timeptr->sec, timeptr->nanosec);

                                     }
                             
                          
                                    // Release all the memory held by the terminated process
                                    for(i = 0; i < MAX_PAGE; i++)
                                    {

                                            pcbptr[available_pid].pagetable[i].frameno = -1;
                                            pcbptr[available_pid].pagetable[i].valid = -1;
                                            pcbptr[available_pid].pagetable[i].dirty = -1;

                                    }
                                      
                                    // Free up the frame owned by the terminated process
                                    for(i = 0; i < TOTAL_MEMORY; i++)
                                    {
                                      
                                           if(ftptr[i].owner == pid) 
                                           {
                                                ftptr[i].reference = -1;
                                                ftptr[i].owner = -1;
                                           }
                                    }

                               } else if(mesg.mesg_value == 0) // process is requesting for read the memory

                               {
                                    // Get the memory address and calculate the page number that should be read
                                    available_pid = mesg.pid;
                                    pid = pcbptr[available_pid].realpid;
                                    memory_address = mesg.mem_addr;
                                    page = (memory_address / 1024);

                                    fprintf(fp, "%-5d: OSS: P%d with PID: %3d requesting to read from address %d at %3ds.%3dns\n", lines++, available_pid, pid, memory_address, timeptr->sec, timeptr->nanosec);

                                   //Check if the page is in memory from page table
                                   if(pcbptr[available_pid].pagetable[page].valid == 1)    // No page fault
                                   {
                                            frame_num = pcbptr[available_pid].pagetable[page].frameno; //Get the frame num the page is present in
                                            if(ftptr[frame_num].owner == pid) 
                                            {
                                                   ftptr[frame_num].reference = 1;  //Set the reference bit indicating the page has been accessed recently
                                                  //  printf("P%d reads Page %d from frame num %d\n", available_pid, page, frame_num);
                                                   fprintf(fp, "%-5d: OSS: Indicating to P%d with PID: %3d that read has happened from address %d at %3ds.%3dns\n", lines++, available_pid, pid, memory_address, timeptr->sec, timeptr->nanosec);
                                                   mesg.mesg_type = pcbptr[available_pid].realpid;
                                                   mesg.mesg_value = 4;  //SUCCESS
                                                   if (msgsnd(msgid, &mesg, sizeof(mesg), 0) == -1) {
                                                             perror("./user: Error: msgsnd OSS READ GRANTED");
                                                             exit(EXIT_FAILURE);
                                                   }
                                                   // Increment the number of memory access
                                                   num_mem_access++;

                                           } else {  // If there is some confusion in the frame num and owner pid decline READ
                                                  fprintf(fp, "%-5d: OSS: P%d READ declined due to wrong address %d in frame %d at %3ds.%3dns\n", lines++, available_pid, memory_address, frame_num, timeptr->sec, timeptr->nanosec);
                                                  mesg.mesg_type = pcbptr[available_pid].realpid;
                                                  mesg.mesg_value = 5;  //DECLINE
                                                  if (msgsnd(msgid, &mesg, sizeof(mesg), 0) == -1) {
                                                            perror("./user: Error: msgsnd OSS READ DECLINED");
                                                            exit(EXIT_FAILURE);
                                                  }

                                          }
                                          increment_timer(semid, 10);     
                                  } else {
                                   
                                          /* If the page is not in memory it is a page fault. Page should be brought to memory from disk */ 
                                          // printf(" READ : Page %d not is memory - Page Fault for %d\n", page, pid);
                                          fprintf(fp, "%-5d: OSS: P%d address %d, page fault\n", lines++, available_pid, memory_address);
                                          fprintf(fp, "%-5d: OSS: P%d added to device wait queue at %3ds.%3dns\n", lines++, available_pid, timeptr->sec, timeptr->nanosec);
                                          pagefaultcounter++;
                                          devicewaitpids[available_pid] = 1; //add the process to device wait queue
                                          current_ns = (timeptr->sec * 1000000000) + timeptr->nanosec; 
                                          pcbptr[available_pid].wakeupns = current_ns + 14000000;  //calculate the time this process in waiting queue should wakeup
                                          address[available_pid] = mesg.mem_addr;   //save the memory addr and READ/WRITE action for later user
                                          action[available_pid] = mesg.mesg_value;
                                 }


                               } else if(mesg.mesg_value == 1) // process is requesting to WRITE in the memory

                               {    
                                         // Get the memory address and calculate the page number that should be read
                                         available_pid = mesg.pid;
                                         pid = pcbptr[available_pid].realpid;
                                         memory_address = mesg.mem_addr;
                                         page = (memory_address / 1024);
                                         fprintf(fp, "%-5d: OSS: P%d with PID: %3d requesting to write to address %d at %3ds.%3dns\n", lines++, available_pid, pid, memory_address, timeptr->sec, timeptr->nanosec);
                                         //Check if the page is in memory from page table
                                         if(pcbptr[available_pid].pagetable[page].valid == 1)    // No page fault
                                         {  
                                                 frame_num = pcbptr[available_pid].pagetable[page].frameno; //Get the frame num the page is present in

                                                 if((ftptr[frame_num].owner == pid) && (pcbptr[available_pid].pagetable[page].protection == 1))
                                                 {
                                                           ftptr[frame_num].reference = 1;
                                                           pcbptr[available_pid].pagetable[page].dirty = 1; //Set dirty bit to indicate that page is modified
                                                           //printf("P%d writes to Page %d in frame num %d\n", available_pid, page, frame_num);
                                                           fprintf(fp, "%-5d: OSS: Indicating to P%d with PID: %3d that write has happened to address %d at %3ds.%3dns\n", lines++, available_pid, pid, memory_address, timeptr->sec, timeptr->nanosec);
                                                           mesg.mesg_type = pcbptr[available_pid].realpid;
                                                           mesg.mesg_value = 4;  //SUCCESS
                                                           if (msgsnd(msgid, &mesg, sizeof(mesg), 0) == -1) {
                                                                 perror("./user: Error: msgsnd OSS WRITE GRANTED");
                                                                 exit(EXIT_FAILURE);
                                                           }
                                                           num_mem_access++;


                                                 } else {
                                                           if(ftptr[frame_num].owner != pid)
                                                           {
                                                                //printf("P%d trying to access the wrong frame %d \n", available_pid, frame_num);
                                                                fprintf(fp, "%-5d: OSS: P%d WRITE declined due to wrong address %d in frame %d at %3ds.%3dns\n", lines++, available_pid, memory_address, frame_num, timeptr->sec, timeptr->nanosec);
                                                           } else if(pcbptr[available_pid].pagetable[page].protection == 0)
                                                           {
                                                               //printf("P%d does not have write access to page %d \n", available_pid, page);
                                                               fprintf(fp, "%-5d: OSS: P%d does not have write access to page %d\n", lines++, available_pid, page);
                                                           }
                                                           mesg.mesg_type = pcbptr[available_pid].realpid;
                                                           mesg.mesg_value = 5;  //DECLINE
                                                           if (msgsnd(msgid, &mesg, sizeof(mesg), 0) == -1) {
                                                                   perror("./user: Error: msgsnd OSS WRITE DECLINE");
                                                                   exit(EXIT_FAILURE);
                                                           }
                                          
                                                }
                                                increment_timer(semid, 10);
 
                                         } else {
                                     
                                                if(pcbptr[available_pid].pagetable[page].protection == 0) //Deny WRITE access if page does not have persmission
                                                {
                                                         //printf("P%d does not have write access to page %d \n", available_pid, page);
                                                         fprintf(fp, "%-5d: OSS: P%d does not have write access to page %d, page fault\n", lines++, available_pid, page);
                                    
                                                         mesg.mesg_type = pcbptr[available_pid].realpid;
                                                         mesg.mesg_value = 5;  //DECLINE
                                                         if (msgsnd(msgid, &mesg, sizeof(mesg), 0) == -1) {
                                                                 perror("./OSS: Error: msgsnd OSS NO WRITE PERMISSION");
                                                                 exit(EXIT_FAILURE);
                                                         }
                                                } else {
                                                         /* If the page is not in memory it is a page fault. Page should be brought to memory from disk */


                                                         fprintf(fp, "%-5d: OSS: P%d address %d not in memory , page fault\n", lines++, available_pid, memory_address);
                                                         pagefaultcounter++;
                                                         devicewaitpids[available_pid] = 1; //Add process to wait for device
                                                         current_ns = (timeptr->sec * 1000000000) + timeptr->nanosec;
                                                         pcbptr[available_pid].wakeupns = current_ns + 14000000; //Calculate the wakeup time
                                                         address[available_pid] = mesg.mem_addr;  //Store mem addr for later use
                                                         action[available_pid] = mesg.mesg_value;
                               
                                               }
                                               increment_timer(semid, 10000);


                                        } // end page fault condition

                                } // end else if
                   } // end result if
                    
                   /* If there are any process that are ready to wakeup from device wait queue process them */    
                   if ((available_pid = blocked_process(devicewaitpids)) >= 0)
                   {
                               devicewaitpids[available_pid] = 0;  // process is removed from blocked queue
                               pid = pcbptr[available_pid].realpid;
                               memory_address = address[available_pid];
                               page = (memory_address / 1024);
                               action_req = action[available_pid];
                               frame_pos = get_empty_frame(); //Check for any available free frame
                               if(frame_pos == -1)
                               {
                                      printf("No empty frames\n");
                                      fprintf(fp, "%-5d: OSS: No empty frames\n", lines++);
                                      frame_pos = second_chance_page_replacement();  //Incase of no empty frames, use second chance alg to find page to replace

                               }
                     
                               fprintf(fp, "%-5d: OSS: Clearing frame %d and swapping in P%d page %d\n", lines++, frame_pos, available_pid, page);
                               //If the selected frame is previously occupied by another process then update the page table of that process
                               if(ftptr[frame_pos].owner >= 0) 
                               {
                                      old_owner = ftptr[frame_pos].owner;
                                      for(i = 0; i < MAX_CONCURRENT; i++)
                                      {
                                             if(pcbptr[i].realpid == old_owner)
                                                   old_pid = i;
                                           
                                      }
                          
                                      for(i = 0; i < MAX_PAGE; i++)
                                      {
                                             pcbptr[old_pid].pagetable[i].frameno = -1;
                                             pcbptr[old_pid].pagetable[i].valid = 0;
                                             pcbptr[old_pid].pagetable[i].dirty = -1;
                                      }
                               }
                                     
                               //Update the frame table to show the new PID and the page table of the new process should show the frame num     
                               ftptr[frame_pos].owner = pid;
                               ftptr[frame_pos].reference = 1;
                               pcbptr[available_pid].pagetable[page].frameno = frame_pos;
                               pcbptr[available_pid].pagetable[page].valid = 1;       
                               if(action_req == 0)  //READ
                               {
                                       pcbptr[available_pid].pagetable[page].dirty = 0;
                               } else if(action_req == 1)  //WRITE
                               {
                                       pcbptr[available_pid].pagetable[page].dirty = 1;
                                       fprintf(fp, "%-5d: OSS: Dirty bit of frame %d set\n", lines++, frame_pos);
                               }
 

                               address[available_pid] = -1;
                               action[available_pid] = -1;
                               mesg.mesg_type = pcbptr[available_pid].realpid;
                               mesg.mesg_value = 4;  //SUCCESS
                               //printf("Blocked P%d will be released with value = 4,  available_pid = %d\n",pcbptr[available_pid].realpid,available_pid);
                               fprintf(fp, "%-5d: OSS: Indicating to P%d with PID: %3d that write has happened to address %d at %3ds.%3dns\n", lines++, available_pid, pid, memory_address, timeptr->sec, timeptr->nanosec);
                               if (msgsnd(msgid, &mesg, sizeof(mesg), 0) == -1) {
                                       perror("./user: Error: msgsnd OSS BLOCKED GRANTED");
                                       exit(EXIT_FAILURE);
                               }
                               num_mem_access++;
                               increment_timer(semid, 10000);



                         } //end if for blocked 

      
             // Display memory map once every second
            if ((timeptr->sec > display_sec) || ((timeptr->sec >= display_sec) && (timeptr->nanosec > display_ns))) {

                      print_frame_table();
                      display_sec = timeptr->sec + 1;
                      display_ns = timeptr->nanosec;
            }  

            increment_timer(semid, 1000000);

      } //end while
     

       /* Display statistics */
       mem_access_per_sec = (num_mem_access / timeptr->sec);
       page_fault_per_mem_access = (pagefaultcounter / mem_access_per_sec);
       avg_mem_access_speed = ((float)(timeptr->sec + (timeptr->nanosec / 1000000000)) / (float) num_mem_access);
       print_frame_table();

       printf("\n*******************************************************************************\n");
       printf("***************************** Final Results ***********************************\n");
       printf("*******************************************************************************\n");

       fputs("\n**********************************************************************************\n", fp);
       fputs("***************************** Final Results ***********************************\n", fp);
       fputs("************************************************************************************\n", fp);

       printf("Memory reference type selected = %d\n", m + 1);
       printf("Total number of proceses = %d\n", total_process);
       printf("Total simulation time taken for execution = %3ds.%3dns\n", timeptr->sec, timeptr->nanosec);
       printf("Total number of memory accesses  = %d\n", num_mem_access);
       printf("Number of memory access per seconds  = %d\n", mem_access_per_sec);
       printf("Total number of page faults  = %d\n", pagefaultcounter);
       printf("Number of page faults per memory access  = %d\n", page_fault_per_mem_access);
       printf("Average memory access speed = %f\n", avg_mem_access_speed);


       fprintf(fp, "%-5d Total number of proceses = %d\n", lines++, total_process);
       fprintf(fp, "%-5d Total simulation time taken for execution = %3ds.%3dns\n", lines++, timeptr->sec, timeptr->nanosec);
       fprintf(fp, "%-5d Total number of memory accesses = %d\n", lines++, num_mem_access);
       fprintf(fp, "%-5d Number of memory access per seconds = %d\n", lines++, mem_access_per_sec);
       fprintf(fp, "%-5d Total number of page faults = %d\n", lines++, pagefaultcounter);
       fprintf(fp, "%-5d Number of page faults per memory access = %d\n", lines++, page_fault_per_mem_access);
       fprintf(fp, "%-5d Average memory access speed = %f\n", lines++, avg_mem_access_speed);


       printf("\n******************************************************************************\n");
       printf("*******************************************************************************\n");
       printf("*******************************************************************************\n");

       fputs("\n*********************************************************************************\n", fp);
       fputs("***********************************************************************************\n", fp);
       fputs("************************************************************************************\n", fp);

       /*return memory and close file */
       free(cpid);
       fclose(fp);


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


       if (shmdt(ftptr) == -1) {
            perror("shmdt failed");
            return 1;
       } else {
            printf("OSS : Detachment of shared memory Success\n");
       }

       if (shmctl(ftid, IPC_RMID, 0) == -1) {
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
   shmdt(ftptr);
   shmctl(ftid, IPC_RMID, 0);
   exit(0);
}


void sighandler(int signo)
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
    if ((pid < 0) || (active_process >= MAX_CONCURRENT))
      return 0;

    if ((next_sec > timeptr->sec) || ((next_sec >= timeptr->sec) && (next_ns > timeptr->nanosec)))
      return 0;

    if (total_process >= MAX_PROCESSES)
       return 0;

return 1;

}

/* This function initialises the process control block with initial values */
void initialise_pcb(int pid)
{

   int i;

    pcbptr[pid].realpid = pid;

   for(i = 0; i < MAX_PAGE; i++)
    {
        pcbptr[pid].pagetable[i].frameno = -1;
        pcbptr[pid].pagetable[i].protection = (rand() % 2);
        pcbptr[pid].pagetable[i].dirty = -1;
        pcbptr[pid].pagetable[i].valid = -1;

    }


} 


/* This function initialises the frame table */
void initialise_frame()
{
 
   int i;
   for(i = 0; i < TOTAL_MEMORY; i++)
   { 
      ftptr[i].reference = -1;
      ftptr[i].owner = -1;
   }

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
        timeptr->nanosec = timeptr->nanosec - 999999999;
   }


   /* Release semaphore after finishing with file update */
    semerror = semop(semid, semsignal, 1);
    if (semerror == -1) {
        perror("./OSS:Failed to unlock semaphore");
    }


}


// returns the position of the first empty frame in the table. -1 if their are no empty frames
int get_empty_frame() {
  int i;
  for(i = 0; i < TOTAL_MEMORY ; i++) {
    if (ftptr[i].owner == -1) {
      return i;
    }
  }
  return -1;
}
 


// Print frame table
void print_frame_table() 
{
    int i;
    printf("Current memory layout at time %d:%5d is:\n", timeptr->sec, timeptr->nanosec);
    fprintf(fp, "Current memory layout at time %d:%5d is:\n", timeptr->sec, timeptr->nanosec);

    for (i = 0; i < TOTAL_MEMORY; i++) 
    {
        if (ftptr[i].owner >= 0) // > 0 indicates occupied frame
        {
               
                fprintf(fp, "+");
                printf("+");
     
        } else {
  
                fprintf(fp, ".");
                printf(".");
       }
       if((i != 0) && ((i % 64) == 0))
       {
           fprintf(fp, "\n");
           printf("\n");
      }

   }

    fprintf(fp, "\n");
    printf("\n");
}
     

int second_chance_page_replacement() 
{
     int i;
     /* var point acts as a circular queue head which follows the first element in the queue */
     /* Second chance replacement algorithm works as follows: If any frame has reference bit = 0, then it can be replaced becos those pages are not accessed recently. If we come across any frame with reference bit = 1, then it means it is accessed recently and hence it deserves a second chance. So change those pages ref bit to 0 and check the next page */
     if(point != (TOTAL_MEMORY - 1))
        point = point + 1;
     //printf("Starting to check from frame num %d\n", point);
     fprintf(fp, "%-5d: OSS: Second chance algorithm starting to check from frame %d at %3ds.%3dns\n", lines++, point, timeptr->sec, timeptr->nanosec);
     for(i = 0; i < TOTAL_MEMORY; i++)
     {
         if(ftptr[point].reference == 0)
         {
              printf("Replacing frame %d with reference bit %d\n", point, ftptr[point].reference);
              return point; 
         } else {
              printf("Giving second chance to frame %d with reference bit %d\n", point, ftptr[point].reference);
              fprintf(fp, "%-5d: OSS: Ref bit of frame %d changed to 0 to give another chance\n", lines++, point);
              ftptr[point].reference = 0;
         }
 
         if(point == (TOTAL_MEMORY - 1))
            point = 0;
         else 
            point = point + 1;;
          
     }
     return 0;   
}
    

/* the function returns if any blocked process is ready to execute */
int blocked_process(int* blockedarray)
{
    int i;
    unsigned int sec = 0, ns = 0;
    for(i = 0; i < MAX_CONCURRENT; i++)
    {
       sec = 0;
       ns = 0;
       if(blockedarray[i] == 1)   //if any process is blocked then calculate if its time to make it ready
       {
           sec = pcbptr[i].wakeupns/1000000000;
           ns = pcbptr[i].wakeupns % 1000000000;

           if((sec < timeptr->sec) || ((sec <= timeptr->sec) && (ns < timeptr->nanosec))) //if event_wait_time is passed then make process ready
           {
               return i;
           }
       }
    }
  return -1;
}
