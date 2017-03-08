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
#include <errno.h>

//for message queue
#define MSGSZ	12
typedef struct msgbuf {
	long mtype;
	int mtext[MSGSZ];
} message_buf;

//for shared memory
static int *shared;
static int shmid;
//for pids
static pid_t *pidptr;
//for message queue
static int msqid;

void sighandler(int sigid){
	printf("Caught signal %d\n", sigid);
	//send kill message to children
	//access pids[] to kill each child
	int i = 0;
	while(pidptr[i] != '\0'){
		if(pidptr[i] != 0){
			kill(pidptr[i], SIGQUIT);
		}
		i++;
	}
	//kill(0, SIGQUIT);
	//cleanup shared memory
	detachshared();
	removeshared();
	deletequeue();
	exit(sigid);
}
int deletequeue(){
	//delete message queue
	struct msqid_ds *buf;
	if(msgctl(msqid, IPC_RMID, buf) == -1){
		perror("msgctl: remove queue failed.");
		return -1;
	}
}
int detachshared(){
	if(shmdt(shared) == -1){
		perror("failed to detach from shared memory");
		return -1;
	}
	
}
int removeshared(){
	if(shmctl(shmid, IPC_RMID, NULL) == -1){
		perror("failed to delete shared memory");
		return -1;
	}
}
int main(int argc, char **argv){
	
	
	//getopt
	extern char *optarg;
	extern int optind;
	int c, err = 0;
	int hflag=0, sflag=0, lflag=0, tflag=0;
	static char usage[] = "usage: %s -h  \n-l filename \n-i y \n-t z\n";
	
	char *filename, *x, *z;
	
	while((c = getopt(argc, argv, "hs:l:i:t:")) != -1)
		switch (c) {
			case 'h':
				hflag = 1;
				break;
			case 's':
				sflag = 1;
				x = optarg;//max number of slave processes
				break;
			case 'l':
				lflag = 1;
				filename = optarg;//log file 
				break;
			
			case 't':
				tflag = 1;
				z = optarg;//time until master terminates
				break;
			case '?':
				err = 1;
				break;
		}
		
	if(err){
		fprintf(stderr, usage, argv[0]);
		exit(1);
	}
	//help
	if(hflag){
		puts("-h for help\n-l to name log file\n-s for number of slaves\n-i for number of increments per slave\n-t time for master termination\n");
	}
	//set default filename for log
	if(lflag == 0){
		filename = "test.out";
	}
	puts(filename);
	//number of slaves
	int numSlaves=5;
	if(sflag){//change numSlaves
		numSlaves = atoi(x);
	}
	//puts(x);
	
	//time in seconds for master to terminate
	int endTime=20;
	if(tflag){//change endTime
		endTime = atoi(z);
	}
	//puts(z);
	
	//message queue
	//int msqid;
	key_t msgkey;
	message_buf sbuf, rbuf;
	size_t buf_length = 0;
	
	if((msgkey = ftok("oss.c", 2)) == -1){
		perror("msgkey error");
		return 1;
	}
	if((msqid = msgget(msgkey, IPC_CREAT | 0666)) < 0){
		perror("msgget from oss");
		return 1;
	}
	//message type 1
	sbuf.mtype = 1;
	//sbuf.mtext[0] = 1;
	//buf_length = sizeof(sbuf.mtext) + 1;
	//send message
	if(msgsnd(msqid, &sbuf, buf_length, IPC_NOWAIT) < 0) {
		printf("%d, %d, %d, %d\n", msqid, sbuf.mtype);//, sbuf.mtext[0], buf_length);
		perror("msgsnd");
		return 1;
	}else{
		printf("message sent.\n");
	}
	//to use delete queue and to access msg_lspid
	//struct msqid_ds *buf;
	
	//shared memory
	key_t key;
	//int shmid;
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
	clock[0] = 0;//initialize "clock" to zero
	clock[1] = 0;
	
	//create start time
	struct timespec start, now;
	clockid_t clockid;//clockid for timer
	clockid = CLOCK_REALTIME;
	long starttime, nowtime;
	if(clock_gettime(clockid, &start) == 0){
		starttime = start.tv_sec;
	}
	if(clock_gettime(clockid, &now) == 0){
		nowtime = now.tv_sec;
	}
	
	//begin forking children
	pid_t pids[numSlaves];//can i have a pointer to this? pid_t *pidptr
	pidptr = pids;
	int i;
	for(i = 0; i < numSlaves; i++){
		pids[i] = fork();
		if(pids[i] == -1){
			perror("Failed to fork");
			return 1;
		}
		if(pids[i] == 0){
			execl("user", "user", NULL);
			perror("Child failed to exec user");
			return 1;
		}
			
	}
	
	int totalProcesses = numSlaves;//keep count of total processes created
	int n = numSlaves;
	int status;
	pid_t pid;
	int childsec, childns;//for time sent by child
	while(totalProcesses < 100 && clock[0] < 2 && (nowtime - starttime) < endTime){
		//signal handler
		signal(SIGINT, sighandler);
		
		//increment "system" clock
		clock[1] += 100000;
		if(clock[1] > 1000000000){
			clock[0] += 1;
			clock[1] -= 1000000000;
		}
		//check for messages from children
		if(n > 0){
			//check mailbox for msg
			//get time from child
			errno = 0;
			if(msgrcv(msqid, &rbuf, MSGSZ, 2, MSG_NOERROR | IPC_NOWAIT) < 0){
				if(errno != ENOMSG){
					perror("msgrcv in oss");
					return 1;
				}
				//printf("message time up from user not received.\n");
			}else{
				childsec = rbuf.mtext[0];
				childns = rbuf.mtext[1];
				printf("time up message from user received.\n");
				
				//write to file
				FILE *logfile;
				logfile = fopen(filename, "a");
				if(logfile == NULL){
					perror("Log file failed to open");
					return -1;
				}
				fprintf(logfile, "Master: Child pid is terminating at my time %d %d because it reached %d %d in slave\n", clock[0], clock[1], childsec, childns);
				fclose(logfile);
				
				pid = wait(&status);//make sure child terminated
				printf("User process %ld exited with status 0x%x.\n", (long)pid, status);
				n--;
				//find pid in pids[]
				int x;
				for(x = 0; x < numSlaves; x++){
					if(pids[x] == pid){
						printf("found pid that terminated.\n");
						//fork new process
						pids[x] = fork();
						if(pids[x] == -1){
							perror("Failed to fork");
							return 1;
						}
						if(pids[x] == 0){
							execl("user", "user", NULL);
							perror("Child failed to exec user");
							return 1;
						}
						n++;
						totalProcesses++;
					}
				}
				
		
			}
				
			
			
		}
		
		
		//get current time
		if(clock_gettime(clockid, &now) == 0){
			nowtime = now.tv_sec;
		}
	}
	//pid = wait(&status);
	//printf("User process %ld exited with status 0x%x.\n", (long)pid, status);
	//terminate children
	while(n > 0){
		n--;
		kill(pids[n], SIGQUIT);
	}
	//kill(0, SIGQUIT);
	printf("%d total processes were created.\n", totalProcesses);
	//code for freeing shared memory
	if(detachshared() == -1){
		return 1;
	}
	if(removeshared() == -1){
		return 1;
	}
	/* if(shmdt(shared) == -1){
		perror("failed to detach from shared memory");
		return 1;
	}
	if(shmctl(shmid, IPC_RMID, NULL) == -1){
		perror("failed to delete shared memory");
		return 1;
	} */
	
	//delete message queue
	if(deletequeue() == -1){
		return 1;
	}
	//struct msqid_ds *buf;
	/* if(msgctl(msqid, IPC_RMID, buf) == -1){
		perror("msgctl: remove queue failed.");
		return 1;
	} */
	return 0;
}