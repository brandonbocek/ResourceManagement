#include "project5.h"


//static constants
static const int DEFAULT_FILE_SIZE = 9;

//static variables
static char *filename;

int main (int argc, char **argv)
{
    srand(time(NULL) + getpid());

    int index;
    filename = malloc(sizeof (char)*DEFAULT_FILE_SIZE); //cmd line 'l' : output file name
    strcpy(filename, "log.out");


    //process cmd line arguments
    evaluateCmdLineArguments(argc, argv);

    //create and attach memory segments
    createANDattachMemorySegments();

    //Initialize the alarm and CTRL-C handler
    signal(SIGALRM, interruptHandler);
    signal(SIGINT, interruptHandler);
    signal(SIGCHLD, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);

    //set the alarm to tValue seconds
    alarm(tValue);

	//initialize pcb block with base values
    initializePCBStruct();
	//setup initial resource allocations
    setupResources();

    //Open file and mark the beginning of the new log
    file = fopen(filename, "w");
    if(!file) {
        fprintf(stderr, "Error opening log file\n");
        exit(-1);
    }

    mainStruct->virtualClock = 0;
    mainStruct->sigNotReceived = 1;


    do {

        if(mainStruct->virtualClock >= timeToSpawn) {
            forkChild();
            setTimeToSpawn();
        }
        if(mainStruct->virtualClock - lastDeadlockCheck > NANOPERSECOND) {
            lastDeadlockCheck = mainStruct->virtualClock;
            if(deadlockCheck()) {
                do {
                    killAfterDeadlock();
                } while(deadlockCheck());
            }
        }

        mainStruct->virtualClock += iterateClock();

        processMessage(getMessage());

        mainStruct->virtualClock += iterateClock();
        processResourceRequests();


    } while (mainStruct->virtualClock < MAX_TIME && mainStruct->sigNotReceived);


    cleanup();

    return 0;
}

//*********FUNCTIONS*****************************************************
//process command line arguments
void evaluateCmdLineArguments(int _argc, char **_argv) {

    char *tmpLogFileName;
    int getoptLoop;

    //main getopt loop to utilize cmd line arguments
    while ((getoptLoop = getopt(_argc, _argv, "hvl:t:")) != -1)
        switch (getoptLoop) {
        case 'h':
            printHelpMessage();
            abort();
        case 'v':
            vFlag = 1;
            MAX_IDLE_INCREMENT = 100000001;
            break;
        case 'l':
            tmpLogFileName = malloc(sizeof (char)*(strlen(optarg) + 1));
            strcpy(tmpLogFileName, optarg);
            filename = tmpLogFileName;
            break;
        case 't':
            tValue = atoi(optarg);
            break;
        case '?':
            if (optopt == 'l') {
                fprintf(stderr, "\nOption -l requires an argument.Please Try again\n");
                abort();
            } else if (optopt == 't') {
                fprintf(stderr, "\nOption -t requires an argument.Please Try again\n");
                abort();
            } else if (isprint(optopt)) {
                fprintf(stderr, "\nUnknown option -%c Please Try again\n", optopt);
                abort();
            } else {
                fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
                abort();
            }
        default:
            abort();
        }
}

//create and attach memory segments for shared memory and message queue

