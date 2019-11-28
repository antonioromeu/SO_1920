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
#include "../client/tecnicofs-api-constants.h"

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

int openFile(char* filename, int mode, fileNode files[], int clientID, int acceptedSocket) {
    int fd, counter = 0, var, i = 0, iNumber = lookup(fs, filename, numberBuckets);
    printf("open inicio\n");
    inode_t* openNode;
    if (mode < 0 || mode > 4) {
        var = -10;
        printf("1\n");
        write(acceptedSocket, &var, sizeof(var));
        return -1;
    }
    while (i < MAX_FILES) {
        if (files[i].fd == -1) {
            fd = i;
            break;
        }
        i++;
    }
    for (i = 0; i < MAX_FILES; i++) {
        if (files[i].fd != -1)
            counter++;
        if (files[i].iNumber == iNumber) {
            var = -9;
            printf("2\n");
            write(acceptedSocket, &var, sizeof(var));
            return -1;
        }
        i++;
    }
    if ((i == MAX_FILES) && (counter == MAX_FILES)) {
        var = -7;
        printf("3\n");
        write(acceptedSocket, &var, sizeof(var));
        return -1;
    }
    openNode = (inode_t*) malloc(sizeof(struct inode_t));
    openNode->fileContent = (char *) malloc(sizeof(char) * MAX_BUFFER_SZ);
    if (inode_get(iNumber, &openNode->owner, &openNode->ownerPermissions, &openNode->othersPermissions, openNode->fileContent, strlen(openNode->fileContent)) == -1) {
        var = -5;
        printf("4\n");
        write(acceptedSocket, &var, sizeof(var));
        return -1;
    }
    if ((openNode->owner == clientID && openNode->ownerPermissions == mode) || (openNode->owner != clientID && openNode->othersPermissions == mode)) {
        files[fd].fd = fd;
        files[fd].iNumber = iNumber;
        printf("no open: %d\n", fd);
        var = 0;
        write(acceptedSocket, &var, sizeof(fd));
        return var;
    }
    var = -6;
    printf("ultimo\n");
    write(acceptedSocket, &var, sizeof(var));
    return -1;
}

int closeFile(int fd, fileNode files[], int acceptedSocket) {
    int var;
    printf("passou no close\n");
    if (files[fd].fd == -1) {
        printf("close: %d\n", fd);
        var = -8;
        write(acceptedSocket, &var, sizeof(var));
        return -1;
    }
    printf("dentro do close\n");
    files[fd].fd = -1;
    var = 0;
    write(acceptedSocket, &var, sizeof(var));
    return 0;
}

int readFile(int fd, char* buffer, int len, fileNode files[], int clientID) {
    inode_t* openNode = NULL;
    for (int i = 0; i < 5; i++) {
        if (files[i].fd == fd) {
            openNode = (inode_t*) malloc(sizeof(struct inode_t));
            if (inode_get(files[i].iNumber, &openNode->owner, &openNode->ownerPermissions, &openNode->othersPermissions, openNode->fileContent, strlen(openNode->fileContent)) == -1) {
                perror("Nao leu\n");
                return -1;
            }
            if (strlen(openNode->fileContent) > len - 1) {
                perror("Grande demais\n");
                return -1;
            }
            if ((openNode->owner == clientID && (openNode->ownerPermissions == 1 || openNode->ownerPermissions == 3)) || (openNode->owner != clientID && (openNode->othersPermissions == 1 || openNode->othersPermissions == 3))) {
                strcpy(buffer, openNode->fileContent);
                return (strlen(buffer) - 1);
            }
            else {
                perror("Nao pode ser lido\n");
                return -1;
            }
        }
        perror("Nao esta aberto\n");
        return -1;
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
                    return -1;
                }
                return len - 1;
            }
            else {
                perror("Nao pode ser lido eheh\n");
                return -1;
            }
        }
    }
    perror("Nao esta aberto eheh");
    return -1;
}

int createFile(char* filename, int ownerPermissions, int othersPermissions, int clientID, int acceptedSocket) {
    int var;
    if (lookup(fs, filename, numberBuckets) != -1) {
        var = -4;
        write(acceptedSocket, &var, sizeof(var));
        return -1;
    }
    int iNumber = inode_create(clientID, (permission) ownerPermissions, (permission) othersPermissions);
    if (iNumber == -1) {
        var = -11;
        write(acceptedSocket, &var, sizeof(var));
        return -1;
    }
    LOCK;
    create(fs, filename, iNumber, numberBuckets);
    UNLOCK;
    var = 0;
    write(acceptedSocket, &var, sizeof(var));
    return 0; 
}

