/* Author: Sujatha Gopi
   File Name: bfs.c
   Course: 4760 - Operating Systems
   Purpose: This file contains functions for all the queue implementation and bfs traversal and printing the details
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <grp.h>
#include <pwd.h>
#include <time.h>
#include <dirent.h>
#include <errno.h>
#include "bt.h"

/* This function does the actual level order traversal of the directories */
void breadth_first(char * dir_name, char *options, bool symlnk, char *exe_name)
{

  struct stat file_prop;
  DIR * d;
  char *next;
  struct dirent * entry;
  char full_path[100], error_str[100];


  // Create a queue structure using createQueue() to add the directories
  struct queue* q = createQueue();

  // Open the directory specified by user / current working directory
  d = opendir(dir_name);
   
  // Print appropriate error message is not able to open the directory
  if (!d)
  {

     snprintf(error_str, sizeof error_str, "'%s':cannot open directory '%s'",exe_name,dir_name);
     perror(error_str);
     exit (EXIT_FAILURE);
  }

  // Add the root directory to the queue 
  enqueue(q, dir_name);

  /* If symbolic link option is not enabled then use lstat to avoid symbolic links  
     lstat() and stat() returns 0 on successful operation,
     if not successfull in getting file properties  returns -1.
     lstat() returns information about the attributes of the file named by its first operand.
     lstat and stat are similar functions, except lstat does not follow symbolic links. */
  
  if (!(symlnk))
  { 
      if (lstat(dir_name, &file_prop) == 0)
      {

           print_properties(options, file_prop);

      } else {
     
           snprintf(error_str, sizeof error_str, "'%s':Error getting file properties", exe_name);
           perror(error_str);
 
      }
  }
  
  /* If symbolic link option is enabled then use stat to follow symbolic links 
     If stat() or lstat() is successfull, then call the print_properties function to 
     print the required properties of the files and folders.
     If stat() or lstat() fails then return error message using perror */ 
 
  if (symlnk)
  {
      if (stat(dir_name, &file_prop) == 0)
      {

          print_properties(options, file_prop);

      } else {

          snprintf(error_str, sizeof error_str, "'%s':Error getting file properties",exe_name);
          perror(error_str);

      }
 
  }
  
  printf("%s\n", dir_name);
 
  /* While the queue is not empty , then dequeue the first directory from the front of the queue */
  while (! isEmpty(q) ) 
  {
     next = dequeue(q);
     if (!*next)
        continue;

     /* Open that dequeued directory and start visiting/reading the names of every file/folder under it 
      * Add error message if file opening fails */            
     d = opendir(next);
     if (!d)
     {
  
        snprintf(error_str, sizeof error_str, "'%s':cannot open directory '%s'",exe_name,next);
        perror(error_str);
        exit (EXIT_FAILURE);
     } 

 
     /* Start reading every file/folder under the directory. Avoid traversing ".,.. and .git folders" */
     while ((entry = readdir(d)) != NULL)
     {

             if(strcmp(".", entry->d_name) == 0 || strcmp("..", entry->d_name) == 0 || strcmp(".git", entry->d_name) == 0)
             {
                       continue;
	     }
             strcpy(full_path, next);
             strcat(full_path, "/");
             strcat(full_path, entry->d_name);
             /* For every object use lstat to avoid symbolic links and to read file properties.
              If it is a directory ,then add to queue */
             if(!(symlnk))
             { 
                if ( lstat(full_path, &file_prop) == 0)
                {
                    print_properties(options, file_prop);

                    if(S_ISDIR(file_prop.st_mode))
                    {  
                       enqueue(q, full_path);
                  
		    }
 
                } else {
              
                    snprintf(error_str, sizeof error_str, "'%s':Error getting file properties", exe_name);
                    perror(error_str);

               }
            
             } else {
                  
                   /* follow the same procedure of reading file properties and enqueing folders by using 
                    * stat() to follow symbolic link  when symlink is enabled*/  
                   if (symlnk) 
                   {
                      if (stat(full_path, &file_prop) == 0)
                      {  
                          print_properties(options, file_prop);

                          if(S_ISDIR(file_prop.st_mode))
                          {
                              enqueue(q, full_path);

                          } 

                      } else {
                
                          snprintf(error_str, sizeof error_str, "'%s':Error getting file properties", exe_name);
                          perror(error_str);
    
                     }
                  }  

            }
            printf("%s/%s\n", next, entry->d_name);

    }
   // closedir(d);
   }
         
}

/* This function allocats memory to queue and initialises its pointers */
struct queue* createQueue() 
{
    struct queue* q = (struct queue*) malloc(sizeof(struct queue));
    q->front = -1;
    q->rear = -1;
    q->top = 0;
    q->bottom = 0;
    return q;
}

/* This function checks if the queue is empty */
int isEmpty(struct queue* q) 
{
    if((q->rear == -1) || (q->top > q->bottom)) 
       return 1;
    else 
        return 0;
}

/*Adds directories to rear of queue */
void enqueue(struct queue* q, char * value)
{

    if(q->rear == SIZE-1)
        printf("\nQueue is Full!!");
    else 
     {
        if(q->front == -1)
            q->front = 0;
        if(q->rear == -1)
            q->rear = 0;

        strcpy(&(q->items[q->rear]), value);
        q->rear = q->rear + strlen(value);
        q->length[q->bottom] = strlen(value);
        q->bottom++;
       
     } 


}

