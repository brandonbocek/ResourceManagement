#include "project5.h"

int main (int argc, char **argv) {

  timeoutValue = 30;
  shmid = 0;
  pcbShmid = 0;
  resourceShmid = 0;
  myPid = getpid();

  
  //process cmd line arguments passed by master
    processMasterArguments(argv);

	//attach memory segments
	attachSharedMemorySegments();

  srand(time(NULL) + getpid());

 

  //Ignore SIGINT so that it can be handled below
  signal(SIGINT, SIG_IGN);

  //Set the sigquitHandler for the SIGQUIT signal
  signal(SIGQUIT, sigquitHandler);

  //Set the alarm handler
  signal(SIGALRM, killLeftoverProcesses);

  //Set the default alarm time
  alarm(QUIT_TIMEOUT);

  
  alarm(timeoutValue);


  long long duration;
  int notFinished = 1;

  do {
  
    //If request flag -1, not waiting
    if(pcbGroup[processNumber].request == -1 && pcbGroup[processNumber].release == -1) {
      //Check to see if process will terminate
      if(willTerminate()) {
        notFinished = 0;
      }
      //if not take other actions, choose resources to request
      else {
        if(takeAction()) {
          int choice = rand() % 2;
          //Request a resource
          if(choice) {
           pcbGroup[processNumber].request = pickResourceToRequest();  
           sendMessage(masterQueueId, 3);
          }
          //release a reasource randomly
          else {
            int i;
            for(i = 0; i < 20; i++) {
              if(pcbGroup[processNumber].allocation.quantity[i] > 0) {
                pcbGroup[processNumber].release = i;
                break;
              }
            }
            sendMessage(masterQueueId, 3);
          }
        }
      }
    }
  } while (notFinished && mainStruct->sigNotReceived && !pcbGroup[processNumber].terminate);

  if(!pcbGroup[processNumber].terminate) {
    pcbGroup[processNumber].processID = -1;
    sendMessage(masterQueueId, 3);
  }

  if(shmdt(mainStruct) == -1) {
    perror("    Slave could not detach shared memory struct");
  }

  if(shmdt(pcbGroup) == -1) {
    perror("    Slave could not detach from shared memory array");
  }

  if(shmdt(resourceArray) == -1) {
    perror("    Slave could not detach from resource array");
  }

  kill(myPid, SIGTERM);
  sleep(1);
  kill(myPid, SIGKILL);


}

//**************FUNCTIONS****************************
//get arguments passed by master

void processMasterArguments(char ** argv) {

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

}

//attached shared mem segments
void attachSharedMemorySegments() {

//Try to attach to shared memory
  if((mainStruct = (clockStruct *)shmat(shmid, NULL, 0)) == (void *) -1) {
        fprintf(stderr, "Could not attach memory segment %i for the virtual clock.\n", shmid);
    exit(1);
  }

  if((pcbGroup = (PCB *)shmat(pcbShmid, NULL, 0)) == (void *) -1) {
        fprintf(stderr, "Could not attach memory segment %i for the process control block (PCB)\n", pcbShmid);
    exit(1);
  }

  if((resourceArray = (resource *)shmat(resourceShmid, NULL, 0)) == (void *) -1) {
        fprintf(stderr, "Could not attach memory segment %i for the resource control block \n", resourceShmid);
    exit(1);
  }
  
   //if((masterQueueId = msgget(masterKey, IPC_CREAT | 0777)) == -1) {
    //perror("    Slave msgget for master queue");
    //exit(-1);
  //}


}

int willTerminate(void) {
  if(mainStruct->virtualClock - pcbGroup[processNumber].createTime >= NANOPERSECOND) {
    int choice = 1 + rand() % 5;
    return choice == 1 ? 1 : 0;
  }
  return 0;
}

int pickResourceToRequest(void) {
  int choice = rand() % 20;
  return choice;
}

int takeAction(void) {
  int choice = rand() % 2;
  return choice;
}

void sendMessage(int qid, int msgtype) {
  struct msgbuf msg;

  msg.mType = msgtype;
  sprintf(msg.mText, "%d", processNumber);

  if(msgsnd(qid, (void *) &msg, sizeof(msg.mText), IPC_NOWAIT) == -1) {
    perror("    Slave msgsnd error");
  }
}

//This handles SIGQUIT being sent from parent process
//It sets the volatile int to 0 so that it will not enter in the CS.
void sigquitHandler(int sig) {

  if(shmdt(mainStruct) == -1) {
    //perror("    Slave could not detach shared memory");
  }

  if(shmdt(pcbGroup) == -1) {
    //perror("    Slave could not detach from shared memory array");
  }

  if(shmdt(resourceArray) == -1) {
    //perror("    Slave could not detach from resource array");
  }

  kill(myPid, SIGKILL);

  //The slaves have at most 5 more seconds to exit gracefully or they will be SIGTERM'd
  alarm(5);
}

//function to kill itself if the alarm goes off,
//signaling that the parent could not kill it off
void killLeftoverProcesses(int sig) {
  kill(myPid, SIGTERM);
  sleep(1);
  kill(myPid, SIGKILL);
}