void createANDattachMemorySegments() {


//Try to get the shared mem id from the key with a size of the struct

    if((shmid = shmget(timerKey, sizeof(clockStruct), IPC_CREAT | 0777)) == -1) {
        fprintf(stderr, "Shared memory segment for virtual clock has failed during creation.\n");
        exit(-1);
    }

    //Try to attach the struct pointer to shared memory
    if((mainStruct = (struct clockStruct *)shmat(shmid, NULL, 0)) == (void *) -1) {
        fprintf(stderr, "Could not attach memory segment %i for the virtual clock.\n", shmid);
        exit(-1);
    }

    //get shmid for pcbGroup of 18 pcbs
    if((pcbShmid = shmget(pcbArrayKey, (sizeof(*pcbGroup) * 18), IPC_CREAT | 0777)) == -1) {
        fprintf(stderr, "Shared memory segment for process control block (PCB) has failed during creation.\n");
        exit(-1);
    }

    //try to attach pcb array to shared memory
    if((pcbGroup = (struct PCB *)shmat(pcbShmid, NULL, 0)) == (void *) -1) {
        fprintf(stderr, "Could not attach memory segment %i for the process control block (PCB)\n", pcbShmid);
        exit(-1);
    }

    //try to attach resource array to shared memory
    if((resourceShmid = shmget(resourceKey, (sizeof(*resourceArray) * 20), IPC_CREAT | 0777)) == -1) {
        perror("Bad shmget allocation resource array");
        exit(-1);
    }

    if((resourceArray = (struct resource *)shmat(resourceShmid, NULL, 0)) == (void *) -1) {
        fprintf(stderr, "Could not attach memory segment %i for the resource control block \n", resourceShmid);
        exit(-1);
    }

    //create message queue for the master process
    if((masterQueueId = msgget(masterQueueKey, IPC_CREAT | 0777)) == -1) {
        fprintf(stderr, "Failed to create message queue\n");
        exit(-1);
    }

}

//initialize process control block with info
void initializePCBStruct(void) {
    int i;
    for (i = 0; i < ARRAY_SIZE; i++) {
        pcbGroup[i].processID = 0;
        pcbGroup[i].deadlocked = 0;
        pcbGroup[i].terminate = 0;
        pcbGroup[i].request = -1;
        pcbGroup[i].release = -1;
        pcbGroup[i].totalTimeRan = 0;
        pcbGroup[i].createTime = 0;
        int j;
        for(j = 0; j < 20; j++) {
            pcbGroup[i].allocation.quantity[j] = 0;
        }
    }


}

//calcualte time to spawn for next child
void setTimeToSpawn(void) {
    timeToSpawn = mainStruct->virtualClock + MIN_FUTURE_SPAWN + rand() % MAX_FUTURE_SPAWN;
    if(vFlag) {
        printf("Will try to spawn slave at time %llu\n", timeToSpawn);
		fprintf(file, "Will try to spawn slave at time %llu\n", timeToSpawn);
    }
}

//fork child from master
void forkChild(void) {

    processNumberBeingSpawned = -1;

    int i;
    for(i = 0; i < ARRAY_SIZE; i++) {
        if(pcbGroup[i].processID == 0) {
            processNumberBeingSpawned = i;
            pcbGroup[i].processID = 1;
            break;
        }
    }

    if(processNumberBeingSpawned == -1) {
        if(vFlag) {
            printf("PCB array is full. No process created.\n");
        }
        if(vFlag) {
            fprintf(file, "PCB array is full. No process created.\n");
        }
    }

    if(processNumberBeingSpawned != -1) {
        if(vFlag) {
            printf("Found open PCB. Spawning process.\n");
        }
        totalProcessesSpawned = totalProcessesSpawned + 1;
        //exit on bad fork
        if((childPid = fork()) < 0) {
            perror("Fork Failure");
            //exit(1);
        }
        fprintf(file,"Process number %d spawned at %llu.%09llu\n", processNumberBeingSpawned,mainStruct->virtualClock / NANOPERSECOND, mainStruct->virtualClock % NANOPERSECOND);

        //If good fork, continue to call exec with all the necessary args
        if(childPid == 0) {
            if(vFlag) {
                printf("Total processes spawned: %d\n", totalProcessesSpawned);
            }
            printf("Process number %d spawned at %llu.%09llu\n", processNumberBeingSpawned,mainStruct->virtualClock / NANOPERSECOND, mainStruct->virtualClock % NANOPERSECOND);
       
            pcbGroup[processNumberBeingSpawned].createTime = mainStruct->virtualClock;
            pcbGroup[processNumberBeingSpawned].processID = getpid();

            //pass segment ID
            char arg1[25];
            sprintf(arg1, "%i", shmid);

            //process Number
            char arg2[5];
            sprintf(arg2, "%i", processNumberBeingSpawned);

            //resource segment id
            char arg3[25];
            sprintf(arg3, "%i", pcbShmid);

            //resource segment id
            char arg4[25];
            sprintf(arg4, "%i", resourceShmid);

            //timer value
            char arg5[5];
            sprintf(arg5, "%d", tValue);

            //message queue ID
            char arg6[25];
            sprintf(arg6,"%i", masterQueueId);

            //invoke user executable and pass arguments
            execl("./user", "user", arg1, arg2, arg3, arg4, arg5, arg6,(char *) NULL);

        }

    }
}


