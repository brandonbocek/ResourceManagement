#include "project5.h"

int main (int argc, char *argv[]) {

    int c;

    /* Handling command line arguments w/ ./oss  */
    while ((c = getopt(argc, argv, "hvl:t:")) != -1)
        switch (c) {
        case 'h':
            printHelpMenu();
        case 'v':
			/* turning verbose mode on */
            verboseOn = 1;
			/* With more prints increment clock quicker for run time speed to be similar */
            CLOCK_INCREMENT_MAX = 1000001;
            break;
        case 'l':
			/*  Change the name of the file to write to */
			filename = optarg;
			break;
        case 't':
		/*  Change the time before the master terminates */
            if(isdigit(*optarg)) {
				maxTimeToRun = atoi(optarg);
			} else {
				fprintf(stderr, "'Give a number with -t'\n", optarg);
				exit(EXIT_FAILURE);
			}
			break;
        case '?':
			/* User entered a valid flag but nothing after it */
            if (optopt == 'l' || optopt == 't') {
                fprintf(stderr, "-%c needs to have an argument!\n", optopt);
            } else {
                fprintf(stderr, "%s is an unrecognized flag\n", argv[optind - 1]);
            }
        default:
            /* User entered an invalid flag */
			printHelpMenu();
        }
		

    /*Creating and attaching shared memory segments for... */
	
	/* Virtual Clock Shared Mem */
    if((shmid = shmget(timerKey, sizeof(clockStruct), IPC_CREAT | 0666)) == -1) {
        perror("Error: shmget for attaching the virtual clock to shared memory has failed.");
        exit(EXIT_FAILURE);
    }
    if((mainStruct = (struct clockStruct *)shmat(shmid, NULL, 0)) == (void *) -1) {
        fprintf(stderr, "Error: Attaching memory segment %i for the vclock has failed.\n", shmid);
        exit(EXIT_FAILURE);
    }
	
	/* Resource Array Shared Mem */
    if((resourceShmid = shmget(resourceArrKey, (sizeof(*resourceArray) * 20), IPC_CREAT | 0666)) == -1) {
        perror("Error: shmget for attaching the resource array to shared memory has failed.");
        exit(EXIT_FAILURE);
    }
    if((resourceArray = (struct resource *)shmat(resourceShmid, NULL, 0)) == (void *) -1) {
        fprintf(stderr, "Error: Attaching memory segment %i has failed.\n", resourceShmid);
        exit(EXIT_FAILURE);
    }

    /* PCB Array Shared Mem */
    if((pcbShmid = shmget(pcbArrKey, (sizeof(*pcbGroup) * 18), IPC_CREAT | 0666)) == -1) {
        perror("Error: shmget for attaching the PCB array to shared memory has failed.");
        exit(EXIT_FAILURE);
    }
    if((pcbGroup = (struct PCB *)shmat(pcbShmid, NULL, 0)) == (void *) -1) {
        fprintf(stderr, "Error: Attaching memory segment %i for the PCB array has failed.\n", pcbShmid);
        exit(EXIT_FAILURE);
    }

    /*Message queue is created for the OSS Master */
    if((masterQueueId = msgget(masterQKey, IPC_CREAT | 0666)) == -1) {
        fprintf(stderr, "Error: message queue creation has failed\n");
        exit(-1);
    }

    /* Interrupt handlers for Alarm and CTRL-C are initialized */
    signal(SIGALRM, interruptHandler);
    signal(SIGINT, interruptHandler);
    signal(SIGCHLD, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);

    /* Alarm to terminate is set to 20 seconds by default or the command line arg */
    alarm(maxTimeToRun);

	/* The 20 PCBs in the array have their fields initialized to starting defaults*/
    int x, y;
    for (x = 0; x < PCB_ARRAY_SIZE; x++) {
		for(y = 0; y < RSRC_ARR_SIZE; y++) {
            pcbGroup[x].allocation.quantity[y] = 0;
        }
        pcbGroup[x].processID = 0;
		pcbGroup[x].createTime = 0;
        pcbGroup[x].deadlocked = 0;
        pcbGroup[x].request = -1;
        pcbGroup[x].release = -1;
		pcbGroup[x].terminate = 0;
    }
	
	/* Setting the random number generator */
	srand(time(NULL) + getpid());
	
	/* The 20 resources are initialized */
	
	int indexToBeShared, qtyOfResourcesToBeShared;
    /* Each resource in the array has between 1 and 10 resources available */
    for(x = 0; x < RSRC_ARR_SIZE; x++) {
        resourceArray[x].quantity = 1 + rand() % 10;
        resourceArray[x].quantAvail = resourceArray[x].quantity;
    }

    /* Between 3 and 5 of the resources are shareable
	so on average 4 of 20 resource or 20% are shareable	*/
    qtyOfResourcesToBeShared = 3 + rand() % 3;

    for(x = 0; x < qtyOfResourcesToBeShared; x++) {
        indexToBeShared = rand() % RSRC_ARR_SIZE;
        resourceArray[indexToBeShared].quantity = 100;
        resourceArray[indexToBeShared].quantAvail = 100;
    }

    /*  File pointer opens log.out by default */
    file = fopen(filename, "w");
    if(!file) {
        fprintf(stderr, "Error: failed to open the log file\n");
        exit(EXIT_FAILURE);
    }

	
	/* Start the virtual clock and the main loop in master */
    mainStruct->virtualClock = 0;
    mainStruct->sigNotReceived = 1;

    do {

        if(mainStruct->virtualClock >= timeToSpawn) {
            forkAndExecuteNewChild();
			
			/* The time for the next process to spawn is set */
            timeToSpawn = mainStruct->virtualClock + MIN_FUTURE_SPAWN + rand() % MAX_FUTURE_SPAWN;
			if(verboseOn) {
				printf("OSS: will try to spawn a child process at time %llu.%llu\n", timeToSpawn / NANOPERSECOND, timeToSpawn % NANOPERSECOND);
				if(fileLinesPrinted < LINE_LIMIT) {
					fprintf(file, "OSS: will try to spawn a child process at time %llu%.llu\n", timeToSpawn / NANOPERSECOND, timeToSpawn % NANOPERSECOND);
					fileLinesPrinted++;
				}
			}
        }
        if(mainStruct->virtualClock - lastDeadlockCheck > NANOPERSECOND) {
            lastDeadlockCheck = mainStruct->virtualClock;
			
			/* If there is deadlock kill the process using the most resources until deadlock ends */
            if(deadlockCheck()) {
                do {
                    killAfterDeadlock();
                } while(deadlockCheck());
            }
        }

        mainStruct->virtualClock += 1 + rand() % CLOCK_INCREMENT_MAX;

		/* receive message from child */
        processMessage(getMessage());

        mainStruct->virtualClock += 1 + rand() % CLOCK_INCREMENT_MAX;
        processResourceRequests();


    } while (mainStruct->virtualClock < MAX_TIME_RUN_MASTER && mainStruct->sigNotReceived);


    cleanup();

    return 0;
}
/* END MAIN */

