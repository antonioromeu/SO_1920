#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
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
int sockerServer = 1;
sem_t sem_prod;
sem_t sem_cons;
pthread_rwlock_t locker;

typedef struct fichNode {
    int fd;
    int iNumber;
} fichNode;

#define INIT { \
    if (pthread_rwlock_init(&locker, NULL)) \
        exit(EXIT_FAILURE); }
#define LOCK pthread_rwlock_wrlock(&locker)
#define UNLOCK pthread_rwlock_unlock(&locker)
#define DESTROY { \
    if (pthread_rwlock_destroy(&locker)) \
        exit(EXIT_FAILURE); }

static void displayUsage(const char* appName) {
    printf("Usage: %s input_filepath output_filepath threads_number buckets_number\n", appName);
    exit(EXIT_FAILURE);
}

static void parseArgs(long argc, char* const argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Invalid format:\n");
        displayUsage(argv[0]);
    }
    socketName = argv[1];
    fileOutput = argv[2];
}

void commandRename(char* name, char* rname) {
    int searchResult1;
    int searchResult2;
    if (hash(name, numberBuckets) == hash(rname, numberBuckets)) {
        LOCK;
        searchResult1 = lookup(fs, name, numberBuckets);
        searchResult2 = lookup(fs, rname, numberBuckets);
        if (!searchResult1) {
            printf("%s file not found\n", name);
            return;
        }
        if (searchResult2) {
            printf("%s file already exists\n", rname);
            return;
        }
        delete(fs, name, numberBuckets);
        createFS(fs, rname, searchResult1, numberBuckets);
        UNLOCK;
    }
    else if (hash(name, numberBuckets) < hash(rname, numberBuckets)) {
        LOCK;
        searchResult1 = lookup(fs, name, numberBuckets);
        if (!searchResult1) {
            printf("%s file not found\n", name);
            UNLOCK;
            return;
        }
        searchResult2 = lookup(fs, rname, numberBuckets);
        if (searchResult2) {
            printf("%s file already exists\n", rname);
            UNLOCK;
            return;
        }
        delete(fs, name, numberBuckets);
        createFS(fs, rname, searchResult1, numberBuckets);
        UNLOCK;
    }
    else {
        LOCK;
        searchResult1 = lookup(fs, name, numberBuckets);
        if (!searchResult1) {
            printf("%s file not found\n", name);
            UNLOCK;
            return;
        }
        searchResult2 = lookup(fs, rname, numberBuckets);
        if (searchResult2) {
            printf("%s file already exists\n", rname);
            UNLOCK;
            return;
        }
        delete(fs, name, numberBuckets);
        createFS(fs, rname, searchResult1, numberBuckets);
        UNLOCK;
    }
}

int createSocket(char* address) {
    int sockfd, servlen;
    struct sockaddr_un serv_addr;
    if ((sockfd = socket(AF_UNIX,SOCK_STREAM,0) ) < 0) {
        err_dump("Server: can't open stream socket");
        //perror("Server: can't open stream socket");
        //exit(EXIT_FAILURE);
    }
    unlink(address); 
    bzero((char*) &serv_addr, sizeof(serv_addr)); 
    sockerServer = sockfd; 
    serv_addr.sun_family = AF_UNIX; 
    strcpy(serv_addr.sun_path, address); 
    servlen = strlen(serv_addr.sun_path) + sizeof(serv_addr.sun_family); 
    if (bind(sockfd, (struct sockaddr*) &serv_addr, servlen) < 0)
        err_dump("Server, can't bind local address");
    listen(sockfd, MAX_FILES);
}

int open(char* filename, int mode) {
    int fd = 0;
    int  flag = 0;
    int iNumber = lookup(fs, filename, numberBuckets);
    inode_t* openNode;
    if (!mode)
        exit(EXIT_FAILURE);
    openNode = (inode_t*) malloc(sizeof(struct inode_t));
    if (inode_get(iNumber, &openNode->owner, &openNode->ownerPermissions, &openNode->othersPermissions, openNode->fileContent, strlen(openNode->fileContent)) == -1) {
        perror("Nao leu\n");
        exit(EXIT_FAILURE);
    }
    if (openNode->owner == getuid() && openNode->ownerPermissions == mode)
        flag = openNode->ownerPermissions;
    else if (openNode->owner != getuid() && openNode->othersPermissions == mode)
        flag = openNode->othersPermissions;
    else
        perror("Cliente com acesso negado\n");
        exit(EXIT_FAILURE);
    fd = open(filename, flag);
    return fd;
}

