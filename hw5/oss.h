#define SHM_KEY 0x032011
#define PCB_KEY 0x051977
#define RD_KEY 0x062019
#define MSG_KEY 0x080781
#define SEM_KEY 0x061819
#define SIZE 1000
#define MAX_PROCESSES 100
#define MAX_RESOURCES 20
#define MAX_PCB 18
#define TRUE 1
#define FALSE 0


struct shared_time
{
   unsigned int sec;
   unsigned int nanosec;

};


struct resource_descriptor
{
   int resource_id;
   int no_of_instances;
   int available;
   int is_shared_resource;
}rd_t[20];


struct pcb
{
     int pid;
     int maximum[MAX_RESOURCES];
     int allocated[MAX_RESOURCES];
     int requested[MAX_RESOURCES];
     int released[MAX_RESOURCES];
}pcb_t[18];


struct mesg_buffer {
    long mesg_type;
    int mesg_value;
    int pid;
    int rid;
};

struct queue
{
    int items[SIZE];
    int front;
    int rear;
    int length;
};

void initialise_rd();
void initialise_pcb();
void increment_timer(int semid, int increment);
int get_available_pid(int *pids, int num_pcb);
int req_lt_avail(int * req, int * avail, int pnum, int num_res);
void deadlock_detection(int *pids);
void terminate_processes(int available_pid);
void print_matrix();
int ready_to_spawn(int pid, unsigned int next_sec, unsigned int next_ns);
static void sighandler(int signo);
void alarmhandler();


/* This function allocats memory to queue and initialises its pointers */
struct queue* createQueue()
{
    struct queue* q = (struct queue*) malloc(sizeof(struct queue));
    q->front = -1;
    q->rear = -1;
    q->length = 0;
    printf("front = %d, rear = %d, length = %d\n", q->front , q->rear, q->length);
    return q;
}

/* This function checks if the queue is empty */
int isEmpty(struct queue* q)
{
    if((q->rear == -1))
       return 1;
    else
        return 0;
}

/*Adds to rear of queue */
void enqueue(struct queue* q, int pid)
{

    if(q->rear == SIZE-1){
        printf("\nQueue is Full!!");
        printf("rear = %d\n", q->rear);
   } else
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

     int pid;
    if(isEmpty(q)){
        printf("Queue is empty");
    } else {
         pid = q->items[q->front];
         q->front = q->front + 1;
         q->length = q->length - 1;

         if(q->front > q->rear)
        {
            printf("Resetting queue");
            q->front = q->rear = -1;

        }


    }

     return pid;
}

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

