/* Author: Sujatha Gopi
 * File Name: oss.c
 * Course: 4760 - Operating Systems
 * Purpose: The goal of this project is to use shared memory and multiple processes to determine if a set of numbers are prime.This project will use getopt, perror, fork and exec.  */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <time.h>
#include "oss.h"
#define SHM_KEY 0x062019

//static void sighandler(int SIG_CODE);
//static void alarmhandler();
//void release_shared_mem();
//void check_timer();
//FILE *fileopen(char* filename, char* mode);
//void write_prime(char* filename, int total_child);

int shmid;
struct shared_mem *shmptr; /* pointer to shared memory*/
pid_t pid;
int *cpid; /* array to hold the PIDs*/
int total_child = 0, n = 4, inc_num = 2, st_num = 57;
char filename[20];
FILE *fp;

int main (int argc, char *argv[])
{

   int opt, addr, status, i, k, flag;
   char *args[10];     /* Array to hold list of arguments for the child process. */
   char str[100], stnum[100], logicalnum[20];
   static char usage[] = "usage: ./oss [-h] [-n x -s x -b B -i I -o filename].\n";
   int  active_child = 0, max_no_child = 2, st_num1;
   int increment = 10000; /*increment for stimulated clock */
   int bflag = 0, iflag = 0, eflag = 0;
 
   strcpy(filename,"output.log");
   
   /* Signal Handlers for CTRL+C and Alarm 
   sighandler will send kill signal to child when a CTRL+C is detected
   alarmhandler will send a kill signal to child when the program runs for more than 2 seconds */
   signal(SIGINT,sighandler);
   signal(SIGALRM, alarmhandler);
   alarm(2);       // This calls the alarm signal handler when the program runs for more than 2 real clock seconds

   //  Below while loop uses getopt to parse command line arguments from the user   
   while ((opt = getopt(argc, argv, ":hn:s:b:i:o:")) != -1)
   {
       switch(opt)
       {
          case 'h':
		
                  printf("This executable %s will find if a number is prime by using child processes and shared memory.\n", argv[0]);
                  printf("*******************************************************************************\n");
                  printf("\n");
                  printf("\nOptions available to execute:\n");
                  printf("\n");
                  printf("*******************************************************************************\n");
                  printf("./%s [-h] [-n x -s x -b B -i I -o filename].\n", argv[0]);
                  printf("-h : Print help message and exit.\n");
                  printf("-n [number] : Indicate the maximum total of child processes oss will ever create. (Default 4).\n");
                  printf("-s [number] : Indicate the number of children allowed to exist in the system at the same time. (Default 2).\n");
                  printf("-b <number> : Start of the sequence of numbers to be tested for primality. (Default 57)\n");
                  printf("-i <number> : Increment between numbers that we test. (Default 2)\n");
                  printf("-o [filename] : Output file. (Default output.log)\n");
 
          case 'n':
                  if((optarg && isdigit(*optarg)))
                  {
                     n = atoi(optarg);
                  }
                  printf("option for  n is %s and n = %d\n",optarg, n );
                  break;

	  case 's':
                  
                  if((optarg && isdigit(*optarg)))
                  {
                         max_no_child  = atoi(optarg);
                  }
                  printf("option for s is %s and s = %d\n",optarg, max_no_child );
                  break;

	  case 'b':
               
                  if((optarg && isdigit(*optarg)))
                  {
                       st_num = atoi(optarg);
                       bflag = 1;
                  }
                  printf("option for  b is %s and st_num = %d\n",optarg, st_num );
                  break;

	  case 'i':

                  if((optarg && isdigit(*optarg)))
                  {

                     inc_num = atoi(optarg);
                     iflag = 1;
                  }
                  printf("option for  i is %s and inc_num = %d\n",optarg, inc_num );
                  break;
 
          case 'o':

                  strcpy(filename,optarg);
                  printf("option for  o is %s and fname = %s\n",optarg, filename );
                  break;

          case '?':

                  eflag = 1;
                  break;
   
       }

   }
    
 /*   if(iflag == 0)  
    {
        printf("%s: Missing required option -i\n", argv[0]);
        printf("%s", usage);
        exit(1);

    } 
     if (bflag == 0)
    {
        printf("%s: Missing required option -b\n", argv[0]);
        printf("%s", usage);
        exit(1);

    } */

    // If receiving unknown options for input exit with error message 
    if (eflag == 1)
    {  
        perror("./oss : Unknown Options\n");
        printf("%s", usage);
        exit(1);
    }


    // call malloc to allocate appropriate number of bytes for the array to store pids
     cpid = (int *) malloc (sizeof(int)*n); // allocate n ints
    
    // Allocate shared memory with the key defined by macro SHM_KEY 
    // If any problem with memory allocation throw error
    shmid = shmget(SHM_KEY, sizeof(struct shared_mem), 0644|IPC_CREAT);
    if (shmid == -1) {
       perror("Shared memory");
       return 1;
    } else {
       printf("OSS: Allocation of shared memory Success\n");
    }

    //Attach to the memory and get a pointer to it.
    shmptr = (struct shared_mem *)shmat(shmid, NULL, 0);
    if (shmptr == (void *) -1) {
       perror("Shared memory attach");
       return 1;
    } else {
       printf("OSS : Attachment of shared memory Success\n");
    }
 
    //OSS sets the shared memory contents to 0
    shmptr->sec = 0;
    shmptr->nanosec = 0;
    //  printf("OSS: Simulated time Seconds: %d Nanoseconds: %u\n",shmptr->sec, shmptr->nanosec);
 
    // open output file for writing
    fp = fileopen(filename, "w");
    strcpy(str,"Child PID  \t\t\t Launched/Terminated \t\t Time\n");
    fputs(str,fp);


    // Make a copy of the starting num from which we find primality 
    // so that it doesnt get modified
    st_num1 = st_num;    

    /* when there is less num of child than what the system can have at any time and
     * when the total num of child spawned < the given number of child then enter a 
     * loop to start forking as many required children */
    while ((active_child < max_no_child) && (total_child < n))
    {
   
        /* check_timer func checks if nanoseconds are increased enough to add 1 sec */
    	check_timer();

        snprintf(stnum, (sizeof(int)* 10 + 1), "%d",st_num1);
        snprintf(logicalnum, (sizeof(int) * 10 + 1), "%d",total_child + 1);

        /* Set up arguments to run an execvp in the child process. */
        /* This will call the prime.c file.) */

        args[0] = "./prime";
        args[1] = "-b";
        args[2] = stnum;
        args[3] = "-l";
        args[4] = logicalnum;
        args[5] = filename;
        args[6] = NULL; /* Indicates the end of arguments. */
        st_num1 = st_num1 + inc_num; 	 // st_num1 gives the next number to find primality
        shmptr->nanosec += increment;	 // increments nanosec

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
             
             printf(" I am child process with a pid of %d, my parents pid is %d!\n",getpid(),getppid());
             // Child process will now execute the exec() command which runs the prime.c program
             int e = execvp(args[0], args);

             // If execvp is successfull child process does not execute any commands below
             if ( e )
                  perror("Error and didn't execute ls");

        } else {

             /* all the below commands are executed by parent process 
              *  parent will increment the num of spawned children and num of children currently active
              *  The pid of the forked child is stored in cpid[] and the child launched time is printed in output file */
              active_child += 1;
              total_child += 1;
              cpid[total_child - 1] = pid;
              printf("OSS: The child PID %d is launched at %d sec and %d ns\n",cpid[total_child - 1],shmptr->sec,shmptr->nanosec );
              sprintf(str, " %d \t\t  Launched \t\t %d sec and %u ns\n", cpid[total_child - 1],shmptr->sec,shmptr->nanosec);
              fputs(str,fp);
              fputs("\n", fp);

       }

     // set a flag to increment nanosec everytime this following loop is executed
     flag = 0;

     /* If there is atleast 1 active child and if the system has already spawned maximum num of child
      * that can be at any given time then do not fork any more children
      * wait for the termination of aleast 1 of the currently active children so that there will be room to 
      * spawn more children */
     while((active_child > 0) && (active_child == max_no_child))
     {
             flag = 1;
             // waitpid will check if any child has terminated. If not will check again while being in this loop
             k = waitpid(-1, &status, WNOHANG);
             if ( k > 0 )
             {
                 active_child = active_child - 1;
                 printf("OSS: The child PID %d terminated normally  at %d sec and %u ns\n",k,shmptr->sec,shmptr->nanosec );
                 sprintf(str, " %d \t\t Terminated\t\t  %d sec and %u ns\n", k,shmptr->sec,shmptr->nanosec);
                 fputs(str,fp);
                 fputs("\n", fp);
       
             }
 
             /* Since this loop can execute any number of time depending on the wait time for the termination
              * of the child, we will increment the nanosec everytime we go thru this loop */      
             if (flag == 1)
             {
                  shmptr->nanosec += increment;
             } 

             check_timer();

     }  
   }

   /* If the system has spawned all the children allowed, then it will no more spawn new children
    * The system will now wait for the termination of any active children before it terminates itself */
   while(active_child > 0)
   {
       k = wait(&status);
       if ( k > 0 )
       {
           active_child = active_child - 1;
           sprintf(str, " %d \t\t  Terminated \t\t%d sec and %u ns\n", k,shmptr->sec,shmptr->nanosec);
           fputs(str,fp);
           fputs("\n", fp);

       }
       check_timer();

   }
     
   printf("OSS: Completion Time %d sec and %u ns\n", shmptr->sec,shmptr->nanosec);

  /* write_prime() will read the shared memery for primality results from child 
   * and will update the output file accordingly */
  write_prime(filename,total_child);
   
  /* When we reach this point, all the children spawned must have been terminated 
   * And we have written the results in the output file. So now its is time for the 
   * OSS to detach and disconnect shared memory */
  if (shmdt(shmptr) == -1) {
      perror("shmdt failed");
      return 1;
  } else {
      printf("OSS : Detachment of shared memory Success\n");
  }

  if (shmctl(shmid, IPC_RMID, 0) == -1) {
      perror("shmctl failed");
      return 1;
  } else {
      printf("OSS : Deletion of shared memory Success\n");
  }


