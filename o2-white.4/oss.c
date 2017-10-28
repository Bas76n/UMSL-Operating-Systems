// Sherd White
// cs4760 Assignment 4  
// 10/04/2017

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <string.h>
#include <semaphore.h>
#include <fcntl.h>
#include <time.h>

#define PERM (S_IRUSR | S_IWUSR)
#define LENGTH 132

typedef struct {
	long total_CPU_time;
	long total_time;
	long last_burst;
	int priority;
	pid_t pid;
	int complete;
} pcb;

typedef struct {
	unsigned int seconds;
	unsigned int nanoseconds;
} timer;

int max_time = 20;
int max_children = 18;
FILE *file;
char *filename = "log";

int main(int argc, char * argv[]) 
{
	int c;
	clock_t begin = clock();
	clock_t end;
	double elapsed_secs;
	int pcb_control[18];
	int total_log_lines = 0;

	while ((c = getopt (argc, argv, "hl:t:")) != -1)
    switch (c)
		  {
			case 'h':
				break;
			case 'l':
				filename = strdup(optarg);
				break;
			case 't':
				max_time = atoi(optarg);
				if (max_time <= 0 || max_time > 60) {
					fprintf (stderr, "Can only specify time between 1 and 60 seconds. \n");
					// perror("Can only specify time between 1 and 60 seconds. \n");
					return 1;
				}
				break;
			case '?':
				if (optopt == 'l'){
					fprintf (stderr, "Option -%c requires an argument. \n", optopt);
					perror("No arguement value given! \n");
					return 1;
				}
				if (optopt == 't'){
					fprintf (stderr, "Option -%c requires an argument. \n", optopt);
					perror("No arguement value given! \n");
					return 1;
				}
				else if (isprint (optopt)){
					fprintf (stderr, "Unknown option `-%c'.\n", optopt);
					perror("Incorrect arguement given! \n");
					return 1;
				}
				else {
					fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);
					perror("Unknown arguement given! \n");
					return 1;
				}
			default:
				exit(1);
		  }
		  
	// open log file
	file = fopen(filename, "w+");

	// create shared memory segment and get the segment id
	// IPC_PRIVATE, child process, created after the parent has obtained the
	// shared memory, so that the private key value can be passed to the child
	// when it is created.  Key could be arbitrary integer also.
	// IPC_CREAT says to create, but don't fail if it is already there
	// IPC_CREAT | IPC_EXCL says to create and fail if it already exists
	// PERM is read write, could also be number, say 0755 like chmod command
	int key = 92111;
	int pcb_id = shmget(key, sizeof(pcb), PERM | IPC_CREAT | IPC_EXCL);
    if (pcb_id == -1) {
        perror("Failed to create shared memory segment. \n");
        return 1;
	}
	// printf("My OS segment id for shared memory is %d\n", pcb_id);
	
	// attach shared memory segment
	pcb* PCB = (pcb*)shmat(pcb_id, NULL, 0);
	// shmat(segment_id, NULL, SHM_RDONLY) to attach to read only memory
    if (PCB == (void*)-1) {
        perror("Failed to attach shared memory segment. \n");
        return 1;
    }
	// printf("My OS shared address is %x\n", shared);
	
	int clock_key = 91514;
	int timer_id = shmget(clock_key, sizeof(timer), PERM | IPC_CREAT | IPC_EXCL);
    if (timer_id == -1) {
        perror("Failed to create shared memory segment. \n");
        return 1;
	}
	// printf("My OS segment id for the msg share is %d\n", timer_id);
	
	// attach shared memory segment
	timer* shmTime = (timer*)shmat(timer_id, NULL, 0);
	// shmat(segment_id, NULL, SHM_RDONLY) to attach to read only memory
    if (shmTime == (void*)-1) {
        perror("Failed to attach shared message segment. \n");
        return 1;
    }
	// printf("My OS message address is %x\n", PCB);
	
	// set shmTime to zero.
	shmTime->seconds = 0;
	shmTime->nanoseconds = 0;
	
	// Initialize named semaphore for shared processes.  Create it if it wasn't created, 
	// 0644 permission. 1 is the initial value of the semaphore
	sem_t *sem = sem_open("BellandJ", O_CREAT | O_EXCL, 0644, 1);
	if(sem == SEM_FAILED) {
        perror("Failed to sem_open. \n");
        return;
    }	
	
	srand(time(NULL));
	unsigned int nano_end = 0;
	unsigned int sec_end = 0;
	unsigned int random_time = rand() % 1000000 + 1;
	if((shmTime->nanoseconds + random_time)  < 1000000000){
			nano_end = shmTime->nanoseconds + rand() % 1000000 + 1;
			sec_end = shmTime->seconds;
		}
	else if((shmTime->nanoseconds + random_time)  >= 1000000000){
		nano_end = (shmTime->nanoseconds + random_time) - shmTime->nanoseconds ;
		sec_end = shmTime->seconds  + 1;
	}
	
	pid_t childpid;
	char cpid[12];
	int i;
	for (i = 0; i < max_children; i++) {
		childpid = fork();
		if (childpid == -1) {
			perror("Failed to fork. \n");
			return 1;
		}
		if (childpid == 0) { /* child code */
			sprintf(cpid, "%d", i);
			execlp("user", "user", cpid, NULL);  // lp for passing arguements
			perror("Child failed to execlp. \n");
			return 1;
		}
	}
	
	printf("Total Children: %d. \n", i);
	
	char shsec[2];
	char shnano[10];
	char msgtext[132];
	while (i > 0){
		shmTime->nanoseconds += 100000;
		end = clock();
		elapsed_secs = (double)(end - begin) / CLOCKS_PER_SEC;  //only reports in seconds.
		if(shmTime->nanoseconds  > 999900000){
			shmTime->nanoseconds  = 0;
			shmTime->seconds  += 1;
		}

		if(PCB->complete == 1){
			sprintf(shsec, "%d", shmTime->seconds);
			sprintf(shnano, "%ld", shmTime->nanoseconds);
			sprintf(msgtext, "Master: Child pid %d is terminating at my time ", PCB->pid);
			fputs(msgtext, file);
			fputs(shsec, file);
			fputs(".", file);
			fputs(shnano, file);
			fputs(". \n", file);
			total_log_lines += 1;
			i--;
			continue;

		}
		if(shmTime->seconds >= 2 || i >= 100 || elapsed_secs > max_time || total_log_lines >= 10000){
			pid_t pid = getpgrp();  // gets process group
			printf("Terminating PID: %i due to limit met. \n", pid);
			sem_close(sem);  // disconnect from semaphore
			sem_unlink("BellandJ"); // destroy if all closed.
			shmctl(pcb_id, IPC_RMID, NULL);
			shmctl(time_id, IPC_RMID, NULL);
			shmdt(PCB);
			shmdt(shmTime);
			killpg(pid, SIGINT);  // kills the process group
			exit(EXIT_SUCCESS);
			// break;
		}
	}
	
	// wait for children
	int j;
	for (j = 0; j <= i; j++){
		wait(NULL);
	}
	printf("All children returned. \n");
	printf("Total Children end: %d. \n", i);
	
    // printf("Msg: %s\n", shmTime->msg);
	
	sem_close(sem);  // disconnect from semaphore
	sem_unlink("BellandJ"); // destroy if all closed.
	 
	// detach from shared memory segment
	int detach = shmdt(PCB);
	if (detach == -1){
		perror("Failed to detach shared memory segment. \n");
		return 1;
	}
	// delete shared memory segment
	int delete_mem = shmctl(pcb_id, IPC_RMID, NULL);
	if (delete_mem == -1){
		perror("Failed to remove shared memory segment. \n");
		return 1;
	}
	
	// detach from msg memory segment
	detach = shmdt(shmTime);
	if (detach == -1){
		perror("Failed to detach msg memory segment. \n");
		return 1;
	}
	// delete msg memory segment
	delete_mem = shmctl(timer_id, IPC_RMID, NULL);
	if (delete_mem == -1){
		perror("Failed to remove msg memory segment. \n");
		return 1;
	}

    return 0;
}