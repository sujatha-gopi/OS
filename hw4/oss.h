#define SHM_KEY 0x032011
#define PCB_KEY 0x062019
#define MSG_KEY 0x080781

#define NUM_PCB 18
#define SIZE 1000
#define MAX_PROCESSES 100

#define TRUE 1
#define FALSE 0

struct shared_time
{
   unsigned int sec;
   unsigned int nanosec;

};

struct pcb
{
   int pid;
   int priority;
   int childpid;
   int queue_priority;
   int ready;
   int preempt_complete;
   int age;
   unsigned int burst_second;
   unsigned int burst_nanosecond;
   unsigned int event_wait_second;
   unsigned int event_wait_nanosecond;
   unsigned int block_waitq_second;
   unsigned int block_waitq_nanosecond;
   unsigned int run_second;
   unsigned int run_nanosecond;
   unsigned int block_second;
   unsigned int block_nanosecond;
   unsigned int preempt_balance_nanosecond;
   unsigned int process_burst_time;
   unsigned int process_wait_time;
   unsigned int process_run_time;
   unsigned int process_cpu_time;
   unsigned int process_block_waitq_time;
   

}pcb_t[18];

struct mesg_buffer { 
    long mesg_type; 
    int  mesg_value; 
}; 

struct queue
{
    int items[SIZE];
    int front;
    int rear;
    int length;
};

struct pcb pcb_t[18];
/* This function allocats memory to queue and initialises its pointers */
struct queue* createQueue()
{
    struct queue* q = (struct queue*) malloc(sizeof(struct queue));
    q->front = -1;
    q->rear = -1;
    q->length = 0;
    return q;
}


/* This function checks if the queue is empty */
int isEmpty(struct queue* q)
{
    if((q->rear == -1) || (q->front > q->rear))
       return 1;
    else
        return 0;
}


/*Adds to rear of queue */
void enqueue(struct queue* q, int pid)
{

    if(q->rear == SIZE-1)
        printf("\nQueue is Full!!");
    else
     {
        if(q->front == -1)
            q->front = 0;
        if(q->rear == -1)
            q->rear = 0;

        q->items[q->rear] =  pid;
        q->rear = q->rear + 1;
        q->length += 1;

     }


}


/* Remove from front of the queue */
int dequeue(struct queue* q)
{
    //int item = malloc(1024);
    int pid;
    if(isEmpty(q)){
        printf("Queue is empty");
    } else {
         pid = q->items[q->front];
         q->front = q->front + 1;
         q->length = q->length - 1;

      //  printf("Dequeuing %d\n", pid);
        if(q->front > q->rear)
        {
            printf("Resetting queue");
            q->front = q->rear = -1;

        }


    }

    return pid;
}

/* This is an additional function to print all items from queue
 * This was used for debugging purpose */
void printQueue(struct queue *q)
{
    int i;
    if(isEmpty(q)) {
        printf("Queue is empty");
    } else {
        printf("\nQueue contains \n");
        for(i = q->front; i < q->rear; i++)
               printf("%d\n ", q->items[i]);
             
        
    }
}

/*Adds to head of queue */
void enqueue_head(struct queue* q, int pid)
{

    if(q->rear == SIZE-1)
        printf("\nQueue is Full!!");
    else if(isEmpty(q)){
      //  printf("Queue is empty");
        if(q->front == -1)
            q->front = 0;
        if(q->rear == -1)
            q->rear = 0;
        q->items[q->rear] =  pid;
        q->rear = q->rear + 1;
        q->length += 1;

     } else {
       q->items[q->front - 1] = pid;
       q->front = q->front - 1;
       q->length += 1;
      
     }


}