return 0;

}

/* sighandler will get invoked on receiving CTRL + C from user
 * It will send kill signal to all active children */
static void sighandler(int signo)
{
    int i;
    char str[20];
    printf("\n****************\n");
    printf("\nReceived CTRL+C\n");
    printf("\n****************\n");

    for(i = 0; i < total_child; i++)
    {
         if (cpid[i] != 0)
         {
               kill(cpid[i],SIGINT);
 
         }
    }
     
    // write results to output file
    sprintf(str,"%s", filename);
    write_prime(str, total_child);

    // release shared memory
    release_shared_mem();
}

/*  release_shared_mem() wil release all shared memory from OSS before termination 
     due to CTRL+C or Alarm signal */
void release_shared_mem()
{
  //free(cpid);
  shmdt(shmptr);
  shmctl(shmid, IPC_RMID, 0);
  exit(0);
}

/* alarmhandler will get invoked if program runs for more than 2 seconds
 * It will send kill signal to all active children */

void alarmhandler()
{
  int i;
  char str[20];

  printf("\n****************\n");
  printf("Total execution time of program exceeded 2 seconds\n");
  printf("Terminating the program\n");
  printf("\n****************\n");

  for(i = 0; i < total_child; i++)
  {
     if (cpid[i] != 0)
     {
        kill(cpid[i],SIGINT);

     }
  }
  write_prime(filename, total_child);

  release_shared_mem();

}

