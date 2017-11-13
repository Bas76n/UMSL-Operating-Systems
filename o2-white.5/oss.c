// Sherd White
// cs4760 Assignment 5
// 11/10/2017

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
	unsigned int seconds;
	unsigned int nanoseconds;
} shared_clock;

typedef struct {
	pid_t pid;
	int request;
	int allocation;
	int release;
} shared_resources;

int max_time = 2;
int max_children = 5;
FILE *file;
char *filename = "log";
int verbose = 0;

// void signalHandler();

int main(int argc, char * argv[]) 
{
	
	// Signal Handler
	signal(SIGINT, signalHandler);
	signal(SIGSEGV, signalHandler);
	
	int c;
	clock_t begin = clock();
	clock_t end;
	double elapsed_secs;

	while ((c = getopt (argc, argv, "hs:l:t:v:")) != -1)
    switch (c)
		  {
			case 'h':
				break;
			case 's':
				max_children = atoi(optarg);
				if (max_children <= 0 || max_children > 18) {
					fprintf (stderr, "Can only specify 1 to 18 children. \n");
					// perror("Can only specify 1 to 18 children. \n");
					return 1;
				}
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
			case 'v':
				verbose = atoi(optarg);
				if (verbose < 0 || verbose > 1) {
					fprintf (stderr, "Can only specify verbose as off(0) or on(1).\n");
					// perror("Can only specify verbose as off(0) or on(1). \n");
					return 1;
				}
				break;
			case '?':
				if (optopt == 's'){
					fprintf (stderr, "Option -%c requires an argument. \n", optopt);
					perror("No arguement value given! \n");
					return 1;
				}
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
				if (optopt == 'v'){
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
	int shm_clock_id = shmget(key, sizeof(shared_clock), PERM | IPC_CREAT | IPC_EXCL);
    if (shm_clock_id == -1) {
        perror("Failed to create shared clock segment. \n");
        return 1;
	}
	// printf("My OS segment id for shared memory is %d\n", shm_clock_id);
	
	// attach shared memory segment
	shared_clock* shm_clock = (shared_clock*)shmat(shm_clock_id, NULL, 0);
	// shmat(segment_id, NULL, SHM_RDONLY) to attach to read only memory
    if (shm_clock == (void*)-1) {
        perror("Failed to attach shared clock segment. \n");
        return 1;
    }
	// printf("My OS shared clock address is %x\n", shm_clock);
	
	int rsrc_key = 91514;
	int shm_rsrc_id = shmget(rsrc_key, sizeof(shared_resources)*20, PERM | IPC_CREAT | IPC_EXCL);
    if (shm_rsrc_id == -1) {
        perror("Failed to create shared resources segment. \n");
        return 1;
	}
	// printf("My OS segment id for the resource share is %d\n", shm_rsrc_id);
	
	// attach shared memory segment
	shared_resources* shm_resources = (shared_resources*)shmat(shm_rsrc_id, NULL, 0);
	// shmat(segment_id, NULL, SHM_RDONLY) to attach to read only memory
    if (shm_resources == (void*)-1) {
        perror("Failed to attach shared resources segment. \n");
        return 1;
    }
	// printf("My OS resources address is %x\n", shm_clock);
	
	// Initialize named semaphore for shared processes.  Create it if it wasn't created, 
	// 0644 permission. 1 is the initial value of the semaphore
	sem_t *sem = sem_open("BellandJ", O_CREAT | O_EXCL, 0644, 1);
	if(sem == SEM_FAILED) {
        perror("Failed to sem_open. \n");
        return;
    }	
	
	// set clock to zero
	shm_clock->seconds  = 0;
	shm_clock->nanoseconds  = 0;
	
	// set resources to zero
	for(i = 0; i < 20; i++)
	{
		shm_resources[i].pid = 0;
		shm_resources[i].request = 0;
		shm_resources[i].allocation = 0;
		shm_resources[i].release = 0;
	}
		
	// pid_t childpid;
	// char cpid[12];
	// int i;
	// for (i = 0; i < max_children; i++) {
		// childpid = fork();
		// if (childpid == -1) {
			// perror("Failed to fork. \n");
			// return 1;
		// }
		// if (childpid == 0) { /* child code */
			// sprintf(cpid, "%d", i);
			// execlp("user", "user", cpid, NULL);  // lp for passing arguements
			// perror("Child failed to execlp. \n");
			// return 1;
		// }
	// }
	
	// printf("Total Children: %d. \n", i);
	
	char shsec[2];
	char shnano[10];
	char msgsec[2];
	char msgnano[10];
	char msgtext[132];
	int total_log_lines = 0;
	do {
		if(elapsed_secs >= max_time || i >= max_children || total_log_lines >= 100000){
			pid_t pid = getpgrp();  // gets process group
			printf("Terminating PID: %i due to limit met. \n", pid);
			sem_close(sem);  // disconnect from semaphore
			sem_unlink("BellandJ"); // destroy if all closed.
			shmctl(shm_clock_id, IPC_RMID, NULL);
			shmctl(shm_rsrc_id, IPC_RMID, NULL);
			shmdt(shm_clock);
			shmdt(shm_resources);
			killpg(pid, SIGINT);  // kills the process group
			exit(EXIT_SUCCESS);
			// break;
		}

		if(shm_clock->nanoseconds  > 999990000){
			shm_clock->nanoseconds  = 0;
			shm_clock->seconds  += 1;
		}
		else{
			shm_clock->nanoseconds += 10000;
		}

		if(shm_resources[i].release == 1){
			sprintf(shsec, "%d", shm_clock->seconds);
			sprintf(shnano, "%ld", shm_clock->nanoseconds);
			sprintf(msgtext, "Master: Child pid %d is terminating at my time ", shm_resources->pid);
			fputs(msgtext, file);
			fputs(shsec, file);
			fputs(".", file);
			fputs(shnano, file);
			fputs(".\n", file);
			shm_resources[i].pid = 0;
			shm_resources[i].release = 0;
			i--;
			continue;
		}
		
		end = clock();
		elapsed_secs = (double)(end - begin) / CLOCKS_PER_SEC;  //only reports in seconds.
	} while(elapsed_secs < max_time);
	
	// wait for children
	int j;
	for (j = 0; j <= i; j++){
		wait(NULL);
	}
	printf("All children returned. \n");
	printf("Total Children end: %d. \n", i);
	
	sem_close(sem);  // disconnect from semaphore
	sem_unlink("BellandJ"); // destroy if all closed.
	 
	// detach from shared memory segment
	int detach = shmdt(shm_clock);
	if (detach == -1){
		perror("Failed to detach clock memory segment. \n");
		return 1;
	}
	// delete shared memory segment
	int delete_mem = shmctl(shm_clock_id, IPC_RMID, NULL);
	if (delete_mem == -1){
		perror("Failed to remove shared memory segment. \n");
		return 1;
	}
	
	// detach from msg memory segment
	detach = shmdt(shm_resources);
	if (detach == -1){
		perror("Failed to detach msg memory segment. \n");
		return 1;
	}
	// delete msg memory segment
	delete_mem = shmctl(shm_rsrc_id, IPC_RMID, NULL);
	if (delete_mem == -1){
		perror("Failed to remove msg memory segment. \n");
		return 1;
	}

    return 0;
}

// void signalHandler() {
			
// }