void printHelpMenu() {
	printf("\n\t\t~~Help Menu~~\n\t-h This Help Menu Printed\n");
	printf("\t-v *turns on verbose to see extra log file messages\n");
	printf("\t-l *log file used*\t\t\t\tie. '-l log.out'\n");
	printf("\t-t *time in seconds the master will terminate*\tie. -t 20\n\n");
}

/* Forking a new process if there is room for it in the PCB array */
void forkAndExecuteNewChild(void) {

    processNumberBeingSpawned = -1;

    int i;
    for(i = 0; i < PCB_ARRAY_SIZE; i++) {
        if(pcbGroup[i].processID == 0) {
            processNumberBeingSpawned = i;
            pcbGroup[i].processID = 1;
            break;
        }
    }

    if(processNumberBeingSpawned == -1) {
        if(verboseOn) {
            printf("The PCB array is full. No process is to be created for this loop.\n");
			if(fileLinesPrinted < LINE_LIMIT) {
				fprintf(file, "The PCB array is full. No process is to be created for this loop.\n");
				fileLinesPrinted++;
			}
        }
        
    }else {
        if(verboseOn) {
            printf("An open PCB was located. A new process will be spawned in the opening.\n");
			if(fileLinesPrinted < LINE_LIMIT) {
				fprintf(file, "An open PCB was located. A new process will be spawned in the opening.\n");
				fileLinesPrinted++;
			}
        }
        totalProcessesSpawned = totalProcessesSpawned + 1;
        
		/* Forking a new child process */
        if((childPid = fork()) < 0) {
            perror("Error: some problem occurred in forking new process\n");
        }
		
		if(verboseOn) {
			printf("Process number %d spawned at time %llu.%llu\n", processNumberBeingSpawned, mainStruct->virtualClock / NANOPERSECOND, mainStruct->virtualClock % NANOPERSECOND);
			if(fileLinesPrinted < LINE_LIMIT) {
				fprintf(file,"Process number %d spawned at time %llu.%llu\n", processNumberBeingSpawned, mainStruct->virtualClock / NANOPERSECOND, mainStruct->virtualClock % NANOPERSECOND);
				fileLinesPrinted++;
			}
		}
		
        /* The fork succeeded */
        if(childPid == 0) {
            if(verboseOn) {
                printf("Total processes spawned: %d\n", totalProcessesSpawned);
				if(fileLinesPrinted < LINE_LIMIT) {
					fprintf(file, "Total processes spawned: %d\n", totalProcessesSpawned);
					fileLinesPrinted++;
				}
            }
       
            pcbGroup[processNumberBeingSpawned].createTime = mainStruct->virtualClock;
            pcbGroup[processNumberBeingSpawned].processID = getpid();
			
            char arg1[25], arg2[5], arg3[25], arg4[25], arg5[5], arg6[25];
			//segment ID
            sprintf(arg1, "%i", shmid);

            //process index of pcb array being spawned
            sprintf(arg2, "%i", processNumberBeingSpawned);

            //pcb segment id
            sprintf(arg3, "%i", pcbShmid);

            //resource segment id
            sprintf(arg4, "%i", resourceShmid);

            //timer value
            sprintf(arg5, "%d", maxTimeToRun);

            //message queue ID
            sprintf(arg6,"%i", masterQueueId);

            /* execute the child process */
            execl("./user", "user", arg1, arg2, arg3, arg4, arg5, arg6,(char *) NULL);

        }
    }
}