int close(int fd) {
    for (int i = 0; i < 5; i++) {
        if (fich[i].fd == fd) {
            fich[i] = NULL;
            close(fd);
            return 0;
        }
    }
    perror("Erro no close/nao encontrou\n");
    exit(EXIT_FAILURE);
}

int read(int fd, char* buffer, int len) {
    int iNumber;
    for (int i = 0; i < 5; i++) {
        if (fich[i].fd == fd) {
            openNode = (inode_t) malloc(sizeof(struct inode_t));
            if (inode_get(fich[i]->iNumber, openNode->owner, openNode->ownerPermissions, openNode->othersPermissions, openNode->fileContent, strlen(openNode->fileContent)) == -1) {
                perror("Nao leu\n");
                exit(EXIT_FAILURE);
            }
            if (strlen(openNode->fileContent) > len - 1) {
                perror("Demasiado big eheh\n");
                exit(EXIT_FAILURE);
            }
            if ((openNode->onwer == getuid() && (openNode->ownerPermissions == 1 || openNode->ownerPermissions == 3)) || (openNode->onwer != getuid() && (openNode->othersPermissions == 1 || openNode->othersPermissions == 3))) {
                strcpy(buffer, openNode->fileContent);
                return (strlen(buffer) - 1);
            }
            else {
                perror("Nao pode ser lido eheh\n");
                exit(EXIT_FAILURE);
            }
        }
        perror("Nao esta aberto eheh\n");
        exit(EXIT_FAILURE);
    }
}

int write(int fd, char* buffer, int len) {
    for (int i = 0; i < 5; i++) {
        if (fich[i].fd == fd) {
            if ((openNode->onwer == getuid() && (openNode->ownerPermissions == 2 || openNode->ownerPermissions == 3)) || (openNode->onwer != getuid() && (openNode->othersPermissions == 2 || openNode->othersPermissions == 3))) {
                if (inode_set(fich[i].iNumber, buffer, len) == -1) {
                    perror("Nao settou\n");
                    exit(EXIT_FAILURE);
                }
                return len - 1;
            }
            else {
                perror("Nao pode ser lido eheh\n");
                exit(EXIT_FAILURE);
            }
        }
    }
    perror("Nao esta aberto eheh\n");
    exit(EXIT_FAILURE);
}

int create(char* filename, int ownerPremissions, int othersPermissions) {
    if (lookup(fs, filename, numberBuckets)) {
        perror("ficheiro ja existe eheh\n");
        exit(EXIT_FAILURE);
    }
    if (inode_create(geteuid(), (permission) ownerPremissions, (permission) othersPermissions) == -1) {
        perror("erro ao criar o inode\n");
        exit(EXIT_FAILURE);
    }
    LOCK;
    int iNumber = obtainNewInumber(fs);
    createFS(fs, filename, iNumber, numberBuckets);
    UNLOCK; 
    return 0; 
}

void trata_cliente_stream(int sockfd, int[] fich) {
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
            create(filename, owenerPermissions, othersPermissions);
            break;    
        case 'd':
            numTokens = sscanf(buffer, "%d %s", command, filename);
            if (numTokens != 2) {
                perror("erro no comando\n");
                exit(EXIT_FAILURE);
            }
            LOCK;
            inumber = lookup(fs, filename, numBuckets);
            delete(fs, filename, numberBuckets);
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
            for (int i = 0; i < 5; i++) {
                if (fich[i] == NULL)
                    break;
                perror("Numero max de ficheiros abertos esgotados\n");
                exit(EXIT_FAILURE);
            }
            fd = open(filename, mode);
            fichNode fN = NULL;
            fN.fd = fd;
            fN.iNumber = lookup(fs, filename, numBucket);
            fich[i] = fN;
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

int treatSocket() {
    int newsockfd, clilen, childpid, servlen;
    struct sockaddr_un cli_addr;
    pthread_t tid;
    for (;;) {
        clilen = sizeof(cli_addr);
        newsockfd = accept(sockerServer, (struct sockaddr*) &cli_addr, &clilen);
        if (newsockfd < 0)
            err_dump("Server: accept error");
        if ((childpid = fork()) < 0)
            err_dump("Server: fork error");
        else if (childpid == 0) {
            close(sockerServer);
            fichNode fich[5];
            pthread_create(&tid, NULL, trata_cliente_stream, newsockfd, fich);
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
