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
#include <sys/msg.h>

//for message queue
#define MSGSZ	12
typedef struct msgbuf {
	long mtype;
	int mtext[MSGSZ];
} message_buf;

//for shared memory clock
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
	int endSec = startSec;
	int endNs = startNs + runTime;
	if(endNs > 1000000000){
		endSec++;
		endNs -= 1000000000;
	}
	
	//message queue
	int msqid;
	key_t msgkey;
	message_buf sbuf, rbuf;
	size_t buf_length = 0;
	
	if((msgkey = ftok("oss.c", 2)) == -1){
		perror("msgkey error");
		return 1;
	}
	if((msqid = msgget(msgkey, 0666)) < 0){
		perror("msgget from user");
		return 1;
	}
	
	//loop for critical section
	int timeisup = 0;
	//while(timeisup == 0){
		//THIS IS NOT WORKING
		if(msgrcv(msqid, &rbuf, MSGSZ, 1, 0) < 0){
			printf("message not received.\n");
		}else{
						
			//check time
			clock = shared;
			printf("time is %d, %d", clock[0], clock[1]);
			if(clock[0] > endSec || clock[1] >= endNs){
				timeisup = 1;
				//TODO send message type 2
				sbuf.mtype = 2;//message type 2 for clock time
				sbuf.mtext[0] = clock[0];
				sbuf.mtext[1] = clock[1];
				buf_length = sizeof(sbuf.mtext);
				if(msgsnd(msqid, &sbuf, buf_length, IPC_NOWAIT) < 0){
					printf("%d, %d\n", msqid, sbuf.mtype);
					perror("time msgsend");
					return 1;
				}else{
					printf("user time up message sent.\n");
				}
			}
			//release critical section
			//message type 1
			sbuf.mtype = 1;
			//sbuf.mtext[0] = 1;
			//buf_length = sizeof(sbuf.mtext) + 1;
			//send message
			if(msgsnd(msqid, &sbuf, buf_length, IPC_NOWAIT) < 0) {
				printf("%d, %d\n", msqid, sbuf.mtype);//, sbuf.mtext[0], buf_length);
				perror("msgsnd");
				return 1;
			}else{
				printf("user message sent.\n");
			}
		}
		
	//}
	
			
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