//Checks message queue and returns the process location of the sender in the array
int getMessage(void) {
    struct msgbuf msg;

    if(msgrcv(masterQueueId, (void *) &msg, sizeof(msg.mText), 3, IPC_NOWAIT) == -1) {
        if(errno != ENOMSG) {
            perror("Error: OSS has failed to receive a message from the queue.");
            return -1;
        }
        if(verboseOn) {
            printf("No message was found in the queue for the OSS\n");
			if(fileLinesPrinted < LINE_LIMIT) {
					fprintf(file, "No message was found in the queue for the OSS\n");
					fileLinesPrinted++;
				}
        }
        return -1;
    }
	
	/* Success: there is a message */
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
	
	/* The child wants to request a resource so assign a resource if it is available to assign */
    if((resourceType = pcbGroup[processNum].request) >= 0) {
        requestResource(resourceType, processNum);
		
	/* The child wants to release a resource so release it */
	} else if ((resourceType = pcbGroup[processNum].release) >= 0) {
		releaseResource(resourceType, processNum);
		
	/* The process died so clear its fields for a new process to be spawned in its block */
    } else if(pcbGroup[processNum].processID == -1) {
        performProcessCleanup(processNum);
    } else {
        if(verboseOn) {
            printf("There is nothing to do for this message from child\n");
			if(fileLinesPrinted < LINE_LIMIT) {
				fprintf(file, "There is nothing to do for this message from child\n");
				fileLinesPrinted++;
			}
        }
    }
}

/* A process requesting a resource will be given 1 of the resource if its available to be given */
void processResourceRequests(void) {
   
    int i, j;
	/* Loop through all processes to see if any want a request, release, or if they died */
    for(i = 0; i < PCB_ARRAY_SIZE; i++) {
        int resourceType = -1;
        int quant;
     
		/* Finding the right process that is requesting a resource */
        //If the request flag is set with the value of a resource type, process the request
        if((resourceType = pcbGroup[i].request) >= 0) {
            if(verboseOn) {
                printf("Master has detected Process P%d requesting R:%d at time %llu.%03llu\n", i, resourceType, mainStruct->virtualClock / NANOPERSECOND, mainStruct->virtualClock % NANOPERSECOND);
                if(fileLinesPrinted < LINE_LIMIT) {
					fprintf(file, "Master has detected Process P:%d requesting R:%d at time %llu.%03llu\n", i, resourceType, mainStruct->virtualClock / NANOPERSECOND, mainStruct->virtualClock % NANOPERSECOND);
					fileLinesPrinted++;
				}
			}
            /* Assign a resource if available */
            requestResource(resourceType, i);
        }
        /* Finding any process that wants to be released */
        else if((resourceType = pcbGroup[i].release) >= 0) {
			if(verboseOn) {
				printf("Master has detected Process P:%d requesting R:%d at time %llu.%03llu\n", i, resourceType, mainStruct->virtualClock / NANOPERSECOND, mainStruct->virtualClock % NANOPERSECOND);
				if(fileLinesPrinted < LINE_LIMIT) {
					fprintf(file, "Master has detected Process P:%d requesting R:%d at time %llu.%03llu\n", i, resourceType, mainStruct->virtualClock / NANOPERSECOND, mainStruct->virtualClock % NANOPERSECOND);
					fileLinesPrinted++;
				}
			}
            releaseResource(resourceType, i);
        }
        /* The process is dead and resources will be returned to resource array */
        else if(pcbGroup[i].processID == -1) {
            performProcessCleanup(i);
        }
    }
}

