#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
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
} * fileNode;

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
    printf("Usage: %s input_filepath output_filepath buckets_number\n", appName);
    exit(EXIT_FAILURE);
}

static void parseArgs(long argc, char* const argv[]) {
    if (argc != 4) {
        perror("Invalid format:\n");
        displayUsage(argv[0]);
    }
    socketName = argv[1];
    fileOutput = argv[2];
}

void createSocket(char* address) {
    int servlen;
    struct sockaddr_un serv_addr;
    if ((socketServer = socket(AF_UNIX,SOCK_STREAM,0) ) < 0) {
        perror("Server couldn't create socket\n");
        exit(EXIT_FAILURE);
    }
    unlink(address); 
    bzero((char*) &serv_addr, sizeof(serv_addr)); 
    serv_addr.sun_family = AF_UNIX; 
    strcpy(serv_addr.sun_path, address); 
    servlen = strlen(serv_addr.sun_path) + sizeof(serv_addr.sun_family); 
    if (bind(socketServer, (struct sockaddr*) &serv_addr, servlen) < 0) {
        perror("Server couldn't bind\n");
        exit(EXIT_FAILURE);
    }
    listen(socketServer, MAX_CONNECTIONS_WAITING);
}

int createFile(char* filename, int ownerPermissions, int othersPermissions, int clientID, int acceptedSocket) {
    int var;
    if (lookup(fs, filename, numberBuckets) != -1)
        var = TECNICOFS_ERROR_FILE_ALREADY_EXISTS;
	else { 
		int iNumber = inode_create(clientID, (permission) ownerPermissions, (permission) othersPermissions);
		if (iNumber == -1)
			var = TECNICOFS_ERROR_OTHER;
		else {
			LOCK;
			create(fs, filename, iNumber, numberBuckets);
			UNLOCK;
			var = 0;
		}
	}
    write(acceptedSocket, &var, sizeof(var));
    return var; 
}

int deleteFile(char* filename, fileNode* files, int clientID, int acceptedSocket) {
    int var, iNumber;
    LOCK;
    iNumber = lookup(fs, filename, numberBuckets);
    UNLOCK;
    inode_t* openNode = NULL;
	if (iNumber == -1)
        var = TECNICOFS_ERROR_FILE_NOT_FOUND;
    else {
		for (int i = 0; i < MAX_FILES; i++)
			if (files[i] && files[i]->iNumber == iNumber) {
				var = TECNICOFS_ERROR_FILE_IS_OPEN;
				write(acceptedSocket, &var, sizeof(var));
				return var;
			}
		openNode = (inode_t*) calloc(1, sizeof(struct inode_t));
		openNode->fileContent = (char*) calloc(MAX_BUFFER_SZ, sizeof(char));
		if (inode_get(iNumber, &openNode->owner, &openNode->ownerPermissions, &openNode->othersPermissions, openNode->fileContent, strlen(openNode->fileContent)) == -1)
			var = TECNICOFS_ERROR_FILE_NOT_FOUND;
		else if (openNode->owner == clientID) {
			if (inode_delete(iNumber) == -1)
				var = TECNICOFS_ERROR_OTHER;
			else {
				LOCK;
				delete(fs, filename, numberBuckets);
				UNLOCK;
				var = 0;
			}
		}
		free(openNode->fileContent);
		free(openNode);
	}
    write(acceptedSocket, &var, sizeof(var));
    return var;
}

int openFile(char* filename, int mode, fileNode* files, int clientID, int acceptedSocket) {
    int fd, counter = 0, var, i, iNumber = lookup(fs, filename, numberBuckets);
    inode_t* openNode = NULL;
	if (mode < 0 || mode > 4)
        var = TECNICOFS_ERROR_INVALID_MODE;
    else {
		for (i = 0; i < MAX_FILES; i++) {
			if (!files[i]) {
				fd = i;
				break;
			}
		}
		for (i = 0; i < MAX_FILES; i++) {
			if (files[i]) {
				counter++;
				if (files[i]->iNumber == iNumber)
					var = TECNICOFS_ERROR_FILE_IS_OPEN;
			}
		}
		if ((i == MAX_FILES) && (counter == MAX_FILES))
			var = TECNICOFS_ERROR_MAXED_OPEN_FILES;
		else {
			iNumber = lookup(fs, filename, numberBuckets); 
			openNode = (inode_t*) calloc(1, sizeof(struct inode_t));
			openNode->fileContent = (char*) calloc(MAX_BUFFER_SZ, sizeof(char));
			if (inode_get(iNumber, &openNode->owner, &openNode->ownerPermissions, &openNode->othersPermissions, openNode->fileContent, strlen(openNode->fileContent)) == -1)
				var = TECNICOFS_ERROR_FILE_NOT_FOUND;
			else if ((mode == 2 && openNode->ownerPermissions == 1 && openNode->owner == clientID) || (mode == 1 && openNode->ownerPermissions == 2 && openNode->owner == clientID) || (mode == 2 && openNode->othersPermissions == 1 && openNode->owner != clientID) || (mode == 1 && openNode->othersPermissions == 2 && openNode->owner != clientID))
				var = TECNICOFS_ERROR_INVALID_MODE;
			else {
				files[fd] = (fileNode) calloc(1, sizeof(struct filenode));
				files[fd]->openMode = mode;
				files[fd]->iNumber = iNumber; 
				var = 0;
			}
		}
	}
    write(acceptedSocket, &var, sizeof(fd));
    free(openNode->fileContent);
    free(openNode);
    return var;
}