int iterateClock(void) {
    int random = 1 + rand() % MAX_IDLE_INCREMENT;
    return random;
}

//Checks message queue and returns the process location of the sender in the array
int getMessage(void) {
    struct msgbuf msg;

    if(msgrcv(masterQueueId, (void *) &msg, sizeof(msg.mText), 3, IPC_NOWAIT) == -1) {
        if(errno != ENOMSG) {
            perror("Error master receivine message");
            return -1;
        }
        if(vFlag) {
            printf("No message for master\n");
        }
        return -1;
    }
    else {
        int processNum = atoi(msg.mText);
        return processNum;
    }
}

void processMessage(int processNum) {
    if(processNum == -1) {
        return;
    }
    int resourceType;
    if((resourceType = pcbGroup[processNum].request) >= 0) {
        if(vFlag) {
			//printf("Master has detected Process P%d requesting R%d at time %llu.%03llu\n", processNum, resourceType, mainStruct->virtualClock / NANOPERSECOND, mainStruct->virtualClock % NANOPERSECOND);
            //fprintf(file, "Master has detected Process P%d requesting R%d at time %llu.%03llu\n", processNum, resourceType, mainStruct->virtualClock / NANOPERSECOND, mainStruct->virtualClock % NANOPERSECOND);
        }
        //If there are resources of the type available, assign it
        requestResource(resourceType, processNum);
    }
    else if ((resourceType = pcbGroup[processNum].release) >= 0) {
        if(vFlag) {
            fprintf(file,"Releasing resouce# %d from process %d\n", resourceType, processNum,mainStruct->virtualClock / NANOPERSECOND, mainStruct->virtualClock % NANOPERSECOND);
        }
        releaseResource(resourceType, processNum);
    }
    else if(pcbGroup[processNum].processID == -1) {
        performProcessCleanup(processNum);
    }
    else {
        if(vFlag) {
            printf("Found no action for this message\n");
        }
    }

    //processMessage(getMessage());

}


