#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "fs.h"

#define _GNU_SOURCE
#define MAX_FILES 5
#define MAX_CONNECTIONS_WAITING 10
#define MAX_THREADS 10
#define MAX_BUFFER_SZ 100

tecnicofs* fs;
char* socketName = NULL;
char* fileOutput = NULL;
int numberBuckets = 1;
int listenningSocket = 1;
int socketServer = 0;
sem_t sem_prod;
sem_t sem_cons;
pthread_rwlock_t locker;

typedef struct fileNode {
    int fd;
    int iNumber;
} fileNode;

typedef struct arg_struct {
    int acceptedSocket;
    int userID;
    fileNode* files;
} args;

struct ucred {
    pid_t pid;
    uid_t uid;
    gid_t gid;
};

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

void renameFile(char* name, char* rname) {
    int searchResult1;
    int searchResult2;
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
    create(fs, rname, searchResult1, numberBuckets);
    UNLOCK;
}

void createSocket(char* address) {
    int servlen;
    struct sockaddr_un serv_addr;
    if ((socketServer = socket(AF_UNIX,SOCK_STREAM,0) ) < 0) {
        perror("Servidor nao consegue stream sockey eheh\n");
        exit(EXIT_FAILURE);
    }
    unlink(address); 
    bzero((char*) &serv_addr, sizeof(serv_addr)); 
    serv_addr.sun_family = AF_UNIX; 
    strcpy(serv_addr.sun_path, address); 
    servlen = strlen(serv_addr.sun_path) + sizeof(serv_addr.sun_family); 
    if (bind(socketServer, (struct sockaddr*) &serv_addr, servlen) < 0) {
        perror("Servidor nao consegue bindar eheh\n");
        exit(EXIT_FAILURE);
    }
    listen(socketServer, MAX_CONNECTIONS_WAITING);
}

int openFile(char* filename, int mode, fileNode files[], int clientID) {
    int fd, flag, i = 0;
    int iNumber = lookup(fs, filename, numberBuckets);
    fileNode file;
    inode_t* openNode;
    if (mode == 0) {
        perror("cant touch this\n");
        exit(EXIT_FAILURE);
    }
    while (i < MAX_FILES) {
        if (files[i].fd == -1)
            break;
        i++;
    }
    if (i == MAX_FILES) {
        perror("Numero max de ficheiros abertos esgotados\n");
        exit(EXIT_FAILURE);
    }
    openNode = (inode_t*) malloc(sizeof(struct inode_t));
    if (inode_get(iNumber, &openNode->owner, &openNode->ownerPermissions, &openNode->othersPermissions, openNode->fileContent, strlen(openNode->fileContent)) == -1) {
        perror("Nao leu\n");
        exit(EXIT_FAILURE);
    }
    if (openNode->owner == clientID && openNode->ownerPermissions == mode)
        flag = openNode->ownerPermissions;
    else if (openNode->owner != clientID && openNode->othersPermissions == mode)
        flag = openNode->othersPermissions;
    else {
        perror("Cliente com acesso negado\n");
        exit(EXIT_FAILURE);
    }
    fd = open(filename, flag);
    file.fd = fd;
    file.iNumber = iNumber;
    files[i] = file;
    return fd;
}

int closeFile(int fd, fileNode files[]) {
    for (int i = 0; i < 5; i++) {
        if (files[i].fd == fd) {
            files[i].fd = -1;
            close(fd);
            return 0;
        }
    }
    perror("Erro no close/nao encontrou\n");
    exit(EXIT_FAILURE);
}

int readFile(int fd, char* buffer, int len, fileNode files[], int clientID) {
    inode_t* openNode = NULL;
    for (int i = 0; i < 5; i++) {
        if (files[i].fd == fd) {
            openNode = (inode_t*) malloc(sizeof(struct inode_t));
            if (inode_get(files[i].iNumber, &openNode->owner, &openNode->ownerPermissions, &openNode->othersPermissions, openNode->fileContent, strlen(openNode->fileContent)) == -1) {
                perror("Nao leu\n");
                exit(EXIT_FAILURE);
            }
            if (strlen(openNode->fileContent) > len - 1) {
                perror("Grande demais\n");
                exit(EXIT_FAILURE);
            }
            if ((openNode->owner == clientID && (openNode->ownerPermissions == 1 || openNode->ownerPermissions == 3)) || (openNode->owner != clientID && (openNode->othersPermissions == 1 || openNode->othersPermissions == 3))) {
                strcpy(buffer, openNode->fileContent);
                return (strlen(buffer) - 1);
            }
            else {
                perror("Nao pode ser lido\n");
                exit(EXIT_FAILURE);
            }
        }
        perror("Nao esta aberto\n");
        exit(EXIT_FAILURE);
    }
    return -1;
}