int closeFile(int fd, fileNode* files, int acceptedSocket) {
    int var;
    if (files[fd] == NULL)
        var = TECNICOFS_ERROR_FILE_NOT_OPEN;
    else {
        free(files[fd]);
		files[fd] = NULL;
        var = 0;
    }
    write(acceptedSocket, &var, sizeof(var));
    return var;
}

int readFile(int fd, int len, fileNode* files, int clientID, int acceptedSocket) {
    char* buffer = NULL;
    inode_t* openNode = NULL;
	int var;
    if (files[fd] == NULL)
        var = TECNICOFS_ERROR_FILE_NOT_OPEN;
    else {
		openNode = (inode_t*) calloc(1, sizeof(struct inode_t));
		openNode->fileContent = (char*) calloc(MAX_BUFFER_SZ, sizeof(char));
		if (inode_get(files[fd]->iNumber, &openNode->owner, &openNode->ownerPermissions, &openNode->othersPermissions, openNode->fileContent, len) == -1)
			var = TECNICOFS_ERROR_FILE_NOT_FOUND;
		else if ((openNode->owner == clientID && openNode->ownerPermissions == 1) || (openNode->owner != clientID && openNode->othersPermissions == 1))
			var = TECNICOFS_ERROR_PERMISSION_DENIED;
		else if (files[fd]->openMode == 0 || files[fd]->openMode == 1)
			var = TECNICOFS_ERROR_INVALID_MODE;
		else {
			buffer = (char*) calloc(MAX_BUFFER_SZ, sizeof(char));
			strncpy(buffer, openNode->fileContent, len - 1);
			strcat(buffer, "\0");
			var = strlen(buffer);
			write(acceptedSocket, &var, sizeof(var));
			write(acceptedSocket, buffer, strlen(buffer));
    		free(openNode->fileContent);
			free(openNode);
			free(buffer);
			return var;
		}
		free(openNode->fileContent);
		free(openNode);
	}
    write(acceptedSocket, &var, sizeof(var));
    return var;
}

int renameFile(char* oldName, char* newName, int clientID, int acceptedSocket) {
    int searchResult1, searchResult2, var;
    inode_t* openNode = NULL;
	LOCK;
    searchResult1 = lookup(fs, oldName, numberBuckets);
    searchResult2 = lookup(fs, newName, numberBuckets);
    UNLOCK;
    if (searchResult1 == -1)
        var = TECNICOFS_ERROR_FILE_NOT_FOUND;
    else if (searchResult2 != -1)
        var = TECNICOFS_ERROR_FILE_ALREADY_EXISTS;
    else {
		openNode = (inode_t*) calloc(1, sizeof(struct inode_t));
		openNode->fileContent = (char*) calloc(MAX_BUFFER_SZ, sizeof(char));
		if (inode_get(searchResult1, &openNode->owner, &openNode->ownerPermissions, &openNode->othersPermissions, openNode->fileContent, strlen(openNode->fileContent)) == -1)
			var = TECNICOFS_ERROR_FILE_NOT_FOUND;
		else if (openNode->owner != clientID)
			var = TECNICOFS_ERROR_PERMISSION_DENIED;
		else {
			LOCK;
			delete(fs, oldName, numberBuckets);
			create(fs, newName, searchResult1, numberBuckets);
			UNLOCK;
			var = 0;
		}
	}
    free(openNode->fileContent);
    free(openNode);
    write(acceptedSocket, &var, sizeof(var));
    return var;
}

int writeFile(int fd, char* buffer, int len, fileNode* files, int clientID, int acceptedSocket) {
    int var;
    inode_t* openNode = NULL;
	if (files[fd] == NULL)
        var = TECNICOFS_ERROR_FILE_NOT_OPEN;
	else {
		openNode = (inode_t*) calloc(1, sizeof(struct inode_t));
		openNode->fileContent = (char*) calloc(MAX_BUFFER_SZ, sizeof(char));
		if (inode_get(files[fd]->iNumber, &openNode->owner, &openNode->ownerPermissions, &openNode->othersPermissions, openNode->fileContent, strlen(openNode->fileContent)) == -1)
			var = TECNICOFS_ERROR_FILE_NOT_FOUND;
		else if ((openNode->owner == clientID && openNode->ownerPermissions == 2) || (openNode->owner != clientID && openNode->othersPermissions == 2))
			var = TECNICOFS_ERROR_PERMISSION_DENIED;
		else if (files[fd]->openMode == 0 || files[fd]->openMode == 2)
			var = TECNICOFS_ERROR_INVALID_MODE;
		else {
			var = 0;
			inode_set(files[fd]->iNumber, buffer, len);
		}
	}
    write(acceptedSocket, &var, sizeof(var));
    free(openNode->fileContent);
    free(openNode);
    return var;
}

