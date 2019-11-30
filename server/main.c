#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
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
int socketServer = 0;
int counter = 0;
int flag = 1;
pthread_rwlock_t locker;
pthread_t tid[MAX_THREADS];

typedef struct filenode {
    int fd;
    int iNumber;
    int openMode;
    inode_t* iNode;
} *fileNode;

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

args** Args;
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

int renameFile(char* oldName, char* newName, int clientID, int acceptedSocket) {
    int searchResult1, searchResult2, var;
    LOCK;
    searchResult1 = lookup(fs, oldName, numberBuckets);
    searchResult2 = lookup(fs, newName, numberBuckets);
    UNLOCK;
    if (searchResult1 == -1) {
        var = -5;
        write(acceptedSocket, &var, sizeof(var));
        return -1;
    }
    if (searchResult2 != -1) {
        var = -4;
        write(acceptedSocket, &var, sizeof(var));
        return -1;
    }
    inode_t* openNode = (inode_t*) malloc(sizeof(struct inode_t));
    openNode->fileContent = (char*) malloc(sizeof(char) * MAX_BUFFER_SZ);
    if (inode_get(searchResult1, &openNode->owner, &openNode->ownerPermissions, &openNode->othersPermissions, openNode->fileContent, strlen(openNode->fileContent)) == -1) {
        var = -5;
        free(openNode->fileContent);
        free(openNode);
        write(acceptedSocket, &var, sizeof(var));
        return -1;
    }
    if (openNode->owner != clientID) {
        var = -6;
        free(openNode->fileContent);
        free(openNode);
        write(acceptedSocket, &var, sizeof(var));
        return -1;
    }
    LOCK;
    delete(fs, oldName, numberBuckets);
    create(fs, newName, searchResult1, numberBuckets);
    UNLOCK;
    var = 0;
    free(openNode->fileContent);
    free(openNode);
    write(acceptedSocket, &var, sizeof(var));
    return 0;
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

int openFile(char* filename, int mode, fileNode* files, int clientID, int acceptedSocket) {
    int fd, counter = 0, var, i, iNumber = lookup(fs, filename, numberBuckets);
    if (mode < 0 || mode > 4) {
        var = -10;
        write(acceptedSocket, &var, sizeof(var));
        return -1;
    }
    for (i = 0; i < MAX_FILES; i++) {
        if (files[i] == NULL) {
            fd = i;
            break;
        }
    }
    for (i = 0; i < MAX_FILES; i++) {
        if (files[i] != NULL) {
            counter++;
            if (files[i]->iNumber == iNumber) {
                var = -9;
                write(acceptedSocket, &var, sizeof(var));
                return -1;
            }
        }
    }
    if ((i == MAX_FILES) && (counter == MAX_FILES)) {
        var = -7;
        write(acceptedSocket, &var, sizeof(var));
        return -1;
    }
    iNumber = lookup(fs, filename, numberBuckets); 
    inode_t* openNode = (inode_t*) malloc(sizeof(struct inode_t));
    openNode->fileContent = (char*) malloc(sizeof(char) * MAX_BUFFER_SZ);
    if (inode_get(iNumber, &openNode->owner, &openNode->ownerPermissions, &openNode->othersPermissions, openNode->fileContent, strlen(openNode->fileContent)) == -1) {
        var = -5;
        free(openNode->fileContent);
        free(openNode);
        write(acceptedSocket, &var, sizeof(var));
        return -1;
    }
    if ((mode == 2 && openNode->ownerPermissions == 1 && openNode->owner == clientID)
        || (mode == 1 && openNode->ownerPermissions == 2 && openNode->owner == clientID)
        || (mode == 2 && openNode->othersPermissions == 1 && openNode->owner != clientID)
        || (mode == 1 && openNode->othersPermissions == 2 && openNode->owner != clientID)) {
        var = -10;
        write(acceptedSocket, &var, sizeof(fd));
        return -1;
    }
    files[fd] = (fileNode) malloc(sizeof(struct filenode));
    files[fd]->openMode = mode;
    files[fd]->iNumber = iNumber; 
    files[fd]->iNode = openNode;
    var = 0;
    write(acceptedSocket, &var, sizeof(fd));
    return var;
}

int closeFile(int fd, fileNode* files, int acceptedSocket) {
    int var;
    if (files[fd]->iNode == NULL) {
        var = -8;
        write(acceptedSocket, &var, sizeof(var));
        return -1;
    }
    files[fd] = NULL;
    var = 0;
    write(acceptedSocket, &var, sizeof(var));
    return 0;
}

int readFile(int fd, int len, fileNode* files, int clientID, int acceptedSocket) {
    char* buffer;
    int var;
    if (files[fd] == NULL) {
        var = -8;
        write(acceptedSocket, &var, sizeof(var));
        return -1;
    }
    if ((files[fd]->iNode->owner == clientID && files[fd]->iNode->ownerPermissions == 1) || (files[fd]->iNode->owner != clientID && files[fd]->iNode->othersPermissions == 1)) {
        var = -6;
        write(acceptedSocket, &var, sizeof(var));
        return -1;
    }
    if (files[fd]->openMode == 0 || files[fd]->openMode == 1) {
        var = -10;
        write(acceptedSocket, &var, sizeof(var));
        return -1;
    }
    buffer = (char*) malloc(sizeof(char) * MAX_BUFFER_SZ);
    strncpy(buffer, files[fd]->iNode->fileContent, len - 1);
    strcat(buffer, "\0");
    var = strlen(buffer);
    write(acceptedSocket, &var, sizeof(var));
    write(acceptedSocket, buffer, strlen(buffer));
    return var;
}

int writeFile(int fd, char* buffer, int len, fileNode* files, int clientID, int acceptedSocket) {
    int var;
    if (files[fd] == NULL) {
        var = -8;
        write(acceptedSocket, &var, sizeof(var));
        return -1;
    }
    if ((files[fd]->iNode->owner == clientID && files[fd]->iNode->ownerPermissions == 2) || (files[fd]->iNode->owner != clientID && files[fd]->iNode->othersPermissions == 2)) {
        var = -6;
        write(acceptedSocket, &var, sizeof(var));
        return -1;
    }
    if (files[fd]->openMode == 0 || files[fd]->openMode == 2) {
        var = -10;
        write(acceptedSocket, &var, sizeof(var));
        return -1;
    }
    var = 0;
    strcpy(files[fd]->iNode->fileContent, buffer);
    write(acceptedSocket, &var, sizeof(var));
    return strlen(files[fd]->iNode->fileContent);
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

int deleteFile(char* filename, fileNode* files, int clientID, int acceptedSocket) {
    int var, iNumber;
    LOCK;
    iNumber = lookup(fs, filename, numberBuckets);
    UNLOCK;
    if (iNumber == -1) {
        var = -5;
        write(acceptedSocket, &var, sizeof(var));
        return -1;
    }
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i] != NULL && files[i]->iNumber == iNumber) {
            var = -9;
            write(acceptedSocket, &var, sizeof(var));
            return -1;
        }
    }
    inode_t* openNode = (inode_t*) malloc(sizeof(struct inode_t));
    openNode->fileContent = (char*) malloc(sizeof(char) * MAX_BUFFER_SZ);
    if (inode_get(iNumber, &openNode->owner, &openNode->ownerPermissions, &openNode->othersPermissions, openNode->fileContent, strlen(openNode->fileContent)) == -1) {
        var = -5;
        free(openNode->fileContent);
        free(openNode);
        write(acceptedSocket, &var, sizeof(var));
        return -1;
    }
    if (openNode->owner == clientID) {
        LOCK;
        delete(fs, filename, numberBuckets);
        UNLOCK;
        if (inode_delete(iNumber) == -1) {
            var = -11;
            free(openNode->fileContent);
            free(openNode);
            write(acceptedSocket, &var, sizeof(var));
            return -1;
        }
        var = 0;
        free(openNode->fileContent);
        free(openNode);
        write(acceptedSocket, &var, sizeof(var));
        return 0;
    }
    return -1;
}