void setupResources(void) {
    int i;
    //Set the resource types, quantity, and quantAvail
    for(i = 0; i < 20; i++) {
        resourceArray[i].quantity = 1 + rand() % 10;
        resourceArray[i].quantAvail = resourceArray[i].quantity;
    }

    //Between 3 and 5 resources will be shareable
    int numShared = 3 + rand() % 3;

    //Get randomly choose a resource for those that
    //will be shared
    for(i = 0; i < numShared; i++) {
        int choice = rand() % 20;
        resourceArray[choice].quantity = 1000;
        resourceArray[choice].quantAvail = 1000;
    }

    //resourceSnapshot();
}
/*
void resourceSnapshot(void) {
    int i;
//printf("**************************************");
    for(i = 0; i < 20; i++) {
        if(vFlag) {
            printf("Resource %d has %d available out of %d\n", i, resourceArray[i].quantAvail, resourceArray[i].quantity);
        }
    }
    //printf("**************************************");
}
*/
void processResourceRequests(void) {
   
    int i;
    int j;
    int request = -1;
    int release = -1;
    //Go through and look at all the request/release/processID members of each pcbGroup element
    //and see if there is any processing to do
    for(i = 0; i < ARRAY_SIZE; i++) {
        int resourceType = -1;
        int quant;
     
        //If the request flag is set with the value of a resource type, process the request
        if((resourceType = pcbGroup[i].request) >= 0) {
            if(vFlag) {
                printf("Master has detected Process P%d requesting R%d at time %llu.%03llu\n", i, resourceType, mainStruct->virtualClock / NANOPERSECOND, mainStruct->virtualClock % NANOPERSECOND);
                fprintf(file, "Master has detected Process P%d requesting R%d at time %llu.%03llu\n", i, resourceType, mainStruct->virtualClock / NANOPERSECOND, mainStruct->virtualClock % NANOPERSECOND);
            }
            //If there are resources of the type available, assign it
            requestResource(resourceType, i);
        }
        //If the release flag is set with the value of the resourceType, process it
        else if((resourceType = pcbGroup[i].release) >= 0) {
            releaseResource(resourceType, i);
        }
        //If the process set its processID to -1, that means it died and we can put all
        //the resources it had back into the resourceArray
        else if(pcbGroup[i].processID == -1) {
            performProcessCleanup(i);
        }
        else {
            //If there is a process at that location but doesn't meet the above criteria, print
            if(pcbGroup[i].processID > 0) {
                if(vFlag) {
                    //printf("No request for this process\n");
                }
            }
        }
    }
  
}
void printResourcesAllocatedToEachProcess(){
	int i;
	printf("     R0  R1  R2  R3  R4  R5  R6  R7  R8  R9  R10  R11  R12  R13  R14  R15  R16  R17  R18  R19\n");
	for(i = 0; i < 10; i++){
		printf("P%d:  %d   %d   %d   %d   %d   %d   %d   %d   %d   %d   %d    %d    %d    %d    %d    %d    %d    %d    %d    %d\n", i, pcbGroup[i].allocation.quantity[0],
			pcbGroup[i].allocation.quantity[1], pcbGroup[i].allocation.quantity[2], pcbGroup[i].allocation.quantity[3],
			pcbGroup[i].allocation.quantity[4], pcbGroup[i].allocation.quantity[5], pcbGroup[i].allocation.quantity[6],
			pcbGroup[i].allocation.quantity[7], pcbGroup[i].allocation.quantity[8], pcbGroup[i].allocation.quantity[9],
			pcbGroup[i].allocation.quantity[10], pcbGroup[i].allocation.quantity[11], pcbGroup[i].allocation.quantity[12],
			pcbGroup[i].allocation.quantity[13], pcbGroup[i].allocation.quantity[14], pcbGroup[i].allocation.quantity[15],
			pcbGroup[i].allocation.quantity[16], pcbGroup[i].allocation.quantity[17], pcbGroup[i].allocation.quantity[18],
			pcbGroup[i].allocation.quantity[19]);
	}
	for(i = 10; i < ARRAY_SIZE; i++){
		printf("P%d: %d   %d   %d   %d   %d   %d   %d   %d   %d   %d   %d    %d    %d    %d    %d    %d    %d    %d    %d    %d\n", i, pcbGroup[i].allocation.quantity[0],
			pcbGroup[i].allocation.quantity[1], pcbGroup[i].allocation.quantity[2], pcbGroup[i].allocation.quantity[3],
			pcbGroup[i].allocation.quantity[4], pcbGroup[i].allocation.quantity[5], pcbGroup[i].allocation.quantity[6],
			pcbGroup[i].allocation.quantity[7], pcbGroup[i].allocation.quantity[8], pcbGroup[i].allocation.quantity[9],
			pcbGroup[i].allocation.quantity[10], pcbGroup[i].allocation.quantity[11], pcbGroup[i].allocation.quantity[12],
			pcbGroup[i].allocation.quantity[13], pcbGroup[i].allocation.quantity[14], pcbGroup[i].allocation.quantity[15],
			pcbGroup[i].allocation.quantity[16], pcbGroup[i].allocation.quantity[17], pcbGroup[i].allocation.quantity[18],
			pcbGroup[i].allocation.quantity[19]);
	}
}

