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
Report's job is to read the shared memory segment and report on the perfect numbers found.
It also for each processes currently computing the number tested, skipped, and found. 
Finally it reports a total of these three numbers for the processes currently running. 
If invoked with the "-k" switch, it also is used to inform the Manage process to shut down computation.
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
int shmid;
int msqid;

void initialize () {
	//creating shared segment
	shmid = shmget(KEY, sizeof(struct sharedMem), IPC_CREAT | 0660);
	if (shmid == -1) {
		perror("shmget error");
		exit(-1);
	}
	// mapping to address space
	memHead = ((struct sharedMem *)shmat(shmid, 0, 0));
	if (memHead == (struct sharedMem *) -1) {
		perror("shmat error");
		exit(-1);
	}
	// message queue
	msqid = msgget(KEY,IPC_CREAT | 0660);
	if (msqid == -1) {
		perror("msgget error");
		exit(-1);
	}
}

void printPerfNums() {
	int i = 0;
	printf("\nPerfect Number Array: ");
	while(memHead->perfNums[i] != 0) {
		printf("\n%d ", memHead->perfNums[i]);
		i++;
	}
	printf("\n\n");
}

void printProcessInfo() {
		
}

void shutDown(struct message msg) {
	msg.type = 3; 
	msg.data = 1; 
	printf("Sending -k shutdown request\n");
	msgsnd(msqid, &msg, sizeof(msg.data), 0);
}

int main(int argc, char* argv[]) {
	//in case report is run before manage
	initialize();
	printPerfNums();
	int i = 0;
	int total = 0;
	while(i <= 20) {
		if(memHead->processArray[i].pid != 0) {
			printf("Process id: %d\n", memHead->processArray[i].pid);
			printf("Number found: %d\n", memHead->processArray[i].found);
			printf("Number tested: %d\n", memHead->processArray[i].tested);
			printf("Number skipped: %d\n", memHead->processArray[i].skipped);
			printf("\n");
			total+=memHead->processArray[i].tested;
		}
		i++;
	}
	printf("Total number tested: %d\n\n", total);
	
	// check for if 'k' is an argument
	if((argv[1]!= NULL) && strcmp(argv[1], "-k") == 0) {
		struct message msg;
		shutDown(msg);
		printf("Shutting down Manage\n");
	}

	if (shmdt((char *) memHead) == -1) {
		perror("detaching shared memory error\n");
		exit(-1);
	}	
	return 1;
}