/* Every 20 granted requests print each number of resources allocated to each process */
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
	for(i = 10; i < PCB_ARRAY_SIZE; i++){
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

/* A resource is given to a process */
void requestResource(int resourceType, int i) {
    int quant;
    if((quant = resourceArray[resourceType].quantAvail) > 0) {
		
		totalGrantedRequests++;
		
		if(((totalGrantedRequests % 20) == 0) && (totalGrantedRequests > 0)) {
			if(verboseOn && (fileLinesPrinted < LINE_LIMIT - 20)) {
				//printResourcesAllocatedToEachProcess();
				fileLinesPrinted = fileLinesPrinted + 21;
			}
		}
		
        if(verboseOn) {
            printf("There are %d out of %d for resource R:%d available\n", quant, resourceArray[resourceType].quantity, resourceType);
            printf("Increased resource R:%d for process P:%d\n", resourceType, i);
            
			if(fileLinesPrinted < LINE_LIMIT-1) {
				fprintf(file,"Process number %d has requested Resource# %d at time llu.%llu\n",i, resourceType,mainStruct->virtualClock / NANOPERSECOND, mainStruct->virtualClock % NANOPERSECOND);
				fprintf(file,"Increased resource %d for process %d\n", resourceType, i);
				fileLinesPrinted = fileLinesPrinted + 2;
			}
        }
       
	   /* Process is given 1 share of its requested resource */
        pcbGroup[i].allocation.quantity[resourceType]++;
		
		/* The amount of this resource available is 1 less after being allocated to the process */
        resourceArray[resourceType].quantAvail--;
		
        /* Process is no longer requesting the resource */
        pcbGroup[i].request = -1;
		
        if(verboseOn) {
            printf("After processing the request , there are now %d out of %d available of resource R:%d\n", resourceArray[resourceType].quantAvail, resourceArray[resourceType].quantity, resourceType);
            if(fileLinesPrinted < LINE_LIMIT) {
				fprintf(file,"After processing the request, there are now %d out of %d available of resource R:%d\n", resourceArray[resourceType].quantAvail, resourceArray[resourceType].quantity, resourceType);
				fileLinesPrinted++;
			}
        }
    }
}

/* A resource is released from a process */
void releaseResource(int resourceType, int i) {
    if(verboseOn) {
        printf("Releasing resouce %d from process %d\n", resourceType, i);
		if(fileLinesPrinted < LINE_LIMIT) {
			fprintf(file,"Releasing resouce %d from process %d at time %llu.%llu\n", resourceType, i ,mainStruct->virtualClock / NANOPERSECOND, mainStruct->virtualClock % NANOPERSECOND);
			fileLinesPrinted++;
		}
	}
    
	/* Transferring the resource from the PCB to the resource in the resource array */
    pcbGroup[i].allocation.quantity[resourceType]--;
    resourceArray[resourceType].quantAvail++;
	
    if(verboseOn) {
        printf("There are now %d out of %d for resource R:%d\n", resourceArray[resourceType].quantAvail, resourceArray[resourceType].quantity, resourceType);
        if(fileLinesPrinted < LINE_LIMIT) {
			fprintf(file,"There are now %d out of %d available for resource R:%d\n", resourceArray[resourceType].quantAvail, resourceArray[resourceType].quantity, resourceType);
			fileLinesPrinted++;
		}
	}
    
	/* The Process is no longer wanting this resoruce to be released */
    pcbGroup[i].release = -1;
}

void performProcessCleanup(int i) {
	
    if(verboseOn) {
        printf("Process %d has completed and will be killed off at time %llu.%llu\n", i, mainStruct->virtualClock / NANOPERSECOND, mainStruct->virtualClock % NANOPERSECOND);
		if(fileLinesPrinted < LINE_LIMIT) {
			fprintf(file,"Process %d has completed and will be killed off at time %llu.%llu\n", i, mainStruct->virtualClock / NANOPERSECOND, mainStruct->virtualClock % NANOPERSECOND);
			fileLinesPrinted++;
		}
    }
    
	// Allocations to the dead process are given back to the array of resources
    int j;
    for(j = 0; j < RSRC_ARR_SIZE; j++) {
        //If the quantity is > 0 for that resource, put them back
        if(pcbGroup[i].allocation.quantity[j] > 0) {
            if(verboseOn) {
                printf("Before the resources were returned there are %d out of %d for resource R:%d\n", resourceArray[j].quantAvail, resourceArray[j].quantity, j);
				if(fileLinesPrinted < LINE_LIMIT) {
					fprintf(file, "Before the resoruces were returned there are %d out of %d for resource R:%d\n", resourceArray[j].quantAvail, resourceArray[j].quantity, j);
					fileLinesPrinted++;
				}
			}
            /*Get the quantity to put back into the resource array */
            int returnQuant = pcbGroup[i].allocation.quantity[j];
			
            /*Increase the resource type quantAvail in the resource array */
            resourceArray[j].quantAvail += returnQuant;
            if(verboseOn) {
				printf("Returning %d of resource %d from process %d\n", returnQuant, j, i);
                printf("There are now %d out of %d for resource %d\n", resourceArray[j].quantAvail, resourceArray[j].quantity, j);
               
				if(fileLinesPrinted < LINE_LIMIT - 1) {
					fprintf(file,"Process %d returning %d of resource R:%d due to termination\n", i,returnQuant,j);
					fprintf(file,"There are now %d out of %d for resource R:%d\n", resourceArray[j].quantAvail, resourceArray[j].quantity, j);
					fileLinesPrinted = fileLinesPrinted + 2;
				}
			}
            //Set the quantity of the pcbGroup to zero
            pcbGroup[i].allocation.quantity[j] = 0;
        }
    }
    //Reset all values
    pcbGroup[i].processID = 0;
    pcbGroup[i].createTime = 0;
    pcbGroup[i].request = -1;
    pcbGroup[i].release = -1;
}

/*returns the number of deadlocked processes */
int deadlockCheck(void) {
	
    numberOfDeadlockDetectionRuns++;
    int resourceTempArr[RSRC_ARR_SIZE];
    int safeProcessArr[PCB_ARRAY_SIZE];
    int p;
    for(p = 0; p < RSRC_ARR_SIZE; p++) {
        resourceTempArr[p] = resourceArray[p].quantAvail;
    }
    for(p = 0; p < PCB_ARRAY_SIZE; p++) {
        safeProcessArr[p] = 0;
    }
    for(p = 0; p < PCB_ARRAY_SIZE; p++) {
		
		//If this pcb doesn't have a process, it won't be deadlocked
        if(pcbGroup[p].processID == 0) {
            safeProcessArr[p] = 1;
        }
		
		// if the process has spawned and it isn't making a request where the resources are unavailable
        if(!safeProcessArr[p] && processIsRuledSafe(resourceTempArr, p)) {
            safeProcessArr[p] = 1;
            int i;
			// For each resource add what is allocated to what is available to get the original amount of resources available
            for(i = 0; i < RSRC_ARR_SIZE; i++) {
               resourceTempArr[i] = resourceTempArr[i] + pcbGroup[p].allocation.quantity[i];
            }
			
			// Start this for-loop over again
			// In order to get an accurate max resources available
            p = -1;
        }
    }

	// check to see how many processes are deadlocked
    int deadlockCount = 0;
    for(p = 0; p < PCB_ARRAY_SIZE; p++) {
        if(!safeProcessArr[p]) {
            pcbGroup[p].deadlocked = 1;
			// Even when verbose is off Tell the user which processes are deadlocked
			printf("Process %d was found to be deadlocked at time %llu.%03llu\n", p, mainStruct->virtualClock / NANOPERSECOND, mainStruct->virtualClock % NANOPERSECOND);
            fprintf(file, "Process %d was found to be deadlocked at time %llu.%03llu\n", p, mainStruct->virtualClock / NANOPERSECOND, mainStruct->virtualClock % NANOPERSECOND);
            deadlockCount++;
        }
        else {
            pcbGroup[p].deadlocked = 0;
        }
    }
	
	/*  Print which processes are deadlocked to file and console */
	if(verboseOn && (fileLinesPrinted < LINE_LIMIT)) {
		outputDeadlockStatus(safeProcessArr, deadlockCount);
		fileLinesPrinted++;
	}
	return deadlockCount;
}

/* Returns false only if the process is making a request but resources are not available for that request */
int processIsRuledSafe(int *resourceTempArr, int p) {
	// If the process is not requesting any resource
    if(pcbGroup[p].request == -1)
        return 1;
	
	//If there are resources available for this process's request
    if(resourceTempArr[pcbGroup[p].request] > 0)
        return 1;

	// Otherwise the process is deadlocked
	return 0;
	
}

/* Output which processes are deadlocked if any */
void outputDeadlockStatus(int *safeProcessArr, int numDeadlocked){
	int i;
	if(numDeadlocked > 0) {
        printf("Processes: ");
        fprintf(file, "Processes: ");
		
        for(i = 0; i < PCB_ARRAY_SIZE; i++) {
            if(!safeProcessArr[i]) {
                printf("P:%d ", i);
				fprintf(file, "P:%d ", i);
            }
        }
		printf("are deadlocked\n");
		fprintf(file, "are deadlocked\n");
    } else {
		
		printf("The system is not found to have any deadlock\n");
		fprintf(file, "The system is not found to have any deadlock\n");
		
	}
}

/* Find the resource with the most amount of resources allocated to it and kill*/
void killAfterDeadlock(void) {
    int process;
    int max = 0;
    int i;
    int j;
    for(i = 0; i < PCB_ARRAY_SIZE; i++) {
        if(pcbGroup[i].deadlocked) {
            int total = 0;
            for(j = 0; j < RSRC_ARR_SIZE; j++) {
                total += pcbGroup[i].allocation.quantity[j];
            }
            if(total > max) {
                max = total;
                process = i;
            }
        }
    }

    pcbGroup[process].deadlocked = 0;
    pcbGroup[process].terminate = 1;
    performProcessCleanup(process);
	
	/* Even when verbose is off tell the user how deadlock was resolved */
	printf("Process %d was killed because it used the most total resources\n", process);
	if(fileLinesPrinted < LINE_LIMIT) {
		fprintf(file, "Process %d was killed because it used the most total resources\n", process);
		fileLinesPrinted++;
	}
}

/* When an interrupt is called everything is terminated and everything in shared mem is cleared */
void interruptHandler(int SIG) {
    signal(SIGQUIT, SIG_IGN);
    signal(SIGINT, SIG_IGN);

	// CTRL-C signal given
    if(SIG == SIGINT) {
        fprintf(stderr, "CTRL-C acknowledged, terminating everything\n");
        cleanup();
    }

	// alarm signal given
    if(SIG == SIGALRM) {
        fprintf(stderr, "The time limit has been reached, terminating everything\n");
        cleanup();
    }
}

/* functions to detach and remove shared memory items */
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

/* All processes and shared memory is detached and removed */
void cleanup() {
	
	//Even when verbose is off print the ending message
	printf("Program ending now: the system ran %d deadlock checks for this run\n", numberOfDeadlockDetectionRuns);
	if(fileLinesPrinted < LINE_LIMIT) {
		fprintf(file, "Program ending now: the system ran %d deadlock checks for this run\n", numberOfDeadlockDetectionRuns);
		fileLinesPrinted++;
	}
	signal(SIGQUIT, SIG_IGN);
	mainStruct->sigNotReceived = 0;
	kill(-getpgrp(), SIGQUIT);
	childPid = wait(&status);

    //Detach and remove the shared memory after all child process have died
    if(detachAndRemoveTimer(shmid, mainStruct) == -1) {
        perror("Error: Failed to destroy shared messageQ shared mem segment");
    }
	
    if(detachAndRemoveArray(pcbShmid, pcbGroup) == -1) {
        perror("Error: Failed to destroy shared pcb shared mem segment");
    }
	
    if(detachAndRemoveResource(resourceShmid, resourceArray) == -1) {
        perror("Error: Faild to destroy resource shared mem segment");
    }

    //Deleting the master's message queue
    msgctl(masterQueueId, IPC_RMID, NULL);

	// closing the file pointer
    if(fclose(file)) {
        perror("Error: failed to close the file pointer.");
    }

    exit(EXIT_SUCCESS);
}
