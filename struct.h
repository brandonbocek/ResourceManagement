#ifndef STRUCT_H
#define STRUCT_H
#include <sys/types.h>
#include <stdbool.h>

#define NANOPERSECOND 1000000000

//static const long long NANO_MODIFIER = 1000000000;


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
#endif