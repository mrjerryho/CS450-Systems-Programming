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
Compute's job is to compute perfect numbers. 
It takes one command line argument, which is the first number to test.
It tests all numbers starting at this point, subjective to the constraints below.
There may be more than one copy of compute running simultaneously.
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
	int bits[33554432]; // 2^25
	int perfNums[20];
	struct processDescription processArray[20];
};

struct sharedMem *memHead;	
int processIndex;
int shmid;
int msqid;
int SIZE = sizeof(int)*8;

void killProcess(int sigNum) {
	if(sigNum==SIGINT) printf("Signal INTR received\n");
	else if(sigNum==SIGQUIT) printf("Signal QUIT received\n");
	else if(sigNum==SIGHUP) printf("Signal HUP received\n");
	memHead->processArray[processIndex].pid = 0;
	memHead->processArray[processIndex].found = 0;
	memHead->processArray[processIndex].tested = 0;
	memHead->processArray[processIndex].skipped = 0;
	if (shmdt((char  *) memHead) == -1) {
		perror("detaching shared memory error");
		exit(3);
	}
	printf("Process entry deleted\n");
	exit(0);
}

int testBit( int A[],  int k ) {
	return ( (A[k/(sizeof(int)*8)] & (1 << (k%(sizeof(int)*8)) )) != 0 ) ;
}
void setBit( int A[],  int k ) { 
	A[k/(sizeof(int)*8)] |= 1 << (k%(sizeof(int)*8));
 } // Sets bit at k in A[i]

void clearProcessIndex (int processIndex, int pid) {
	memHead->processArray[processIndex].pid = pid;
	memHead->processArray[processIndex].found = 0;
	memHead->processArray[processIndex].skipped = 0;
	memHead->processArray[processIndex].tested = 0;
}

void initialize () {
	shmid = shmget(KEY, sizeof(struct sharedMem),IPC_CREAT | 0660);
	if (shmid == -1) {
		perror("shmget error");
		exit(1);
	}
	// mapping to address space
	memHead = ((struct sharedMem *)shmat(shmid, 0, 0));
	if (memHead == (struct sharedMem *) -1) {
		perror("shmat error");
		exit(2);
	}
	// message queue
	msqid = msgget(KEY,IPC_CREAT |0660);
	if (msqid == -1) {
		perror("msgget error");
		exit(1);
	}
}

int getProcessIndex(int pid, struct message msg) {
	msg.data = pid; //storing pid
	printf("New process slot request (msg.type: %d msg.data: %d)\n", (int)msg.type, (int)msg.data);
	msgsnd(msqid, &msg, sizeof(msg.data), 0);
	msgrcv(msqid, &msg, sizeof(msg.data), pid, 0); //receive when message type matches pid
	printf("Received process index (msg.type: %d msg.data: %d)\n", (int)msg.type, (int)msg.data);
	processIndex = msg.data; //put message data into process index
	return processIndex;
}

void compute(int start, struct message msg) {
	int max = 33554432;
	for (int j = start; j <= max; j++) {
		if(j == 33554432){
			j = 2;
			max = start;
		}
		if(testBit(memHead->bits, j) == 0) { //Test the bit
			setBit(memHead->bits, j); //set the bit
			memHead->processArray[processIndex].tested++; //Test the number for perfect Number
			int sum = 1;
			for (int i = 2; i < j; i++)
				if (!(j % i)) sum += i;
			if (sum == j && j != 1) {
				memHead->processArray[processIndex].found++;
				msg.type = 2;
				msg.data = j;
				printf("Found and sending a perfect number (msg.type: %d msg.data: %d)\n", (int)msg.type, (int)msg.data);
				msgsnd(msqid, &msg, sizeof(msg.data),0);
			}
			continue; //bit already tested
		}
		
		memHead->processArray[processIndex].skipped++;
	}
}

int main(int argc, char* argv[]) {
	
	if(argc < 2) { 
		printf("Please pass in an integer starting point.\n"); 
		exit(1); 
	}
	int start = atoi(argv[1]);
	if (start < 2) start = 2;
	printf("Starting point: %d\n", start);
	struct message msg;
	//if compute run before manage
	initialize();
	//signals handed in killProcess
	signal(SIGINT, killProcess);
	signal(SIGQUIT, killProcess);
	signal(SIGHUP, killProcess);
	msg.type = 1; //message type 1 asking for new process entry
	int pid = getpid();
	processIndex = getProcessIndex(pid, msg);
	if(processIndex == 21) {
		printf("Manage says no more compute processes. Bye\n");		
		killProcess(0);
	}
	// clear processIndex in array
	clearProcessIndex(processIndex, pid);
	// test bits and compute numbers
	compute(start, msg);
	printf("Compute finished cleanly!\n");
	killProcess(0);
	return 1;
}