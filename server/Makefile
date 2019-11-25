# Makefile, versao 1
# Sistemas Operativos, DEI/IST/ULisboa 2019-20

CC   = gcc
LD   = gcc
CFLAGS = -Wall -std=gnu99 -I../

LDFLAGS=-lm -pthread

# A phony target is one that is not really the name of a file
# https://www.gnu.org/software/make/manual/html_node/Phony-Targets.html
.PHONY: all clean run

all: tecnicofs-rwlock

tecnicofs-rwlock: lib/bst.o lib/hash.o lib/inodes.o fs.o main-rwlock.o
	$(LD) $(CFLAGS) $(LDFLAGS) -DRWLOCK -o tecnicofs-rwlock lib/bst.o lib/hash.o lib/inodes.o fs.o main-rwlock.o

lib/bst.o: lib/bst.c lib/bst.h
	$(CC) $(CFLAGS) -o lib/bst.o -c lib/bst.c

lib/hash.o: lib/hash.c lib/hash.h
	$(CC) $(CFLAGS) -o lib/hash.o -c lib/hash.c

lib/inodes.o: lib/inodes.c lib/inodes.h
	$(CC) $(CFLAGS) -o lib/inodes.o -c lib/inodes.c

fs.o: fs.c fs.h lib/bst.h lib/hash.h
	$(CC) $(CFLAGS) -o fs.o -c fs.c

main-rwlock.o: main.c fs.h lib/bst.h lib/hash.h lib/inodes.h
	$(CC) $(CFLAGS) -DRWLOCK -o main-rwlock.o -c main.c

clean:
	@echo Cleaning...
	rm -f lib/*.o *.o tecnicofs-rwlock

run: tecnicofs
	./tecnicofs
