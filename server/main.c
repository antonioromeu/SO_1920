#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "fs.h"

#define MAX_COMMANDS 10
#define MAX_INPUT_SIZE 100
#define MAX_FILES 5

tecnicofs* fs;
char* socketName = NULL;
char* fileOutput = NULL;
char inputCommands[MAX_COMMANDS][MAX_INPUT_SIZE];
int numberCommands = 0;
int numberThreads = 0;
int numberBuckets = 1;
int headQueue = 0;
int socket = 1;
sem_t sem_prod;
sem_t sem_cons;
pthread_rwlock_t commandsLocker;
pthread_rwlock_t* vecLock;

#define INIT(A) { \
    vecLock = (pthread_rwlock_t*) malloc(sizeof(pthread_rwlock_t) * A); \
    if (pthread_rwlock_init(&commandsLocker, NULL)) \
        exit(EXIT_FAILURE); \
    for (int i = 0; i < A; i++) \
        if (pthread_rwlock_init(&vecLock[i], NULL)) \
            exit(EXIT_FAILURE); }
#define LOCK(A) pthread_rwlock_wrlock(&A)
#define UNLOCK(A) pthread_rwlock_unlock(&A)
#define DESTROY(A) { \
    if (pthread_rwlock_destroy(&commandsLocker)) \
        exit(EXIT_FAILURE); \
    for (int i = 0; i < A; i++) \
        if (pthread_rwlock_destroy(&vecLock[i])); }

static void displayUsage (const char* appName) {
    printf("Usage: %s input_filepath output_filepath threads_number buckets_number\n", appName);
    exit(EXIT_FAILURE);
}

static void parseArgs (long argc, char* const argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Invalid format:\n");
        displayUsage(argv[0]);
    }
    socketName = argv[1];
    fileOutput = argv[2];
}

int insertCommand(char* data) {
    sem_wait(&sem_prod); 
    LOCK(commandsLocker);
    if (numberCommands != MAX_COMMANDS) {
        strcpy(inputCommands[(numberCommands + headQueue) % MAX_COMMANDS], data);
        numberCommands++;
        UNLOCK(commandsLocker);
        sem_post(&sem_cons);
        return 1;
    }
    UNLOCK(commandsLocker);
    return 0;
}

char* removeCommand() {
    if (numberCommands > 0) {
        numberCommands--;
        char* command = inputCommands[headQueue % MAX_COMMANDS];
        headQueue = (headQueue + 1) % MAX_COMMANDS;
        return command;
    }
    return NULL;
}

void errorParse() {
    fprintf(stderr, "Error: command invalid\n");
}

void commandRename(char* name, char* rname) {
    int searchResult1;
    int searchResult2;
    if (hash(name, numberBuckets) == hash(rname, numberBuckets)) {
        LOCK(vecLock[hash(name, numberBuckets)]);
        searchResult1 = lookup(fs, name, numberBuckets);
        searchResult2 = lookup(fs, rname, numberBuckets);
        if (!searchResult1) {
            printf("%s file not found\n", name);
            UNLOCK(vecLock[hash(name, numberBuckets)]);
            return;
        }
        if (searchResult2) {
            printf("%s file already exists\n", rname);
            UNLOCK(vecLock[hash(name, numberBuckets)]);
            return;
        }
        delete(fs, name, numberBuckets);
        create(fs, rname, searchResult1, numberBuckets);
        UNLOCK(vecLock[hash(name, numberBuckets)]);
    }
    else if (hash(name, numberBuckets) < hash(rname, numberBuckets)) {
        LOCK(vecLock[hash(name, numberBuckets)]);
        LOCK(vecLock[hash(rname, numberBuckets)]);
        searchResult1 = lookup(fs, name, numberBuckets);
        if (!searchResult1) {
            printf("%s file not found\n", name);
            UNLOCK(vecLock[hash(name, numberBuckets)]);
            UNLOCK(vecLock[hash(rname, numberBuckets)]);
            return;
        }
        searchResult2 = lookup(fs, rname, numberBuckets);
        if (searchResult2) {
            printf("%s file already exists\n", rname);
            UNLOCK(vecLock[hash(name, numberBuckets)]);
            UNLOCK(vecLock[hash(rname, numberBuckets)]);
            return;
        }
        delete(fs, name, numberBuckets);
        create(fs, rname, searchResult1, numberBuckets);
        UNLOCK(vecLock[hash(rname, numberBuckets)]);
        UNLOCK(vecLock[hash(name, numberBuckets)]);
    }
    else {
        LOCK(vecLock[hash(rname, numberBuckets)]);
        LOCK(vecLock[hash(name, numberBuckets)]);
        searchResult1 = lookup(fs, name, numberBuckets);
        if (!searchResult1) {
            printf("%s file not found\n", name);
            UNLOCK(vecLock[hash(name, numberBuckets)]);
            UNLOCK(vecLock[hash(rname, numberBuckets)]);
            return;
        }
        searchResult2 = lookup(fs, rname, numberBuckets);
        if (searchResult2) {
            printf("%s file already exists\n", rname);
            UNLOCK(vecLock[hash(name, numberBuckets)]);
            UNLOCK(vecLock[hash(rname, numberBuckets)]);
            return;
        }
        delete(fs, name, numberBuckets);
        create(fs, rname, searchResult1, numberBuckets);
        UNLOCK(vecLock[hash(name, numberBuckets)]);
        UNLOCK(vecLock[hash(rname, numberBuckets)]);
    }
}