int deleteFile(char* filename, fileNode files[], int clientID, int acceptedSocket) {
    int var, iNumber;
    inode_t* openNode;
    LOCK;
    iNumber = lookup(fs, filename, numberBuckets);
    UNLOCK;
    printf("inumber%d\n", iNumber);
    printf("filename: %s\n", filename);
    if (iNumber == -1) {
        printf("%d\n", iNumber);
        var = -5;
        write(acceptedSocket, &var, sizeof(var));
        return -1;
    }
    openNode = (inode_t*) malloc(sizeof(struct inode_t));
    openNode->fileContent = (char*) malloc(sizeof(char) * MAX_BUFFER_SZ);
    if (inode_get(iNumber, &openNode->owner, &openNode->ownerPermissions, &openNode->othersPermissions, openNode->fileContent, strlen(openNode->fileContent)) == -1) {
        printf("AQUI\n");
        var = -5;
        write(acceptedSocket, &var, sizeof(var));
    
        return -1;
    }
    printf("adeus\n");
    if (openNode->owner != clientID) {
        var = -6;
        write(acceptedSocket, &var, sizeof(var));
        return -1;
    }
    
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].iNumber == iNumber && files[i].fd != -1) {
            var = -9;
            printf("inumber do delete: %d\n", files[i].iNumber);
            printf("fd: %d\n", files[i].fd);
            write(acceptedSocket, &var, sizeof(var));
            printf("%d\n", var);
            return -1;
            //return 20;
        }
    }
    LOCK;
    delete(fs, filename, numberBuckets);
    UNLOCK;
    if (inode_delete(iNumber) == -1) {
        var = -11;
        write(acceptedSocket, &var, sizeof(var));
        return -1;
    }
    var = 0;
    write(acceptedSocket, &var, sizeof(var));
    return 0;
}

void* treatClient(void* a) {
    int numTokens, fd, len, mode, ownerPermissions, othersPermissions;
    args *Args = (args *)a;
    int acceptedSocket = Args->acceptedSocket;
    fileNode* files = (fileNode*) malloc(sizeof(struct fileNode) * MAX_FILES);
    files = Args->files;
    int clientID = Args->userID;
    char token;
    char* buffer = (char*) malloc(sizeof(char) * (MAX_BUFFER_SZ + 1));
    char* filename = (char*) malloc(sizeof(char) * (MAX_BUFFER_SZ + 1));
    char* newFilename = (char*) malloc(sizeof(char) * (MAX_BUFFER_SZ + 1));
    char* sendBuffer = (char*) malloc(sizeof(char) * (MAX_BUFFER_SZ + 1));
    while (1) {
        printf("le\n");
        for (;;)
            if (read(acceptedSocket, buffer, MAX_BUFFER_SZ + 1) != 0)
                break;
        token = buffer[0];
        printf("%s\n", buffer);
        switch (token) {
            case 'c':
                numTokens = sscanf(buffer, "%c %s %1d%1d", &token, filename, &ownerPermissions, &othersPermissions);
                if (numTokens != 4) {
                    perror("Erro no comando c\n");
                    return NULL;
                }
                createFile(filename, ownerPermissions, othersPermissions, clientID, acceptedSocket);
                printf("acabou, ve se entendes\n");
                continue; 
            case 'd':
                numTokens = sscanf(buffer, "%c %s", &token, filename);
                if (numTokens != 2) {
                    perror("Erro no comando d\n");
                    return NULL;
                }
                deleteFile(filename, files, clientID, acceptedSocket);
                printf("acabou o delete\n");
                continue;
            case 'r':
                numTokens = sscanf(buffer, "%c %s %s", &token, filename, newFilename);
                if (numTokens != 3) {
                    perror("Erro no comando r\n");
                    return NULL;
                }
                LOCK;
                renameFile(filename, newFilename);
                UNLOCK;
                continue;
            case 'o':
                numTokens = sscanf(buffer, "%c %s %1d", &token, filename, &mode);
                if (numTokens != 3) {
                    perror("erro no comando");
                    return NULL;
                }
                openFile(filename, mode, files, clientID, acceptedSocket);
                continue;
            case 'x':
                numTokens = sscanf(buffer, "%c %d", &token, &fd);
                if (numTokens != 2) {
                    perror("erro no comando\n");
                    return NULL;
                }
                printf("comando: %d\n", fd);
                closeFile(fd, files, acceptedSocket);
                continue;
            case 'l':
                numTokens = sscanf(buffer, "%c %d %d", &token, &fd, &len);
                if (numTokens != 3) {
                    perror("erro no comando\n");
                    return NULL;
                }
                readFile(fd, buffer, len, files, clientID);
                continue;
            case 'w':
                numTokens = sscanf(buffer, "%c %d %s", &token, &fd, sendBuffer);
                if (numTokens != 3) {
                    perror("Erro no comando\n");
                    return NULL;
                }
                writeFile(fd, buffer, len, files, clientID);
                continue;
            default:
                perror("Erro default\n");
                return NULL;
                break;
        }
    }
    return NULL;
}

void treatConnection() {
    int newServerSocket;
    unsigned int len;
    struct ucred ucred;
    pthread_t tid[MAX_THREADS];
    len = sizeof(struct ucred);
    for (int counter = 0; counter < 10; counter++) {
        newServerSocket = accept(socketServer, NULL, NULL);
        if (newServerSocket < 0) {
            perror("Servidor nao aceitou\n");
            exit(EXIT_FAILURE);
        }
        if (getsockopt(newServerSocket, SOL_SOCKET, SO_PEERCRED, &ucred, &len) == -1) {
            perror("Erro no getsockopt\n");
            exit(EXIT_FAILURE);
        }
        fileNode files[5];
        for (int i = 0; i < MAX_FILES; i++) { 
            files[i].fd = -1;
            files[i].iNumber = -1;
        }
        args* Args = (args*) malloc(sizeof(struct arg_struct));
        Args->files = (fileNode*) malloc(sizeof(struct fileNode) * MAX_FILES);
        Args->acceptedSocket = newServerSocket;
        Args->files = files;
        Args->userID = ucred.uid;
        if (pthread_create(&tid[counter], NULL, treatClient, Args)) {
            perror("Erro a criar a thread\n");
            exit(EXIT_FAILURE);
        }
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