void endProgram() {
    printf("counter: %d\n", counter);
    for (int i = 0; i < counter; i++) {
        if (pthread_join(tid[i], NULL)  != 0)
            perror("Can't join threads\n");
        close(Args[i]->acceptedSocket);
        free(Args[i]->files);
        free(Args[i]);
    }
    free(Args);
    close(socketServer);
    printf("ja esta a ficar bom\n"); 
    return;

}

void treatSignal(int signum) {
    printf("inicio do treatSignal\n");
    if (signal(SIGINT, treatSignal) == SIG_ERR)
        perror("Couldn't install signal\n");
    printf("dentro do signal\n");
    flag = 0;
    endProgram();
}

void* treatClient(void* a) {
    int numTokens, fd, len, mode, ownerPermissions, othersPermissions;
    sigset_t set;
    int s;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    s = pthread_sigmask(SIG_BLOCK, &set, NULL);
    if (s != 0)
        perror("Coundn't create sigmask\n");
    args* Args = (args*) a;
    int acceptedSocket = Args->acceptedSocket;
    fileNode* files = Args->files;
    int clientID = Args->userID;
    char token;
    char* buffer = NULL;
    char* filename = (char*) malloc(sizeof(char) * (MAX_BUFFER_SZ + 1));
    char* newFilename = (char*) malloc(sizeof(char) * (MAX_BUFFER_SZ + 1));
    char* sendBuffer = (char*) malloc(sizeof(char) * (MAX_BUFFER_SZ + 1));
    while (1) {
        free(buffer);
        buffer = (char*) malloc(sizeof(char) * (MAX_BUFFER_SZ + 1));
        for (;;) {
            if (read(acceptedSocket, buffer, MAX_BUFFER_SZ + 1) != 0)
                break;
            return NULL;
        }
        token = buffer[0];
        switch (token) {
            case 'c':
                numTokens = sscanf(buffer, "%c %s %1d%1d", &token, filename, &ownerPermissions, &othersPermissions);
                if (numTokens != 4) {
                    perror("Erro no comando c\n");
                    return NULL;
                }
                createFile(filename, ownerPermissions, othersPermissions, clientID, acceptedSocket);
                continue; 
            case 'd':
                numTokens = sscanf(buffer, "%c %s", &token, filename);
                if (numTokens != 2) {
                    perror("Erro no comando d\n");
                    return NULL;
                }
                deleteFile(filename, files, clientID, acceptedSocket);
                continue;
            case 'r':
                numTokens = sscanf(buffer, "%c %s %s", &token, filename, newFilename);
                if (numTokens != 3) {
                    perror("Erro no comando r\n");
                    return NULL;
                }
                renameFile(filename, newFilename, clientID, acceptedSocket);
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
                closeFile(fd, files, acceptedSocket);
                continue;
            case 'l':
                numTokens = sscanf(buffer, "%c %d %d", &token, &fd, &len);
                if (numTokens != 3) {
                    perror("erro no comando\n");
                    return NULL;
                }
                readFile(fd, len, files, clientID, acceptedSocket);
                continue;
            case 'w':
                numTokens = sscanf(buffer, "%c %d %s", &token, &fd, sendBuffer);
                if (numTokens != 3) {
                    perror("Erro no comando\n");
                    return NULL;
                }
                writeFile(fd, sendBuffer, len, files, clientID, acceptedSocket);
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
    struct sockaddr_un end_cli;
    unsigned int len, dim_cli;
    struct ucred ucred;
    Args = (args**) malloc(sizeof(struct arg_struct) * MAX_THREADS);
    dim_cli = sizeof(end_cli);
    len = sizeof(struct ucred);
    while (counter < MAX_THREADS) {
        printf("comecou o while com flag: %d\n", flag);
        newServerSocket = accept(socketServer, (struct sockaddr*) &end_cli, &dim_cli);
        if (!flag)
            return;
       if (newServerSocket < 0) {
            perror("Servidor nao aceitou\n");
            return;
        }
        if (getsockopt(newServerSocket, SOL_SOCKET, SO_PEERCRED, &ucred, &len) == -1) {
            perror("Erro no getsockopt\n");
            exit(EXIT_FAILURE);
        }
        fileNode* files = (fileNode*) malloc(sizeof(struct filenode) * MAX_FILES);
        Args[counter] = (args*) malloc(sizeof(struct arg_struct));
        Args[counter]->acceptedSocket = newServerSocket;
        Args[counter]->files = files;
        Args[counter]->userID = ucred.uid;
        if (pthread_create(&tid[counter], NULL, treatClient, Args[counter])) {
            perror("Erro a criar a thread\n");
            exit(EXIT_FAILURE);
        }
        counter++;
        if (signal(SIGINT, treatSignal) == SIG_ERR) {
            perror("Couldn't install signal handler\n");
            exit(-1);
        }
    }
    return;
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
    treatConnection();
    printf("no main\n");
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
