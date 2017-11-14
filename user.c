#include "project5.h"

long long lastTimeChecked = 0;

int main (int argc, char *argv[]) {

	shmid = 0;
	resourceShmid = 0;
	pcbShmid = 0;
	myPid = getpid();

  /* Command Line args from OSS are processed */
    //message queue segment id
    shmid = atoi(argv[1]);

    //segment ID for shared seconds
    processNumber = atoi(argv[2]);

    //segment ID process control block
    pcbShmid = atoi(argv[3]);

    //resource segment ID
    resourceShmid = atoi(argv[4]);
	
	 //resource segment ID
    timeoutValue = atoi(argv[5]);
	
	//message queue id
	masterQueueId = atoi(argv[6]);

	//Try to attach to shared memory
	if((mainStruct = (clockStruct *)shmat(shmid, NULL, 0)) == (void *) -1) {
		fprintf(stderr, "Error: failed to attach memory segment %i for the virtual clock.\n", shmid);
		exit(EXIT_SUCCESS);
	}

	if((pcbGroup = (PCB *)shmat(pcbShmid, NULL, 0)) == (void *) -1) {
		fprintf(stderr, "Error: Failed to attach memory segment %i for the process control block (PCB)\n", pcbShmid);
		exit(EXIT_SUCCESS);
	}

	if((resourceArray = (resource *)shmat(resourceShmid, NULL, 0)) == (void *) -1) {
		fprintf(stderr, "Error: failed to attach memory segment %i for the resource control block \n", resourceShmid);
		exit(EXIT_SUCCESS);
	}

	/* Setting the random number generator */
	srand(time(NULL) + getpid());


	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, sigquitHandler);
	signal(SIGALRM, killLeftoverProcesses);

	/* Child terminates at 10 seconds */
	alarm(10);

	int i, notFinished = 1;

	do {
  
    /* If this child has not made a request or release than let it into the CS to maybe make one or the other */
	if(pcbGroup[processNumber].request == -1 && pcbGroup[processNumber].release == -1) {
      //Check to see if process will terminate
		if(processWillEnd()) {
			notFinished = 0;
      
      //if not either request or release a resource
		} else {
			if(rand() % 2) {
				//Roll a number to either request or release a resource
				int choseToRequestAResource = rand() % 2;
				if(choseToRequestAResource) {
					// Randomly choose a resource to request
					pcbGroup[processNumber].request = rand() % RSRC_ARR_SIZE; 

					// Sending an update to master					
					sendMessage(masterQueueId, 3);
				
				// User will release the lowest index reasource that is allocated to it
				} else{
					for(i = 0; i < RSRC_ARR_SIZE; i++) {
						if(pcbGroup[processNumber].allocation.quantity[i] > 0) {
							pcbGroup[processNumber].release = i;
							break;
						}
					}
				}
				
				// Sending an update to master
				sendMessage(masterQueueId, 3);
			}
		}
	}
    
	} while (notFinished && mainStruct->sigNotReceived && !pcbGroup[processNumber].terminate);

	/* Tell the master to clear this process's PCB vars */
	if(!pcbGroup[processNumber].terminate) {
		pcbGroup[processNumber].processID = -1;
		sendMessage(masterQueueId, 3);
	}

	if(shmdt(mainStruct) == -1) {
		perror("Error: Child failed to detach shared memory struct");
	}

	if(shmdt(pcbGroup) == -1) {
		perror("Error: Child failed to detach from shared memory array");
	}

	if(shmdt(resourceArray) == -1) {
		perror("Error: Child failed to detach from resource array");
	}

  kill(myPid, SIGTERM);
  sleep(1);
  kill(myPid, SIGKILL);
}
/* END MAIN */

/* Between 0 and 250 ms since last check, the process has a 20% chance to terminate */
int processWillEnd(void) {
	int terminateChance;
	if((mainStruct->virtualClock - lastTimeChecked) >= (rand() % USER_TERMINATE_BOUND)) {
		terminateChance = 1 + rand() % 5;
		lastTimeChecked = mainStruct->virtualClock;
		return terminateChance == 1 ? 1 : 0;
	}	
	lastTimeChecked = mainStruct->virtualClock;
	return 0;
}


/* Send a message to the master to tell it what the child wants done to itself */
void sendMessage(int qid, int msgtype) {
  struct msgbuf msg;

  msg.mType = msgtype;
  sprintf(msg.mText, "%d", processNumber);

  if(msgsnd(qid, (void *) &msg, sizeof(msg.mText), IPC_NOWAIT) == -1) {
    perror("Error: Child msgsnd has failed");
  }
}

/* Kills the process if it hasn't been killed by OSS yet */
void killLeftoverProcesses(int sig) {
	kill(myPid, SIGTERM);
	sleep(1);
	kill(myPid, SIGKILL);
}

/* Handles kill signal sent from OSS */
void sigquitHandler(int sig) {
	kill(myPid, SIGKILL);
	alarm(3);
}
