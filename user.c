#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <ctype.h>
#include <signal.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>

static int *shared;

void sighandler(int sigid){
	printf("Caught signal %d\n", sigid);
	
	//cleanup shared memory
	detachshared();
	
	exit(sigid);
}
int detachshared(){
	if(shmdt(shared) == -1){
		perror("failed to detach from shared memory");
		return -1;
	}else{
		return 1;
	}
}

int main(int argc, char **argv){
	
	//signal handler
	signal(SIGINT, sighandler);
	
	//shared memory
	key_t key;
	int shmid;
	//int *shared;
	int *clock;
	void *shmaddr = NULL;
	
	if((key = ftok("oss.c", 7)) == -1){
		perror("key error");
		return 1;
	} 
	//get the shared memory
	if((shmid = shmget(key, (sizeof(int) * 2), IPC_CREAT | 0666)) == -1){
		perror("failed to create shared memory");
		return 1;
	}
	//attach to shared memory
	if((shared = (int *)shmat(shmid, shmaddr, 0)) == (void *)-1){
		perror("failed to attach");
		if(shmctl(shmid, IPC_RMID, NULL) == -1){
			perror("failed to remove memory seg");
		}
		return 1;
	}
	clock = shared;
	
	int startSec, startNs;//start "time" for process
	startSec = clock[0];
	startNs = clock[1];
	int runTime = rand() % 100000;
	
	//TODO loop over critical section
		//TODO check "time"
		
		//TODO if time is up, send message to oss
		
		//TODO wait for confirmation, then terminate
		
	//code for freeing shared memory
	if(detachshared() == -1){
		return 1;
	}
	/* if(shmdt(shared) == -1){
		perror("failed to detach from shared memory");
		return 1;
	} */
	
	
	return 0;
}