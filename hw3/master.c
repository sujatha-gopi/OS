/* Author: Sujatha Gopi
 * File Name: master.c
 * Course: 4760 - Operating Systems
 * Project Num: 3
 * Purpose: This project computes the summation of n integers in two different ways and assess their
 * performance. This project uses semaphore and shared memory to impement this*/


#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/sem.h>
#include <sys/ipc.h>
#include <fcntl.h>
#include <math.h>
#include "master.h"

#define SHM_KEY 0x032011
#define SEM_KEY 0x061819

/* GLOBAL variables */
int *cpid; /* array to hold the PIDs*/
int shmid, semid;
struct shared_mem *shmptr; /* pointer to shared memory*/
int  active_child = 0, max_no_child = 3, total_child = 0;


int main (int argc, char *argv[])
{
    int  i = 0, j, status, eflag = 0, opt, temp, num = 64, k;
    char filename[20], stnum[10], inc[10];
    char *args[10];     /* Array to hold list of arguments for the child process. */
    int init, semerror, tmp;
    int increment, no_of_groups, sum1, sum2, no_of_child = 0, m2;
    FILE *fp;
    clock_t begin, end;
    double time_spent1, time_spent2;
    pid_t pid;
    sigset_t intmask;
    strcpy(filename,"input.in");


    /* We are trying to ignore all signals except SIGINT and SIGALARM 
     * sigfillset initialializes the set intmask to include all signals
     * defined by the system; sigdelset removes SIGINT and SIGALARM from the set intmask */
    if ( ( sigfillset ( &intmask ) == -1 ) ||
         ( sigdelset ( &intmask, SIGINT ) == -1 ) ||  ( sigdelset ( &intmask, SIGALRM ) == -1 ))
    {
        perror ( "Failed to initialize the signal mask" );
        return ( 1 );
    }

    // Add intmask to the current signal mask
    if ( sigprocmask ( SIG_BLOCK, &intmask, NULL ) == -1 )
    {
         fprintf ( stderr, "Error blocking all other signals except SIGINT and SIGALRM\n");
    }

    /* Signal Handlers for CTRL+C and Alarm
     * sighandler will send kill signal to all active children when a CTRL+C is detected
     * alarmhandler will send a kill signal to all active children when the program runs for more than 150 seconds */
   
     signal(SIGINT,sighandler);
     signal(SIGALRM, alarmhandler);

     #ifdef DEBUG

         printf("\n********************************************************************************\n");
         printf("\n Alarm signal is invoked after 3000 seconds if program runs with sleep times\n");
         printf("\n********************************************************************************\n");
         alarm(3000);

     #else
          printf("\n****************************************************************\n");
          printf("\n Alarm signal will get invoked after 100 seconds if program runs without sleep\n");
          printf("\n****************************************************************\n");

         alarm(100);   // This calls the alarm signal handler when the program runs for more than 100 real clock seconds


     #endif


     alarm(300);   // This calls the alarm signal handler when the program runs for more than 150 real clock seconds

     //  Below while loop uses getopt to parse command line arguments from the user
     while ((opt = getopt(argc, argv, ":hn:m:")) != -1)
     {
        switch(opt)
        {
             case 'h':

                  printf("This executable %s will find summation of n numbers in 2 diff way using semaphores and shared memory.\n", argv[0]);
                  printf("*******************************************************************************\n");
                  printf("\n");
                  printf("\nOptions available to execute:\n");
                  printf("\n");
                  printf("*******************************************************************************\n");
                  printf("./%s [-h] [-n x -m x].\n", argv[0]);
                  printf("-h : Print help message and exit.\n");
                  printf("-n [number] : Number of integers the user wants in input file. (Default 64).\n");
                  printf("-m [number] : Indicate the number of children allowed to exist in the system at the same time. (Default 3).\n");
                  return EXIT_SUCCESS;

             case 'n':

                   num = atoi(optarg);

                   if ((num != 0) && ((num & (num - 1)) != 0))
                   {
                            printf("*******************************************************************************\n");
                            perror("./master : Enter number which is a power of 2\n");
                            printf("*******************************************************************************\n");
                            exit(1);
                   } else {
                         if (num == 0)
                         {
                             printf("*******************************************************************************\n");
                             printf("./master: Enter any number greater than 0 and power of 2\n");
                             printf("*******************************************************************************\n");
                             exit(1);
   
                         }
                   }
                   break;
                   

             case 'm':
                  max_no_child = atoi(optarg);
                  printf("Number of children that can execute concurrently = %d\n", max_no_child );
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


   /* Calculate total num of processes that may be required for this project 
    * This calculation is done to synamically allocate space for array to hold child PIDs */
   tmp = num;
   while (tmp > 1)
   {
       no_of_child += (tmp / 2);
       if ((tmp % 2) == 1)
       {
           tmp += 1;
       }
       tmp = tmp / 2;
   }

   /* Num of child processes that may be needed for nlog mathod */
   m2 = (int) ceil(num / log2(num));
   no_of_child += m2;

   if ((m2 % 2) == 1)
     {
         m2 += 1;
     }
   
   while ((m2) > 1)
   {
      no_of_child += (m2 / 2);
      if ((m2 % 2) == 1)
      {
          m2 += 1;
      }
      m2 = m2 / 2;
   }

   /* Adding 10 more to be on safer side */ 
   no_of_child += 10;

   // call malloc to allocate appropriate number of bytes for the array to store pids
   cpid = (int *) malloc (sizeof(int)*no_of_child); // allocate ints


   // Allocate shared memory with the key defined by macro SHM_KEY
   // If any problem with memory allocation throw error
   shmid = shmget(SHM_KEY, sizeof(struct shared_mem), 0644|IPC_CREAT);
   if (shmid == -1) {
       perror("Shared memory");
       return 1;
   } else {
       printf("Master: Allocation of shared memory Success\n");
   }


   //Attach to the memory and get a pointer to it.
   shmptr = (struct shared_mem *)shmat(shmid, NULL, 0);
   if (shmptr == (void *) -1) {
       perror("Shared memory attach");
       return 1;
   } else {
       printf("Master : Attachment of shared memory Success\n");
   }

   /* Create semaphores for shared processes with key defined by macro SEM_KEY */
   semid = semget(SEM_KEY,1,IPC_CREAT|0666);
   if (semid == -1) {
        perror("./master: Failed to create semaphore");
        exit(EXIT_FAILURE);
   } 

   /* Initialize 0th element of semaphore to 1 */
   init = semctl(semid, 0, SETVAL, 1);  
   if (init == -1) {
        perror("./master:Failed to initialize semaphore element to 1");
        exit(EXIT_FAILURE);
   }

   /* The following function will generate random 'n' integers between 1 - 256 in the file input.in */ 
   generate_input(num);
  
   /* Open the input file in read mode and copy the integers from file to shared memory for the child process to access */
   fp = fileopen(filename,"r");
   while(fscanf(fp, "%d", &temp)==1)
   {
       //printf("%d", temp);
       shmptr->array[i] = temp;
       i++;
   }
   fclose(fp);

   /* Create output log file and write the header details for child process to use it */
   fp = fileopen("adder_log", "w");
   fprintf(fp, "\n\t\t\t\t\t\t\t%s\n", "Binary Tree Method");
   fprintf(fp, "\t%s\t\t %s\t\t %s\t\t %s\t\t %s\t\t\n", "PID", "Index", "Size", "Value added by this child", "Log File Accessed Time");
   fclose(fp);

   /*From now we get into actual summation process by binary tree algorithm
    * Start to record the time taken for summation by binary tree method */
   begin = clock();
   binary_tree(num);
   sum1 = shmptr->array[0];
   end = clock();
   time_spent1 = (double)(end - begin) / CLOCKS_PER_SEC;

   printf("\n*********************************************************\n");
   printf("Final sum by binary tree algorithm = %d \n", sum1);
   printf("Time taken by Binary tree algorithm = %f seconds\n", time_spent1);
   printf("\n*********************************************************\n");


   printf("\n*********************************************************\n");
   printf("\n\tStarting LOGN method \n");
   printf("\n*********************************************************\n");

   /* This part of the code will deal with summation using n/log n method 
    * The child processes have kept only the index values and have made all other values
    * in shared memory = 0. Hence we copy the input values from file to shared memory again */
   fp = fileopen(filename,"r");
   fseek(fp, 0, SEEK_SET);
   i = 0;
   while(fscanf(fp, "%d", &temp)==1)
   {
       shmptr->array[i] = temp;
       i++;
   }


   /* Create output log file and write the header details for child process to use it */
   fp = fileopen("adder_log", "a");
   fprintf(fp, "\n\t\t\t\t\t\t\t%s\n", "N/LogN Method");
   fclose(fp);


   /* we are staring the actual summation by nlogn method 
    * start recording the time taken for calculation 
    * Calculate the no_of_groups and num of elements in each group */

   begin = clock();
   increment = log2(num);
   no_of_groups = (int) ceil(num / log2(num));
   printf("No of groups = %d and increment =  %d\n", no_of_groups, increment);
   for (j = 1; j <= no_of_groups; j++)
   {
       snprintf(stnum, sizeof(int), "%d", increment * (j -1));
       snprintf(inc, sizeof(int), "%d", increment);
       
       /* Get argument ready for child process in format ./bin_adder stnum inc
        * where stnum is the starting shared memory index from which summation should start
        * inc = the number of integers from index that the child process should add */ 
       args[0] = "./bin_adder";
       args[1] = stnum;
       args[2] = inc;
       args[3] = NULL;    /* Indicates the end of arguments. */

      // printf("Values to be added are stnum = %s and increment = %s \n", stnum, inc);        
   
      /* Spawn one child for every group */
      //  printf("Master ready to fork.. \n");
      pid = fork();

      /* Report error if fork fails */
      if (pid == -1) {
            perror("Failed to fork");
      }

        
      // On successful completion, fork() returns 0 to the child process and
      // returns the process ID of the child process to the parent process.
      if (pid == 0) {

             //printf(" I am child process with a pid of %d, my parents pid is %d!\n",getpid(),getppid());
             /* Child process will now execute the exec() command which runs the bin_adder.c program */
             int e = execvp(args[0], args);
             //If execvp is successfull child process does not execute any commands below
             if ( e )
                 perror("Error and didn't execute ./bin_adder");
     
      } else {
 
             /* all the below commands are executed by parent process
              *  parent will increment the num of spawned children and num of children currently active
              *  The pid of the forked child is stored in cpid[]  */

             active_child += 1;
             total_child += 1;
             cpid[total_child - 1] = pid;
            
             /* If there is atleast 1 active child and if the system has already spawned maximum num of child
              * that can be at any given time then do not fork any more children
              * wait for the termination of aleast 1 of the currently active children so that there will be room to
              * spawn more children */

             while((active_child > 0) && (active_child == max_no_child))
             {
                  k = waitpid(-1, &status, WNOHANG);
                  if ( k > 0 )
                  {
                       active_child = active_child - 1;
                       printf("Master: The child PID %d terminated normally\n",k );

                  }

             }
       
      }    
  }

 /* If the system has spawned all the children allowed, then it will no more spawn new children
  * The system will now wait for the termination of any active children before it terminates itself */
 
  while(active_child > 0)
  {
       k = waitpid(-1, &status, WNOHANG);
       if ( k > 0 )
       {
           active_child = active_child - 1;
           printf("Master: The child PID %d terminated normally\n",k );
       }

  }

  /* After all active children are terminated, we copy the result of each group to the beginning 
   * locations of the shared memory to make it easy for binary tree algorithm to process the rest */ 
  if (active_child == 0)
  {
       for (j = 1; j <= no_of_groups; j++)
       {
           shmptr->array[j-1] = shmptr->array[increment * (j -1)];
           // printf("*** %d = %d\n", j-1, shmptr->array[j-1]);
       }
  
       /* Binary tree alg can work only on even number. So if num of results from n/logn groups is odd
        * we add one dummy result and make it even */
       if ((no_of_groups % 2 ) == 1)
       {
          no_of_groups += 1;
          shmptr->array[no_of_groups - 1] = 0;

       }

  }


  /* we now call the binary tree algorithm to calculate the summation from the results of nlogn groups */
  binary_tree(no_of_groups);
  sum2 = shmptr->array[0];
  printf("Total num of children is %d \n", total_child - 10);
  end = clock();
  time_spent2 = (double)(end - begin) / CLOCKS_PER_SEC;

  printf("\n*********************************************************\n");
  printf("Final sum by n/logn method  = %d \n", sum2);
  printf("Time taken by n/logn method = %f seconds\n", time_spent2);
  printf("\n*********************************************************\n");


  /* Remove semaphore */
  semerror = semctl(semid, 0, IPC_RMID);
  if (semerror == -1) {
        perror("./master:Failed to remove semaphore");
  }
      
 
  printf("\n*******************RESULTS*********************\n");
  printf("\n1. BINARY TREE METHOD\n");
  printf("Final sum = %d \n", sum1);

  printf("Time taken to execute = %f\n", time_spent1);

  printf("\n2. NLOGN  METHOD\n");

  printf("Final sum = %d\n", sum2);

  printf("Time taken to execute = %f\n", time_spent2);

  if (time_spent1 > time_spent2)
  {
      printf("\nNLOGN method has better performance for summation of %d integers\n", num);
 
  } else if (time_spent1 < time_spent2) {
    
     printf("\nBinary Tree method has better performance for summation of %d integers\n", num);

  } else if (time_spent1 == time_spent2) {

     printf("\nBoth Binary Tree method and N/LogN method has same performance for summation of %d integers\n", num);
     printf("\nTry again or with another set of integers to confirm\n");

  }

  printf("\n***********************************************\n");


  /* Detach and delete shared memory */ 
  if (shmdt(shmptr) == -1) {
      perror("shmdt failed");
      return 1;
   } else {
      printf("Master : Detachment of shared memory Success\n");
   }

   if (shmctl(shmid, IPC_RMID, 0) == -1) {
      perror("shmctl failed");
      return 1;
   } else {
      printf("Master : Deletion of shared memory Success\n");
   }


return 0;
}

/* sighandler will get invoked on receiving CTRL + C from user
 * It will send kill signal to all active children */

static void sighandler(int signo)
{
    
    int i;
    printf("\n***************************\n");
    printf("\n\tReceived CTRL+C\n");
    printf("\n***************************\n");
    
    /* Send kill signal to all active children */
    for(i = 0; i < total_child; i++)
    {
         if (cpid[i] != 0)
         {
               kill(cpid[i],SIGINT);

         }
    }
    
    // cleanup semaphores and shared memory
    release_shared_mem();

}


/*  release_shared_mem() wil release all shared memory, delete semaphores and free dynamic memory before termination
 *   due to CTRL+C or Alarm signal */

void release_shared_mem()
{
   free(cpid);
   semctl(semid, 0, IPC_RMID);
   shmdt(shmptr);
   shmctl(shmid, IPC_RMID, 0);
   exit(0);
}

/* alarmhandler will get invoked if program runs for more than 150 seconds
 * It will send kill signal to all active children */

void alarmhandler()
{
    int i;

    printf("\n**************************************************\n");
    printf("Total execution time of program exceeded 100 seconds\n");
    printf("Terminating the program\n");
    printf("\n**************************************************\n");

    for(i = 0; i < total_child; i++)
    {
         if (cpid[i] != 0)
         {
               kill(cpid[i],SIGINT);
  
         }
    }
 
    // Cleanup semaphores and shared memory  
    release_shared_mem();
 
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


/* This function will generate random integers between 1 - 256 */
void generate_input(int count)
{
   
   int i;
   int num[count];
   FILE *fp;
   fp = fileopen("input.in","w");
   srand(time(0));
   for (i = 0; i < count; i++) { 
        num[i] = (rand() % (256 - 1 + 1)) + 1; 
        //printf("%d ", num[i]); 
    } 
 
   // write the generated integers in input file
   for (i = 0; i < count; i= i+2) {
       fprintf(fp, "%d \n%d \n", num[i], num[i+1]);

   }
   fclose(fp);
}
 

/* Calculate the summation using binary tree algorithm */  
void binary_tree(int num)
{

   int i, j, k,  total_num, value = 1, increment, count;
   char *args[10]; 
   char stnum[10], inc[10];
   int status, flag = 0;
   pid_t pid;
 
   total_num = num;
   i = -1;
   while(total_num >= 1)
   {
       value = value * 2;
       i ++;
       for (j = 1; j <= total_num/2; j++)
       {

           count = 1;
           increment = 1;
           while(count <= i)
           {
               increment = increment * 2;
               count ++;
           } 


           snprintf(stnum, sizeof(int), "%d", value * (j -1));
           snprintf(inc, sizeof(int), "%d", increment + 1);


      	  /* Set up arguments to run an execvp in the child process. */
          /* This will call the bin_adder.c file.) */
     
          args[0] = "./bin_adder";
          args[1] = stnum;
          args[2] = inc;
          args[3] = NULL;

          // Fork a new process
          //printf("Master ready to fork.. \n");
          pid = fork();

          // Display error if fork fails
          if (pid == -1) {
             perror("Failed to fork");
          }

          // On successful completion, fork() returns 0 to the child process and
          // returns the process ID of the child process to the parent process.
          if (pid == 0) {
              //  printf(" I am child process with a pid of %d, my parents pid is %d!\n",getpid(),getppid());

              int e = execvp(args[0], args);
              // If execvp is successfull child process does not execute any commands below
              if ( e )
                  perror("Error and didn't execute ./bin_adder");

          } else {
    
             /* all the below commands are executed by parent proces
              *  parent will increment the num of spawned children and num of children currently active
              *  The pid of the forked child is stored in cpid[] and the child launched time is printed in output file */
              active_child += 1;
              total_child += 1;
              cpid[total_child - 1] = pid;
              while((active_child > 0) && (active_child == max_no_child))
              {
                   k = waitpid(-1, &status, WNOHANG);
                   if ( k > 0 )
                   {
                        active_child = active_child - 1;
                        printf("Master: The child PID %d terminated normally\n",k );

                   }

              }
          }  

        }

   if ( (total_num != 1) && (total_num % 2) == 1)
   {
      total_num += 1;
   }

   if (total_num == 1)
   {
       flag = 1;
   }
   if (flag == 1)
   {
       break;
   }
   total_num = total_num/2;
   while(active_child > 0)
   {
        k = wait(&status);
        if ( k > 0 )
        {
            active_child = active_child - 1;
            printf("Master: The child PID %d terminated normally\n",k );

        }

   }


   }

   while(active_child > 0)
   {
        k = wait(&status);
        if ( k > 0 )
        {
            active_child = active_child - 1;
            printf("Master: The child PID %d terminated normally\n",k );

        }

   }

} 