void requestResource(int resourceType, int i) {
    int quant;
    if((quant = resourceArray[resourceType].quantAvail) > 0) {
		
		totalGrantedRequests++;
		//printf("Total number of resource requests granted is %d\n", totalGrantedRequests);
		
		if(((totalGrantedRequests % 20) == 0) && (totalGrantedRequests > 0)) {
			//printResourcesAllocatedToEachProcess();
		}
		
        if(vFlag) {
            printf("There are %d out of %d for resource %d available\n", quant, resourceArray[resourceType].quantity, resourceType);
            //fprintf(file,"Process number %d has requested Resource# %d at llu.%llu\n",i, resourceType,mainStruct->virtualClock / NANOPERSECOND, mainStruct->virtualClock % NANOPERSECOND);
        }
        if(vFlag) {
            printf("Increased resource %d for process %d\n", resourceType, i);
            //fprintf(file,"Increased resource %d for process %d\n", resourceType, i);
        }
        //Increase the quantity of the resourceType for the element in the pcbGroup
        //requesting it
        pcbGroup[i].allocation.quantity[resourceType]++;
        //This process is no longer requesting the resource after allocation
        pcbGroup[i].request = -1;
        //Decrease the quantity of the resource type in the resource array
        resourceArray[resourceType].quantAvail--;
        if(vFlag) {
            printf("There are now %d out of %d for resource %d\n", resourceArray[resourceType].quantAvail, resourceArray[resourceType].quantity, resourceType);
            fprintf(file,"There are now %d out of %d available for resource %d\n", resourceArray[resourceType].quantAvail, resourceArray[resourceType].quantity, resourceType);

        }
    }
}

void releaseResource(int resourceType, int i) {
    if(vFlag) {
        printf("Releasing resouce %d from process %d\n", resourceType, i);
        //fprintf(file,"Releasing resouce %d from process %d at %llu.%llu\n", resourceType, i,mainStruct->virtualClock / NANOPERSECOND, mainStruct->virtualClock % NANOPERSECOND);
    }
    //Decrease the count of the quantity of that resource for that element in the pcbGroup
    pcbGroup[i].allocation.quantity[resourceType]--;
    //Increase the quantity of that resource type in the resourceArray
    resourceArray[resourceType].quantAvail++;
    if(vFlag) {
        printf("There are now %d out of %d for resource %d\n", resourceArray[resourceType].quantAvail, resourceArray[resourceType].quantity, resourceType);
        fprintf(file,"There are now %d out of %d available for resource %d\n", resourceArray[resourceType].quantAvail, resourceArray[resourceType].quantity, resourceType);

	}
    //Reset the release flag to -1
    pcbGroup[i].release = -1;
}

void performProcessCleanup(int i) {
    if(vFlag) {
        printf("	Process %d completed its time\n", i);
        fprintf(file,"	Process %d completed its time\n", i);
    }
    //Go through all the allocations to the dead process and put them back into the
    //resource array
    int j;
    for(j = 0; j < 20; j++) {
        //If the quantity is > 0 for that resource, put them back
        if(pcbGroup[i].allocation.quantity[j] > 0) {
            if(vFlag) {
                printf("Before return of resources, there are %d out of %d for resource %d\n", resourceArray[j].quantAvail, resourceArray[j].quantity, j);
            }
            //Get the quantity to put back
            int returnQuant = pcbGroup[i].allocation.quantity[j];
            //Increase the resource type quantAvail in the resource array
            resourceArray[j].quantAvail += returnQuant;
            if(vFlag) {
				printf("	Returning %d of resource %d from process %d\n", returnQuant, j, i);
				fprintf(file,"	Process %d returning %d of resource %d due to termination\n", i,returnQuant,j);
                printf("There are now %d out of %d for resource %d\n", resourceArray[j].quantAvail, resourceArray[j].quantity, j);
                fprintf(file,"	There are now %d out of %d for resource %d\n", resourceArray[j].quantAvail, resourceArray[j].quantity, j);
			}
            //Set the quantity of the pcbGroup to 0
            pcbGroup[i].allocation.quantity[j] = 0;
        }
    }
    //Reset all values
    pcbGroup[i].processID = 0;
    pcbGroup[i].totalTimeRan = 0;
    pcbGroup[i].createTime = 0;
    pcbGroup[i].request = -1;
    pcbGroup[i].release = -1;

}

