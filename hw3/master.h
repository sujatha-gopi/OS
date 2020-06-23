struct shared_mem
{
   int array[80];

};

void generate_input(int count);
FILE *fileopen(char* filename, char* mode);
void alarmhandler();
static void sighandler(int signo);
void binary_tree(int num);
void release_shared_mem();

