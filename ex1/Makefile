# Makefile, versao 1
# Sistemas Operativos, DEI/IST/ULisboa 2019-20

CC   = gcc
LD   = gcc
CFLAGS = -Wall -std=gnu99 -I../

LDFLAGS=-lm -pthread

# A phony target is one that is not really the name of a file
# https://www.gnu.org/software/make/manual/html_node/Phony-Targets.html
.PHONY: all clean run

all: tecnicofs-nosync tecnicofs-mutex tecnicofs-rwlock

tecnicofs-nosync: lib/bst.o fs.o main-nosync.o
	$(LD) $(CFLAGS) $(LDFLAGS) -o tecnicofs-nosync lib/bst.o fs.o main-nosync.o

tecnicofs-mutex: lib/bst.o fs.o main-mutex.o
	$(LD) $(CFLAGS) $(LDFLAGS) -DMUTEX -o tecnicofs-mutex lib/bst.o fs.o main-mutex.o

tecnicofs-rwlock: lib/bst.o fs.o main-rwlock.o
	$(LD) $(CFLAGS) $(LDFLAGS) -DRWLOCK -o tecnicofs-rwlock lib/bst.o fs.o main-rwlock.o

lib/bst.o: lib/bst.c lib/bst.h
	$(CC) $(CFLAGS) -o lib/bst.o -c lib/bst.c

fs.o: fs.c fs.h lib/bst.h
	$(CC) $(CFLAGS) -o fs.o -c fs.c

main-nosync.o: main.c fs.h lib/bst.h
	$(CC) $(CFLAGS) -o main-nosync.o -c main.c

main-mutex.o: main.c fs.h lib/bst.h
	$(CC) $(CFLAGS) -DMUTEX -o main-mutex.o -c main.c

main-rwlock.o: main.c fs.h lib/bst.h
	$(CC) $(CFLAGS) -DRWLOCK -o main-rwlock.o -c main.c

clean:
	@echo Cleaning...
	rm -f lib/*.o *.o tecnicofs-nosync tecnicofs-mutex tecnicofs-rwlock

run: tecnicofs
	./tecnicofs