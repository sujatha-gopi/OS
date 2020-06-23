/* Author: Sujatha Gopi
 * File Name: dt.h
 * Course: 4760 - Operating Systems
 * Project: 1
 * Purpose: This .h file contains the struct and function declaration
 *     */

#include <stdio.h>
#include <stdbool.h>
#define SIZE 1000


struct queue
{
    char items[SIZE];
    int front;
    int rear;
    int bottom;
    int length[SIZE];
    int top;
};

struct queue* createQueue();
void enqueue(struct queue* q, char * value);
char * dequeue(struct queue* q);
int isEmpty(struct queue* q);
void printQueue(struct queue* q);
void breadth_first(char * dir_name, char *options, bool symlnk, char * exe_name);
void print_properties(char * options, struct stat file_prop);

