#define SHM_KEY 0x032011
#define PCB_KEY 0x051977
#define FT_KEY 0x062019
#define MSG_KEY 0x080781
#define SEM_KEY 0x061819
#define MAX_PROCESSES 100
#define MAX_CONCURRENT 18
#define MAX_PAGE 32
#define TOTAL_MEMORY 256


void sighandler(int signo);
void alarmhandler();
int get_available_pid(int *pids, int num_pcb);
int ready_to_spawn(int pid, unsigned int next_sec, unsigned int next_ns);
void increment_timer(int semid, int increment);
void initialise_pcb(int pid);
void initialise_frame();
int get_empty_frame();
void print_frame_table();
int blocked_process(int* blockedarray);
int second_chance_page_replacement();
void release_shared_mem();

struct shared_time
{
   unsigned int sec;
   unsigned int nanosec;

};


typedef struct
{
        int frameno;
	int protection; // protection type on that page: read only or read and write. (0 : read only | 1 : read and write) 
	int dirty;      //Is set if the page has been modified. (0 : No  | 1 : Yes)
	int valid;      //Indicates if the page is in memory or not. (0: No | 1 : Yes)
}page_table;


struct pcb
{
     int realpid;
     unsigned int wakeupns;
     page_table pagetable[MAX_PAGE];
}pcb_t[MAX_CONCURRENT];


struct frame_table
{
      int reference;
      int owner;
}frame[TOTAL_MEMORY];

struct mesg_buffer 
{
    long mesg_type;
    int mesg_value;
    int pid;
    int page_num;
    int mem_addr;
};



