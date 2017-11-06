all: oss user
.PHONY: clean

OSSOBJ = oss.o 
USEROBJ = user.o
CC = gcc -g

oss: $(OSSOBJ)
	$(CC) -o oss oss.o
user: $(USEROBJ)
	$(CC) -o user user.o
oss.o: oss.c struct.h  project5oss.h
	$(CC) -c oss.c
user.o: user.c struct.h project5user.h
	$(CC) -c user.c
clean:
	-rm oss user $(OSSOBJ) $(USEROBJ)

