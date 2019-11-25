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
pthread_rwlock_t locker;

#define INIT { \
    if (pthread_rwlock_init(&locker, NULL)) \
        exit(EXIT_FAILURE); \
#define LOCK pthread_rwlock_wrlock(&locker)
#define UNLOCK pthread_rwlock_unlock(&locker)
#define DESTROY { \
    if (pthread_rwlock_destroy(&locker)) \
        exit(EXIT_FAILURE); \

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
    lock_inode_table();
    //LOCK(locker);
    if (numberCommands != MAX_COMMANDS) {
        //inode_create(socket, 
        strcpy(inputCommands[(numberCommands + headQueue) % MAX_COMMANDS], data);
        numberCommands++;
        UNLOCK(locker);
        sem_post(&sem_cons);
        return 1;
    }
    UNLOCK(locker);
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

int createSocket(char* address) {
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
 
int treatSocket() {
    int newsockfd, clilen, childpid, servlen;
    struct sockaddr_un cli_addr;
    pthread_t tid;
    for (;;) {
        clilen = sizeof(cli_addr);
        newsockfd = accept(socket, (struct sockaddr*) &cli_addr, &clilen);
        if (newsockfd < 0)
            err_dump("Server: accept error");
        if ((childpid = fork()) < 0)
            err_dump("Server: fork error");
        else if (childpid == 0) {
            close(socket);
            inode_t fich[5];
            pthread_create(&tid, NULL, trata_cliente_stream, novosockfd, fich);
            exit(0);
        }
    close(newsockfd); 
    }
}

int open(char* filename, int mode, inode_t[] fich) {
    int iNumber = lookup(fs, filename, numBuckets);
    inode_t openNode = NULL;
    for (int i = 0; i < 5; i++) {
        if (fich[i] == NULL)
            break;
        exit(EXIT_FAILURE);
    }
    if (mode == 0)
        exit(EXIT_FAILURE);
    openNode = (inode_t) malloc(sizeof(struct inode_t));
    if(inode_get(iNumber, openNode.owner, openNode.ownerPermissions, openNode.otherPermissions, openNode.fileContent, strlen(openNode.fileContent)) == -1) {
        perror("Nao leu\n");
        exit(EXIT_FAILURE);
    }
    fich[i] = openNode;
    
}

int close(int fd) {
    
}

void trata_cliente_stream(int sockfd, inode_t[] fich) {
    int n = 0;
    int numTokens = 0;
    int fd, len, mode, inumber;
    char buffer[MAXLINHA + 1];
    char command = NULL;
    char filename, newFilename, sendBuffer;
    pthread_t tid;
    n = read(sockfd, buffer, MAXLINHA + 1);
    if (n < 0)
        perror("Erro servidor no read");
    token = buffer[0];
    switch (token) {
        case 'c':
            numTokens = sscanf(buffer, "%c %d %d", command, ownerPermissions, othersPermissions);
            if (numTokens != 3){
                perror("erro no comando\n");
                exit(EXIT_FAILURE);
            }
            LOCK;
            iNumber = obtainNewInumber(fs);
            create(fs, name, iNumber, numberBuckets);
            UNLOCK;
            inode_create(sockfd, (permission) ownerPremissions, (permission) othersPermissions);
            break;    
        case 'd':
            numTokens = sscanf(buffer, "%d %s", command, filename);
            if (numTokens != 2) {
                perror("erro no comando\n");
                exit(EXIT_FAILURE);
            }
            LOCK;
            delete(fs, filename, numberBuckets);
            inumber = lookup(fs, filename, numBuckets);
            UNLOCK;
            inode_delete(inumber);
            break;
        case 'r':
            numTokens = sscanf(buffer, "%c %s %s", command, filename, newFilename);
            if (numTokens != 3) {
                perror("erro no comando\n");
                exit(EXIT_FAILURE);
            }
            LOCK;
            commandRename(name, rname);
            UNLOCK;
            break;
        case 'o':
            numTokens = sscanf(buffer, "%c %s %d", command, filename, mode);
            if (numTokens != 3) {
                perror("erro no comando");
                exit(EXIT_FAILURE);
            }
            open(filename, mode);
            break;
        case 'x':
            numTokens = sscanf(buffer, "%c %d", command, fd);
            if (numTokens != 2) {
                perror("erro no comando\n");
                exit(EXIT_FAILURE);
            }
            close(fd);
            break;
        case 'l':
            numTokens = sscanf(buffer, "%c %d %d", command, fd, len);
            if (numTokens != 3) {
                perror("erro no comando\n");
                exit(EXIT_FAILURE);
            }
            read(fd, buffer, len);
            break;
        case 'w':
            numTokens = sscanf(buffer, "%c %d %s", command, fd, sendBuffer);
            if (numTokens != 3) {
                perror("erro no comando\n");
                exit(EXIT_FAILURE);
            }
            write(fd, buffer, len);
            break;
        default:
            perror("Erro default\n");
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