//returns the number of deadlocked processes
int deadlockCheck(void) {
    //printf("Begin deadlock detection at %llu.%llu\n", mainStruct->virtualClock / NANOPERSECOND, mainStruct->virtualClock % NANOPERSECOND);
    //fprintf(file,"Begin deadlock detection at %llu.%llu\n", mainStruct->virtualClock / NANOPERSECOND, mainStruct->virtualClock % NANOPERSECOND);

    int resourceTempArr[20];
    int safeProcessArr[18];
    int p;
    for(p = 0; p < 20; p++) {
        resourceTempArr[p] = resourceArray[p].quantAvail;
    }
    for(p = 0; p < 18; p++) {
        safeProcessArr[p] = 0;
    }
    for(p = 0; p < 18; p++) {
		
		//If this pcb doesn't have a process, it won't be deadlocked
        if(pcbGroup[p].processID == 0) {
            safeProcessArr[p] = 1;
        }
		
		// if the process has spawned and it isn't making a request where the resources are unavailable
        if(!safeProcessArr[p] && reqLtAvail(resourceTempArr, p)) {
            safeProcessArr[p] = 1;
            int i;
			// For each resource add what is allocated to what is available to get the original amount of resources available
            for(i = 0; i < 20; i++) {
               resourceTempArr[i] = resourceTempArr[i] + pcbGroup[p].allocation.quantity[i];
            }
			
			// Start this for-loop over again
			// In order to get an accurate max resources available
            p = -1;
        }
    }

	// If there are any processes that have not been set to not deadlocked
	// That process is 
    int deadlockCount = 0;
    for(p = 0; p < 18; p++) {
        if(!safeProcessArr[p]) {
            pcbGroup[p].deadlocked = 1;
			//printf("Process %d is deadlocked at %llu.%03llu\n", p, mainStruct->virtualClock / NANOPERSECOND, mainStruct->virtualClock % NANOPERSECOND);
            //fprintf(file, "Process %d is deadlocked at %llu.%03llu\n", p, mainStruct->virtualClock / NANOPERSECOND, mainStruct->virtualClock % NANOPERSECOND);
            deadlockCount++;
        }
        else {
            pcbGroup[p].deadlocked = 0;
        }
    }

	// Tell the user which processes are deadlocked
    if(deadlockCount > 0) {
        printf("Processes: ");
        fprintf(file, "Processes: ");
		
        for(p = 0; p < 18; p++) {
            if(!safeProcessArr[p]) {
                printf("P:%d ", p);
				fprintf(file, "P:%d ", p);
            }
        }
		printf("deadlocked\n");
		fprintf(file, "deadlocked\n");
        
    }
	
	return deadlockCount;
}

// returns false if the process is making a request but resources are not available for that request
int reqLtAvail(int *resourceTempArr, int p) {
	// If the process is not requesting any resource
    if(pcbGroup[p].request == -1)
        return 1;
	
	//If there are resources available for this process's request
    if(resourceTempArr[pcbGroup[p].request] > 0)
        return 1;
	
	// The process is deadlocked
	return 0;
	
}