void endProgram() {
    for (int i = 0; i < counter; i++) {
        if (pthread_join(tid[i], NULL)  != 0)
            perror("Couldn't join threads\n");
        close(Args[i]->acceptedSocket);
        free(Args[i]->files);
		free(Args[i]);
    }
    free(Args);
    close(socketServer);
    return;
}

void treatSignal(int signum) {
    if (signal(SIGINT, treatSignal) == SIG_ERR)
        perror("Couldn't install signal\n");
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
    char* filename = (char*) calloc(MAX_BUFFER_SZ, sizeof(char));
    char* newFilename = (char*) calloc(MAX_BUFFER_SZ, sizeof(char));
    char* sendBuffer = (char*) calloc(MAX_BUFFER_SZ, sizeof(char));
    while (1) {
        free(buffer);
        buffer = (char*) calloc(MAX_BUFFER_SZ, sizeof(char));
        for (;;) {
            if (read(acceptedSocket, buffer, MAX_BUFFER_SZ) != 0)
                break;
            free(buffer);
            free(filename);
            free(newFilename);
            free(sendBuffer);
            return NULL;
        }
        token = buffer[0];
        switch (token) {
            case 'c':
                numTokens = sscanf(buffer, "%c %s %1d%1d", &token, filename, &ownerPermissions, &othersPermissions);
                if (numTokens != 4) {
                    perror("Error on command c\n");
                    return NULL;
                }
                createFile(filename, ownerPermissions, othersPermissions, clientID, acceptedSocket);
                continue; 
            case 'd':
                numTokens = sscanf(buffer, "%c %s", &token, filename);
                if (numTokens != 2) {
                    perror("Error on command d\n");
                    return NULL;
                }
                deleteFile(filename, files, clientID, acceptedSocket);
                continue;
            case 'r':
                numTokens = sscanf(buffer, "%c %s %s", &token, filename, newFilename);
                if (numTokens != 3) {
                    perror("Error on command r\n");
                    return NULL;
                }
                renameFile(filename, newFilename, clientID, acceptedSocket);
                continue;
            case 'o':
                numTokens = sscanf(buffer, "%c %s %1d", &token, filename, &mode);
                if (numTokens != 3) {
                    perror("Error on command o\n");
                    return NULL;
                }
                openFile(filename, mode, files, clientID, acceptedSocket);
                continue;
            case 'x':
                numTokens = sscanf(buffer, "%c %d", &token, &fd);
                if (numTokens != 2) {
                    perror("Error on command x\n");
                    return NULL;
                }
                closeFile(fd, files, acceptedSocket);
                continue;
            case 'l':
                numTokens = sscanf(buffer, "%c %d %d", &token, &fd, &len);
                if (numTokens != 3) {
                    perror("Error on command l\n");
                    return NULL;
                }
                readFile(fd, len, files, clientID, acceptedSocket);
                continue;
            case 'w':
                numTokens = sscanf(buffer, "%c %d %s", &token, &fd, sendBuffer);
                if (numTokens != 3) {
                    perror("Error on command w\n");
                    return NULL;
                }
                writeFile(fd, sendBuffer, strlen(sendBuffer), files, clientID, acceptedSocket);
                continue;
            default:
                perror("Default error\n");
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
    Args = (args**) calloc(MAX_THREADS, sizeof(struct arg_struct));
    dim_cli = sizeof(end_cli);
    len = sizeof(struct ucred);
    while (counter < MAX_THREADS) {
        newServerSocket = accept(socketServer, (struct sockaddr*) &end_cli, &dim_cli);
        if (!flag)
            return;
		if (newServerSocket < 0) {
            perror("Server couldn't accept client\n");
            return;
        }
        if (getsockopt(newServerSocket, SOL_SOCKET, SO_PEERCRED, &ucred, &len) == -1) {
            perror("Error on function getsockopt\n");
            exit(EXIT_FAILURE);
        }
        fileNode* files = (fileNode*) calloc(MAX_FILES, sizeof(struct filenode));
        Args[counter] = (args*) calloc(1, sizeof(struct arg_struct));
        Args[counter]->acceptedSocket = newServerSocket;
        Args[counter]->files = files;
        Args[counter]->userID = ucred.uid;
        if (pthread_create(&tid[counter], NULL, treatClient, Args[counter])) {
            perror("Couldn't create thread\n");
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
    createSocket(socketName);
    treatConnection();
    FILE* fptr = fopen(fileOutput, "w");
    print_tecnicofs_tree(fptr, fs, numberBuckets);
    free_tecnicofs(fs, numberBuckets);
    inode_table_destroy();
    gettimeofday(&end, NULL);
    seconds = (double) (end.tv_sec - start.tv_sec);
    micros = (double) ((seconds + (double) (end.tv_usec - start.tv_usec)/1000000));
    fprintf(fptr, "TecnicoFS completed in %.4f seconds.\n", micros);
    fclose(fptr);
    exit(EXIT_SUCCESS);
}