void check_timer()
{

   if(shmptr->nanosec >= 999999999)
   {
        shmptr->sec += 1;
        shmptr->nanosec = 0;
   }

}

// This function opens the given file in the given mode

FILE *fileopen(char* filename, char* mode)
{
     FILE *fp;
     fp = fopen(filename, mode);
     if (fp == NULL){
        printf("Could not open file %s",filename);
        exit(EXIT_FAILURE);
     }
     return fp;
}

/* This function reads the shared memory and updates the output file with 
 * primality results from children */
void write_prime(char* filename, int total_child)
{
    // FILE *fp;
    int len, i,j = 0, k,count = 1, prime_header = 1, found = 0, st_num1;
    char str[20];
    int nondetermined[total_child], value, negate;
    // fp = fopen(filename, "a");
      if (fp == NULL){
         printf("Could not open file %s",filename);
         exit(EXIT_FAILURE);
      }
 
    // get the numbers for which the children were not able to determine primality
    // Compare them with the original numbers that we sent to child and figure out 
    for (i = 0; i < total_child; i++)
    {
         nondetermined[i] = -1;
    }
    st_num1 = st_num;
    for (k = 1; k <= n; k++)
    {
         negate = -st_num1;
         for (i = 0; i <= total_child; i++)
         {

                if((*(shmptr->prime + i) == st_num1) || (*(shmptr->prime + i) == negate))
                {
                      found = 1;
                } 
         
         }   
         if (found == 0)
         {
                nondetermined[j] = st_num1;
                j = j + 1;
         }

         st_num1 = st_num1 + inc_num;
         found = 0;
    }
    
    while (count < 3)
    {
        for (i = 0; i <= total_child; i++)
        {
              if ((prime_header == 1) && (i == 1))
              {
                  fputs("Prime Numbers\t\t", fp);
              }
              else if ((prime_header == 0) && (i == 1))
              {
                  fputs("Non Prime Numbers\t\t", fp);
              }

              sprintf(str, " %d", *(shmptr->prime + i));
              value = (atoi(str));
              if ((prime_header == 1) && (value > 0)) 
              {

                  fputs(str,fp);
                  fputs("\t",fp);

              } else {
                  if ((prime_header == 0) && (value < 0) && (value != -1))
                  { 
                      fputs(str,fp);
                      fputs("\t",fp);
                  }
              }
       }
       prime_header = 0;
       count += 1;
       fputs("\n", fp);
   }
  
   fputs("Not determined numbers\t", fp);
   for(i = 0; i < total_child; i ++)
   {
        if(nondetermined[i] >= 0)
        { 
            sprintf(str, "%d", nondetermined[i]);
            fputs(str,fp);
            fputs("\t",fp);
        }
   }
   fputs("\n", fp);
    // free(cpid);
}
