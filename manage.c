/* THIS CODE IS MY OWN WORK, IT WAS WRITTEN WITHOUT CONSULTING A TUTOR OR CODE WRITTEN BY OTHER STUDENTS - Jerry Ho */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <setjmp.h>
#include <errno.h>
#include <sys/msg.h>

#define KEY (key_t)37957 //key for shared memory segment

/* 
Manage's job is to maintain the shared memory segment. 
The shared segment is where the compute processes post their results.
It also keeps track of the active "compute" processes, so that it can signal them to terminate.
*/

/*
1) a bit map large enough to contain 2^25 bits. 
	if a bit is off it indicates the coreesponding integer has not been tested.
2) An array of integers of length 20 to contain the perfect numbers found.
3) An array of "process" structures of length 20, to summarize data on the currently active compute processes.
	This structure should contain the pid, the number of perfect numbers found,
	the number of candidates tested, and the number of candidates not tested )ie skipped).
	"Compute" should never test a number already marked in the bitmap.
*/

struct processDescription {
	pid_t pid;
	int found;
	int tested;
	int skipped;
};
	
struct message {
	long type;
	int data;
};

struct sharedMem {
	int bits[33554432]; //2^25
	int perfNums[20];
	struct processDescription processArray[20];
};

struct sharedMem *memHead; 
int shmid;	   
int msqid;	   

void initialize() {
	//shared segment
	shmid = shmget(KEY, sizeof(struct sharedMem), IPC_CREAT |0660);
	if (shmid == -1) {
		perror("shmget error\n");
		exit(-1);
	}
	//mapping to address space
	memHead= ((struct sharedMem*)shmat(shmid,0,0));
	if ( memHead == (struct sharedMem*) -1) {
		perror("shmat error\n");
		exit(-1);
	}
	// message queue
	msqid = msgget(KEY,IPC_CREAT | 0660);
	if (msqid == -1) {
		perror("msgget error");
		exit(-1);
	}
}
// signal received or -k is used by report
void killProcesses(int sigNum) {
	/*
	All process should terminate cleanly on INTR, QUIT and HANGUP signals. 
	Sending INTR signals to all running computes.
	Sleeps for 5 seconds.
	deallocates shared memory.
	*/
	printf("\nSignal number: %d received!\n", sigNum);
	//checks for which signal
	if(sigNum==SIGINT) printf("Signal INTR received\n");
	else if(sigNum==SIGQUIT) printf("Signal QUIT received\n");
	else if(sigNum==SIGHUP) printf("Signal HUP received\n");
	// sending SIGINT to all running computes
	for(int i = 0; i < 20; i++) {
		int pid = memHead->processArray[i].pid;
		if(pid != 0) kill(pid, SIGINT);
	}
	sleep(5);
	printf("Cleaning up\n");
	if (shmdt((char *)memHead) == -1) { //detaches shared memory at memHead
		perror("detaching shared memory error");
		exit(-1);
	}	
	if (msgctl(msqid, IPC_RMID, NULL) == -1) { //Immediately  remove  the  message  queue
		perror("message queue deletion error");
		exit(-1);
	}
	if (shmctl(shmid, IPC_RMID, NULL) == -1) { //Mark the segment to be destroyed.
		perror("shared memory deletion error");
		exit(-1);
	}
	printf("Management closed cleanly!\n");
	exit(0);
}

void registerProcess(int processIndex, struct message msg) {
	printf("Received new process request (msg.type: %d msg.data: %d)\n", (int)msg.type, (int)msg.data);			
	msg.type = msg.data; //pid of the compute			
	processIndex = 0; //reset slot index

	while(processIndex < 20) {
		if(memHead->processArray[processIndex].pid == 0) { //found an empty slot
			msg.data = processIndex; //msg data to be sent
			break; 
		}
		else processIndex++; //iterate till slot found
	}
	if(processIndex == 20) { //process max 20
		printf("No more compute processes allowed. Compute terminated.\n"); 
		msg.data = 21; //to indicate termination
	}
	msgsnd(msqid, &msg, sizeof(msg.data), 0); //sends process index
	printf("Sending process array index (msg.type: %d msg.data: %d)\n", (int)msg.type, (int)msg.data);
}

int submitAndSort(int numOfPerfs, struct message msg) {
	printf("Received perfect number (msg.type: %d msg.data: %d)\n", (int)msg.type, (int)msg.data);
	int newPerf = msg.data; //perfect number from message data
	if(numOfPerfs == 0) { //array empty
		memHead->perfNums[0] = newPerf;
		numOfPerfs++;
		return numOfPerfs;
	}
	int i;
	int present = 0;
	for (i=0; i < numOfPerfs; i++) {
		 if (memHead->perfNums[i] == newPerf) {
			present = 1;
			break;
		}
	}
	if(present != 1) { //sort array
		printf("Inserting into perfect number array\n");
		i = numOfPerfs-1;
		while(newPerf < memHead->perfNums[i] && i>=0) {
			memHead->perfNums[i+1] = memHead->perfNums[i];
			i--;
		}
		memHead->perfNums[i+1] = newPerf;
		numOfPerfs++;
	}
	return numOfPerfs;
}

//calls initialize to set up shared memory
//listens and maintains shared memory
int main() {
	//if compute or report haven't already initialized shared space
	initialize();
	memset(memHead, 0, 4194304);
	// signals handled in killProcesses
	signal(SIGINT, killProcesses);
	signal(SIGQUIT, killProcesses);
	signal(SIGHUP, killProcesses);
	// cleared bitmap
	for (int j=0;j<=33554432;j++){
		memHead->bits[j]=0;
	}
	struct message msg;	
	int numOfPerfs = 0; 
	int processIndex; //processArray index
	while (1) {
		msgrcv(msqid, &msg, sizeof(msg.data), -3, 0);
		if(msg.type == 1) registerProcess(processIndex, msg); //new process request
		else if(msg.type == 2) numOfPerfs = submitAndSort(numOfPerfs, msg); //found a perfect number
		else if(msg.type == 3) { //report sent k
			printf("Report shutdown in progress\n");
			killProcesses(0);
		}
	}
	return 1;
}