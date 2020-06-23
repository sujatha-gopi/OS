struct shared_mem
{
   int sec;
   unsigned int nanosec;
   int prime[80];

};

static void sighandler(int SIG_CODE);
static void alarmhandler();
void release_shared_mem();
void check_timer();
FILE *fileopen(char* filename, char* mode);
void write_prime(char* filename, int total_child);