void* applyCommands() {
    while (1) {
        sem_wait(&sem_cons);
        LOCK(commandsLocker);
        const char* command = removeCommand();
	    if (!command) {
	        fprintf(stderr, "Error: command is null\n");
            UNLOCK(commandsLocker);
            exit(EXIT_FAILURE);
	    }
        if (command[0] == 'x') {
            headQueue = (headQueue - 1) % MAX_COMMANDS;
            numberCommands = numberCommands + 1;
            UNLOCK(commandsLocker);
            sem_post(&sem_cons);
            break;
        }
	    int iNumber;
        int searchResult;
        char token;
	    char name[MAX_INPUT_SIZE];
        char rname[MAX_INPUT_SIZE];
        if (command[0] == 'c')
            iNumber = obtainNewInumber(fs);
        int numTokens = sscanf(command, "%c %s", &token, name);
	    UNLOCK(commandsLocker);
        sem_post(&sem_prod);
        switch (token) {
	        case 'c':
	            if (numTokens != 2) {
	                fprintf(stderr, "Error: invalid command in Queue\n");
                    exit(EXIT_FAILURE);
                }
		        LOCK(vecLock[hash(name, numberBuckets)]);
                create(fs, name, iNumber, numberBuckets);
                UNLOCK(vecLock[hash(name, numberBuckets)]);
                break;
            case 'l':
	            if (numTokens != 2) {
	                fprintf(stderr, "Error: invalid command in Queue\n");
                    exit(EXIT_FAILURE);
                }
                LOCK(vecLock[hash(name, numberBuckets)]);
                searchResult = lookup(fs, name, numberBuckets);
                UNLOCK(vecLock[hash(name, numberBuckets)]);
                if (!searchResult)
                    printf("%s not found\n", name);
                else
                    printf("%s found with inumber %d\n", name, searchResult);
                break;
            case 'd':
	            if (numTokens != 2) {
	                fprintf(stderr, "Error: invalid command in Queue\n");
                    exit(EXIT_FAILURE);
                }
                LOCK(vecLock[hash(name, numberBuckets)]);
                delete(fs, name, numberBuckets);
                UNLOCK(vecLock[hash(name, numberBuckets)]);
                break;
            case 'r':
                numTokens = sscanf(command, "%c %s %s", &token, name, rname);
                if (numTokens != 3) {
                    fprintf(stderr, "Error: invalid command in Queue\n");
                    exit(EXIT_FAILURE);
                }
                commandRename(name, rname);
                break;
            default:
		        fprintf(stderr, "Error: command to apply\n");
                exit(EXIT_FAILURE);
        }
    }
    return NULL;
}

int createSocketStream(char* address) {
    int sockfd;
    struct sockaddr_un serv_addr;
    if ((sockfd = socket(AF_UNIX,SOCK_STREAM,0) ) < 0) 
        err_dump("Server: can't open stream socket");
    unlink(address); 
    bzero((char*) &serv_addr, sizeof(serv_addr)); 
    socket = sockfd; 
    serv_addr.sun_family = AF_UNIX; 
    strcpy(serv_addr.sun_path, address); 
    servlen = strlen(serv_addr.sun_path) + sizeof(serv_addr.sun_family); 
    if (bind(sockfd, (struct sockaddr*) &serv_addr, servlen) < 0)
        err_dump("Server, can't bind local address");
    listen(sockfd, MAX_FILES);
}
 
int acceptSocket() {
    int newsockfd, clilen, childpid, servlen;
    struct sockaddr_un cli_addr;
    for (;;) {
        clilen = sizeof(cli_addr);
        newsockfd = accept(socket, (struct sockaddr*) &cli_addr, &clilen);
        if (newsockfd < 0)
            err_dump("Server: accept error");
        if ((childpid = fork()) < 0)
            err_dump("Server: fork error");
        else if (childpid == 0) {
            close(socket); 
            /*faz cenas*/ 
            exit(0); 
        } 
    close(newsockfd); 
    }
}

int main(int argc, char* argv[]) {
    struct timeval start, end;
    double seconds, micros;
    parseArgs(argc, argv);
    gettimeofday(&start, NULL);
    inode_table_init();
    fs = new_tecnicofs(numberBuckets);
    gettimeofday(&end, NULL);
    FILE* fptr = fopen(fileOutput, "w");
    print_tecnicofs_tree(fptr, fs, numberBuckets);
    fclose(fptr);
    free_tecnicofs(fs, numberBuckets);
    inode_table_destroy();
    seconds = (double) (end.tv_sec - start.tv_sec);
    micros = (double) ((seconds + (double) (end.tv_usec - start.tv_usec)/1000000));
    printf("TecnicoFS completed in %.4f seconds.\n", micros);
    exit(EXIT_SUCCESS);
}
