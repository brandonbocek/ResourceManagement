
#ifndef SLAVE_H
#define SLAVE_H
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
#include <signal.h>
#include <errno.h>
#include <time.h>
#include "struct.h"
#define NANOPERSECOND 1000000000

//function prototypes
int willTerminate(void);
void sendMessage(int, int);
int pickResourceToRequest(void);
int takeAction(void);
void alarmHandler(int);
void sigquitHandler(int);
void killLeftoverProcesses(int);
void processMasterArguments(char **);
void attachSharedMemorySegments(void); 

//variables
pid_t myPid;
long long *ossTimer;
struct clockStruct *mainStruct;
int processNumber = 0;
int masterQueueId;
const int QUIT_TIMEOUT = 10;
struct PCB *pcbGroup;
resource *resourceArray;
int shmid, pcbShmid, resourceShmid;
int timeoutValue;

#endif