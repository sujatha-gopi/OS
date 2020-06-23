/* Author: Sujatha Gopi
   File Name: dt.c
   Course: 4760 - Operating Systems
   Purpose: The goal of this project is to become familiar with the environment in hoare while practicing system calls. We will be using getopt and perror.
            The programming task requires to create a utility to traverse a specified directory in breadth-first order.
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
#include <stdbool.h>
#include "bt.h"


int main (int argc, char *argv[])
{

    int opt, i = 0;
    char options[20];
    char cwd[200];
    bool sym_link = false;
    char defaultstring[100];

   
    /* using getopt to parse the command line arguements */
    while ((opt = getopt(argc, argv, "hLdgipstul")) != -1)
    {
     
         switch (opt)
         {
       
                // Provides all available options
                case 'h': 
		  	printf("This executable %s will traverse a given directory in breadth first order.\n", argv[0]);
			printf("*******************************************************************************\n");
			printf("\n");
			printf("\nOptions:\n");
			printf("\n");	
			printf("*******************************************************************************\n");
			printf("%s bt [-h] [-L -d -g -i -p -s -t -u | -l] [dirname].\n", argv[0]);
			printf("-h : Print a help message and exit.\n");
			printf("-L : Follow symbolic links, if any. Default will be to not follow symbolic links.\n");
			printf("-d : Show the time of last modification.\n");
			printf("-g : Print the GID associated with the file.\n");
			printf("-i : Print the number of links to file in inode table.\n");
			printf("-p : Print permission bits as rwxrwxrwx.\n");
			printf("-s : Print the size of file in bytes.\n");
			printf("-t : Print information on file type.\n");
			printf("-u : Print the UID associated with the file.\n");
			printf("-l : Print information on the file as if the options \"tpiugs\" are all specified.\n\n");
			return EXIT_SUCCESS;

                // Option L will follow symbolic links, if any. Default will be to not follow symbolic links
                case 'L':

                        sym_link = true;
                        break;
         
                // option 'd' to show the time of last modification
                case 'd':
 
                      	options[i] = 'd';
                      	i++;
                      	break;

                // option 'g' to print the GID associated with the file.
                case 'g':
    
                      	options[i] = 'g';
                      	i++;
                      	break;

                // Option 'i' to print the number of links to file in inode table
                case 'i':

                      	options[i] = 'i';
                      	i++;
                      	break;

                // Option 'p' to print permission bits as rwxrwxrwx
                case 'p':

                      	options[i] = 'p';
                      	i++;
                      	break;

 
                //Option 's' to print the size of file in bytes,KB, MB or GB
                case 's':

                      	options[i] = 's';
                      	i++;
                      	break;

                // Option 't' to print information on file type
                case 't':
    
                      	options[i] = 't';
                      	i++;
                      	break;

                // Option 'u' to print the UID associated with the file
                case 'u':

                      	options[i] = 'u';
                      	i++;
                      	break;

                //Option 'l' will print information on the file as if the options tpiugs are all specified
                case 'l':
                        
                        strcat(options, "tpiugs");
                       	i=i+6;
                      	break;

		default:
	         
                        snprintf(defaultstring, sizeof defaultstring, "Please type %s with \"-h\" option to get more info:", argv[0]);
                        perror(defaultstring); 
			return EXIT_FAILURE;
                
            }
     
     }

     //printf ("The value of options is %s",options);

     /* If user does not specify folder name current working directory will be saved in cwd  
      * If user provides directory then it will be saved in cwd to execute the script */   
     if(argv[optind] == NULL)
     {
        	getcwd(cwd, sizeof(cwd));
    
     }	else {
    
        	strcpy(cwd,argv[optind]);
     }


      
     /* breadth_first function will do the breadth first traversal of the directories
      * and will print all the corresponding file properties */  
     breadth_first(cwd, options, sym_link, argv[0]);

     return 0;        
} 