/* Find the resource with the most amount of resources allocated to it and kill*/
void killAfterDeadlock(void) {
    int process;
    int max = 0;
    int i;
    int j;
    for(i = 0; i < 18; i++) {
        if(pcbGroup[i].deadlocked) {
            int total = 0;
            for(j = 0; j < 20; j++) {
                total += pcbGroup[i].allocation.quantity[j];
            }
            if(total > max) {
                max = total;
                process = i;
            }
        }
    }
    fprintf(file, "	Operating System detected process %d is deadlocked\n", process);
    printf("Killing process %d because it used the most total resources\n", process);
    fprintf(file, "	Killing process %d\n",process);
    pcbGroup[process].deadlocked = 0;
    pcbGroup[process].terminate = 1;
    performProcessCleanup(process);
}

//Interrupt handler function that calls the process destroyer

void interruptHandler(int SIG) {
    signal(SIGQUIT, SIG_IGN);
    signal(SIGINT, SIG_IGN);

    if(SIG == SIGINT) {
        fprintf(stderr, "\nCTRL-C received. Calling shutdown functions.\n");
        cleanup();
    }

    if(SIG == SIGALRM) {
        fprintf(stderr, "Master has timed out. Initiating shutdown sequence.\n");
        cleanup();
    }
}

//Cleanup memory and processes.
void cleanup() {

    signal(SIGQUIT, SIG_IGN);
    mainStruct->sigNotReceived = 0;

   
    kill(-getpgrp(), SIGQUIT);


    printf("Master waiting on all processes do die\n");
    childPid = wait(&status);

    //Detach and remove the shared memory after all child process have died
    if(detachAndRemoveTimer(shmid, mainStruct) == -1) {
        perror("Failed to destroy shared messageQ shared mem seg");
    }

    if(detachAndRemoveArray(pcbShmid, pcbGroup) == -1) {
        perror("Failed to destroy shared pcb shared mem seg");
    }

    if(detachAndRemoveResource(resourceShmid, resourceArray) == -1) {
        perror("Faild to destroy resource shared mem seg");
    }

   

 
    //Delete the message queues
    msgctl(masterQueueId, IPC_RMID, NULL);

    if(fclose(file)) {
        perror("    Error closing file");
    }
    printf("Master teminating\n");

    exit(1);

}


//Detach and remove function
int detachAndRemoveTimer(int shmid, clockStruct *shmaddr) {

    int error = 0;
    if(shmdt(shmaddr) == -1) {
        error = errno;
    }
    if((shmctl(shmid, IPC_RMID, NULL) == -1) && !error) {
        error = errno;
    }
    if(!error) {
        return 0;
    }

    return -1;
}

//Detach and remove function
int detachAndRemoveArray(int shmid, PCB *shmaddr) {
    int error = 0;
    if(shmdt(shmaddr) == -1) {
        error = errno;
    }
    if((shmctl(shmid, IPC_RMID, NULL) == -1) && !error) {
        error = errno;
    }
    if(!error) {
        return 0;
    }

    return -1;
}

//Detach and remove function
int detachAndRemoveResource(int shmid, resource *shmaddr) {
    int error = 0;
    if(shmdt(shmaddr) == -1) {
        error = errno;
    }
    if((shmctl(shmid, IPC_RMID, NULL) == -1) && !error) {
        error = errno;
    }
    if(!error) {
        return 0;
    }

    return -1;
}

//Long help message
void printHelpMessage(void) {
    printf("\n\n***************Command Line Argument Guide***********************************************\n\n");
    printf("%-15s%s", "-h", "Displays functions of all cmd line arguments\n\n");
    printf("%-15s%s", "-v", "Switches to verbose mode, printing all debug info\n\n");
    printf("%-15s%s", "-l filename", "Replace 'filename' with desired output file name (default 'log.out')\n\n");
    printf("%-15s%s", "-t z", "Integer that indicates time(s) until program self terminates (default 20) \n\n");
    printf("*****************************************************************************************\n\n");
}
