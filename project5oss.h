#ifndef MASTER_H
#define MASTER_H
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
#include "struct.h"

#define NANOPERSECOND 1000000000



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
long long timeToSpawn = 0;
long long idleTime = 0;
long long turnaroundTime = 0;
long long processWaitTime = 0;
long long totalProcessLifeTime = 0;
int totalProcessesSpawned = 0;
int messageReceived = 0;
long long *virtualClock;

struct clockStruct *mainStruct;

//Constants for timing the program
const long long MAX_TIME = 20000000000;
const int MAX_FUTURE_SPAWN = 280000001;
const int MAX_TOTAL_PROCESS_TIME = 700000001;
const int CHANCE_HIGH_PRIORITY = 20;
const int MAXSLAVE = 20;
const int ARRAY_SIZE = 18;
FILE *file;


int MAX_IDLE_INCREMENT = 10001;
key_t timerKey = 624353;
key_t pcbArrayKey = 8345634;
key_t resourceKey = 474542;
key_t masterQueueKey = 232523;
key_t clockKey = 5314534;
#endif