int writeFile(int fd, char* buffer, int len, fileNode files[], int clientID) {
    inode_t* openNode = NULL;
    for (int i = 0; i < 5; i++) {
        if (files[i].fd == fd) {
            if ((openNode->owner == clientID && (openNode->ownerPermissions == 2 || openNode->ownerPermissions == 3)) || (openNode->owner != clientID && (openNode->othersPermissions == 2 || openNode->othersPermissions == 3))) {
                if (inode_set(files[i].iNumber, buffer, len) == -1) {
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

int createFile(char* filename, int ownerPermissions, int othersPermissions, int clientID) {
    if (lookup(fs, filename, numberBuckets)) {
        perror("ficheiro ja existe eheh\n");
        exit(EXIT_FAILURE);
    }
    int iNumber = inode_create(clientID, (permission) ownerPermissions, (permission) othersPermissions);
    if (iNumber == -1) {
        perror("erro ao criar o inode\n");
        exit(EXIT_FAILURE);
    }
    LOCK;
    create(fs, filename, iNumber, numberBuckets);
    UNLOCK;
    return 0; 
}

int deleteFile(char* filename, fileNode files[], int clientID) {
    LOCK;
    int iNumber = lookup(fs, filename, numberBuckets);
    UNLOCK;
    inode_t* openNode;
    if (!iNumber) {
        perror("ficheiro nao existe\n");
        exit(EXIT_FAILURE);
    }
    openNode = (inode_t*) malloc(sizeof(struct inode_t));
    if (inode_get(iNumber, &openNode->owner, &openNode->ownerPermissions, &openNode->othersPermissions, openNode->fileContent, strlen(openNode->fileContent)) == -1) {
        perror("Nao leu\n");
        exit(EXIT_FAILURE);
    }
    if (openNode->owner != clientID) {
        perror("Quem quer apagar nao e o dono\n");
        exit(EXIT_FAILURE);
    }
    LOCK;
    delete(fs, filename, numberBuckets);
    UNLOCK;
    if (inode_delete(iNumber) == -1) {
        perror("Servidor erro no delete\n");
        exit(EXIT_FAILURE);
    }
    return 0;
}

void* treatClient(args* Args) {
    int numTokens, fd, len, mode, ownerPermissions, othersPermissions;
    int acceptedSocket = Args->acceptedSocket;
    fileNode* files = (fileNode*) malloc(sizeof(struct fileNode) * MAX_FILES);
    files = Args->files;
    int clientID = Args->userID;
    char token, *filename = NULL, *newFilename = NULL, *sendBuffer = NULL;
    char* buffer = (char*) malloc(sizeof(char) * (MAX_BUFFER_SZ + 1));
    read(acceptedSocket, buffer, MAX_BUFFER_SZ + 1);
    printf("ehe\n");
    printf("%s\n", buffer);
    token = buffer[0];
    switch (token) {
        case 'c':
            numTokens = sscanf(buffer, "%c %s %d%d", &token, filename, &ownerPermissions, &othersPermissions);
            if (numTokens != 4) {
                perror("erro no comando\n");
                exit(EXIT_FAILURE);
            }
            createFile(filename, ownerPermissions, othersPermissions, clientID);
            break; 
        case 'd':
            numTokens = sscanf(buffer, "%c %s", &token, filename);
            if (numTokens != 2) {
                perror("erro no comando\n");
                exit(EXIT_FAILURE);
            }
            deleteFile(filename, files, clientID);
            break;
        case 'r':
            numTokens = sscanf(buffer, "%c %s %s", &token, filename, newFilename);
            if (numTokens != 3) {
                perror("erro no comando\n");
                exit(EXIT_FAILURE);
            }
            LOCK;
            renameFile(filename, newFilename);
            UNLOCK;
            break;
        case 'o':
            numTokens = sscanf(buffer, "%c %s %d", &token, filename, &mode);
            if (numTokens != 3) {
                perror("erro no comando");
                exit(EXIT_FAILURE);
            }
            openFile(filename, mode, files, clientID);
            break;
        case 'x':
            numTokens = sscanf(buffer, "%c %d", &token, &fd);
            if (numTokens != 2) {
                perror("erro no comando\n");
                exit(EXIT_FAILURE);
            }
            closeFile(fd, files);
            break;
        case 'l':
            numTokens = sscanf(buffer, "%c %d %d", &token, &fd, &len);
            if (numTokens != 3) {
                perror("erro no comando\n");
                exit(EXIT_FAILURE);
            }
            readFile(fd, buffer, len, files, clientID);
            break;
        case 'w':
            numTokens = sscanf(buffer, "%c %d %s", &token, &fd, sendBuffer);
            if (numTokens != 3) {
                perror("Erro no comando\n");
                exit(EXIT_FAILURE);
            }
            writeFile(fd, buffer, len, files, clientID);
            break;
        default:
            perror("Erro default\n");
            exit(EXIT_FAILURE);
            break;
    }
    return NULL;
}

void treatConnection() {
    int newServerSocket, clilen;
    unsigned int len;
    struct sockaddr_un cli_addr;
    struct ucred ucred;
    pthread_t tid[MAX_THREADS];
    len = sizeof(struct ucred);
    for (int counter = 0; counter < 10; counter++) {
        clilen = sizeof(cli_addr);
        newServerSocket = accept(socketServer, (struct sockaddr*) &cli_addr, (socklen_t*) &clilen);
        if (newServerSocket < 0) {
            perror("Servidor nao aceitou\n");
            exit(EXIT_FAILURE);
        }
        if (getsockopt(newServerSocket, SOL_SOCKET, SO_PEERCRED, &ucred, &len) == -1) {
            perror("Erro no getsockopt\n");
            exit(EXIT_FAILURE);
        }
        fileNode files[5];
        args* Args = (args*) malloc(sizeof(struct arg_struct));
        Args->files = (fileNode*) malloc(sizeof(struct fileNode) * MAX_FILES);
        Args->acceptedSocket = newServerSocket;
        Args->files = files;
        Args->userID = ucred.uid;
        if (pthread_create(&tid[counter], NULL, (void*) treatClient, &Args)) {
            perror("Erro a criar a thread\n");
            exit(EXIT_FAILURE);
        }
        close(newServerSocket);
    }
    close(socketServer);
}

int main(int argc, char* argv[]) {
    struct timeval start, end;
    double seconds, micros;
    parseArgs(argc, argv);
    gettimeofday(&start, NULL);
    inode_table_init();
    fs = new_tecnicofs(numberBuckets);
    gettimeofday(&end, NULL);
    createSocket(socketName);
    while (1) {
        printf("entrou\n");
        treatConnection();
    }
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