/* Remove directories from front of the queue */
char * dequeue(struct queue* q)
{
    char * item = malloc(1024);
    int n = q->length[q->top];
    if(isEmpty(q)){
        printf("Queue is empty");
    } else {
         strncpy(item, &(q->items[q->front]),n);
         q->top++;
         q->front = q->front + strlen(item);

        if(q->front > q->rear)
        {
            printf("Resetting queue");
            q->front = q->rear = -1;
            
        }

           
    }
    
    return item;
}


/* This is an additional function to print all items from queue
 * This was used for debugging purpose */
void printQueue(struct queue *q) 
{
    int i = q->front;
    int j = -1;
    if(isEmpty(q)) {
        printf("Queue is empty");
    } else {
        printf("\nQueue contains \n");
        for(i = q->front; i < q->rear + 1; i = i + q->length[j])
        {
               printf("%s\n ", &q->items[i]);
               j = j + 1;
        } 
    }    
}
   

/* This function prints all the required file properties */
void print_properties(char * options, struct stat file_prop)
{


   char time[500], buf[50];
   struct group *grp;
   struct passwd *usr;
   struct tm *tmp ;
   int i, k = 0;
   time_t t; 
   long long int size;
   const char* units[] = {"B", "KB", "MB", "GB", "TB"};
   int g_flag = 0, i_flag = 0, u_flag = 0, p_flag = 0, s_flag = 0, t_flag = 0;   

   
   /* We get the options passed by the user as a string and parse one at a time
    * to calculate each detail and print them 
    * I am using flags like g_flag, i_flag etc to make sure to print these details 
    * only once. Suppose when user provides -gul then without these flags group id, userid
    *  will be printed twice ( once by option l) To avoid that these flags are used to 
    *  print them only once even if user provides it twice*/ 
    
   for(i = 0; i < strlen(options); i++)
   {
     switch(options[i])
      {

        case 'd':
     
            //localtime() uses the time pointed by t ,  to fill a tm structure with the
            //values that represent the last modified time in our case.
            t = file_prop.st_mtime;
            tmp = localtime(&t);

            // use strftime to format time for display
            strftime(time, sizeof(time), "%x - %I:%M%p", tmp);
            printf("%-20s", time);
            break;


       case 'g':

           if (g_flag == 0)
           {
              grp = getgrgid(file_prop.st_gid);
              printf("%-12s", grp->gr_name);
              g_flag = 1;
           }
           break;

       case 'i': 

           if (i_flag == 0)
           {
              //  Printing number of links
              printf("%-5u",(unsigned int)file_prop.st_nlink);
              i_flag =1;
           }
           break;

      case 'u':

           if (u_flag == 0)
           {
              usr = getpwuid(file_prop.st_uid);
              printf("%-7s", usr->pw_name);
              u_flag = 1;
           }
           break;

      case 'p':

          // Reference : http://man7.org/linux/man-pages/man7/inode.7.html
          // http://man7.org/linux/man-pages/man2/stat.2.html
          
          // Checking file permissions for owner
           if (p_flag == 0)
           {
              if (file_prop.st_mode & S_IRUSR)
              {
                  printf("r");
 
              } else {
     
                  printf("-");
              }     


              if (file_prop.st_mode & S_IWUSR)
              {
                  printf("w");

              } else { 

                  printf("-");
              }    
 
              if (file_prop.st_mode & S_IXUSR)
              {
                  printf("x");

              } else {

                 printf("-");
              }    
  
 
              // Checking file permissions for group
              if (file_prop.st_mode & S_IRGRP)
              {
                  printf("r");

              } else {

                  printf("-");
              }  


              if (file_prop.st_mode & S_IWGRP)
              {
                 printf("w");

              } else {

                 printf("-");
              }    

              if (file_prop.st_mode & S_IXGRP)
              {
                 printf("x");

              } else {

                 printf("-");
              }


              // Checking file permissions for othere
              if (file_prop.st_mode & S_IROTH)
              {
                  printf("r");

              } else {

              	 printf("-");
              }   


             if (file_prop.st_mode & S_IWOTH)
             {
                 printf("w");

             } else {

                 printf("-");
             }

             if (file_prop.st_mode & S_IXOTH)
             {
                 printf("x");

             } else {

                 printf("-");
             } 

             // printf("\n");
             printf("%*s",5, "");
             p_flag = 1;
          }
          break;
 
     case 's':
          
          if (s_flag == 0)
          {
             size = (long long)file_prop.st_size;
             while (size > 1024) 
             {
                size = (size / 1024);
                k++;
             }
             sprintf(buf, "%lld%s", size,units[k]);
             printf("%-10s", buf);
             s_flag = 1;
          }
          break;

     case 't':

          if (t_flag == 0)
          {

             // http://man7.org/linux/man-pages/man7/inode.7.html
             // Above link shows example of how to test for file type

             if (S_ISDIR(file_prop.st_mode))
             {
                 printf("Directory");
                 printf("%*s",6, "");
    

             } else if (S_ISLNK(file_prop.st_mode))
   
             {
 
                 printf("Sym_link");
                 printf("%*s",7, "");

             } else if (S_ISREG(file_prop.st_mode))

             {
       
                 printf("File");
                 printf("%*s",11, "");

             } 
             t_flag = 1;
          }
          break;

    }
}
}
