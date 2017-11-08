#ifndef PROJECT5_H
#define PROJECT5_H

#include <ctype.h>
#include <sys/wait.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/msg.h>
#include <errno.h>
#include <time.h>
#include <stdbool.h>

#define NANOPERSECOND 1000000000

typedef struct clockStruct {
  long long virtualClock;
  int sigNotReceived;
  pid_t scheduledProcess;
} clockStruct;

typedef struct msgbuf {
	long mType;
	char mText[80];
} msgbuf;


typedef struct resourceAlloc {
	int quantity[20];
} resourceAlloc;

typedef struct PCB {
  pid_t processID;
  int request;
  int release;
  int deadlocked;
  int terminate;
  resourceAlloc allocation;
  long long totalTimeRan;
  long long createTime;
} PCB;

typedef struct resource {
  int quantity;
  int quantAvail;
} resource;

//OSS functions
void forkChild(void);
bool isTimeToSpawn(void);
void setTimeToSpawn(void);
int iterateClock(void);
int getMessage(void);
void processMessage(int);
void setupResources(void);
void resourceSnapshot(void);
void requestResource(int, int);
void releaseResource(int, int);
void performProcessCleanup(int);
int deadlockCheck(void);
int reqLtAvail(int*, int);
void killAfterDeadlock(void);
void processResourceRequests(void);
void interruptHandler(int);
void cleanup(void);
void sendMessage(int, int);
int detachAndRemoveTimer(int, clockStruct*);
int detachAndRemoveArray(int, PCB*);
int detachAndRemoveResource(int, resource*);
void printHelpMessage(void);
void evaluateCmdLineArguments(int, char **);
void createANDattachMemorySegments(void);
void initializePCBStruct(void);
void printResourcesAllocatedToEachProcess();


//User functions
int willTerminate(void);
void sendMessage(int, int);
int pickResourceToRequest(void);
int takeAction(void);
void alarmHandler(int);
void sigquitHandler(int);
void killLeftoverProcesses(int);
void processMasterArguments(char **);
void attachSharedMemorySegments(void); 


//PCB Array//
PCB *pcbGroup;
resource *resourceArray;


pid_t myPid, childPid;
int tValue = 20;
int vFlag = 0;
int checkDeadlockFlag = 0;
long long lastDeadlockCheck = 0;
int status;
int shmid;
int pcbShmid;
int resourceShmid;
int clockShmid;
int slaveQueueId;
int masterQueueId;
int nextProcessToSend = 1;
int processNumberBeingSpawned = -1;
long long timeToSpawn = 0;	//First child spawns at time 0
long long idleTime = 0;
long long turnaroundTime = 0;
long long processWaitTime = 0;
long long totalProcessLifeTime = 0;
int totalProcessesSpawned = 0;
int messageReceived = 0;
long long *virtualClock;
int totalGrantedRequests;

//User variables
//pid_t myPid;
long long *ossTimer;
struct clockStruct *mainStruct;
int processNumber = 0;
//int masterQueueId;
const int QUIT_TIMEOUT = 10;
struct PCB *pcbGroup;
resource *resourceArray;
//int shmid, pcbShmid, resourceShmid;
int timeoutValue;

struct clockStruct *mainStruct;

//Constants for timing the program
const long long MAX_TIME = 20000000000;
const long long MIN_FUTURE_SPAWN = 1000000;
const int MAX_FUTURE_SPAWN = 500000000;
const int MAX_TOTAL_PROCESS_TIME = 700000001;
const int CHANCE_HIGH_PRIORITY = 20;
const int MAXSLAVE = 20;
const int ARRAY_SIZE = 18;
FILE *file;


int MAX_IDLE_INCREMENT = 10001;
key_t timerKey = 5745774;
key_t pcbArrayKey = 2143324;
key_t resourceKey = 9475843;
key_t masterQueueKey = 5489589;
key_t clockKey = 9327843;

#ENDIF