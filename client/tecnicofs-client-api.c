#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include "tecnicofs-client-api.h"
#include "tecnicofs-api-constants.h"

#define MAX_BUFFER 100

int clientSocket = -1;

int tfsMount(char* address) {
    int dim_serv;
    struct sockaddr_un end_serv;
    if ((clientSocket = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        perror("Erro ao criar clientSocket clinte\n");
        exit(EXIT_FAILURE);
    }
    bzero((char*) &end_serv, sizeof(end_serv));
    end_serv.sun_family = AF_UNIX;
    strcpy(end_serv.sun_path, address);
    dim_serv = sizeof(end_serv);
    if (connect(clientSocket, (struct sockaddr*) &end_serv, dim_serv) < 0) {
        perror("Error ao fazer connect no client\n");
        exit(EXIT_FAILURE);
    }
    return 0;
}

int tfsCreate(char* filename, permission ownerPermissions, permission othersPermissions) {
    char* buffer = (char*) malloc(sizeof(char) * MAX_BUFFER);
    sprintf(buffer, "c %s %d%d", filename, ownerPermissions, othersPermissions);
    if (write(clientSocket, buffer, strlen(buffer)) != strlen(buffer))
        perror("Error cliente no write/create\n");
    read(clientSocket, buffer, strlen(buffer));
    if (!strcmp(buffer, "SUCESS")) {
        free(buffer);
        return 0;
    }
    else {
        free(buffer);
        return -4;
    }
}

int tfsDelete(char *filename) {
    char* buffer = (char*) malloc(sizeof(char) * MAX_BUFFER);
    sprintf(buffer,"d %s", filename);
    if (write(clientSocket, buffer, strlen(buffer)) != strlen(buffer))
        perror("Error cliente no write/delete\n");
    free(buffer);
    return 0;
}

int tfsRename(char *filename, char *newFilename) {
    char* buffer = (char*) malloc(sizeof(char) * MAX_BUFFER);
    sprintf(buffer, "r %s %s", filename, newFilename);
    if (write(clientSocket, buffer, strlen(buffer)) != strlen(buffer)) 
        perror("Error cliente no write/rename\n");
    free(buffer);
    return 0;
}

int tfsOpen(char *filename, permission mode) {
    char* buffer = (char*) malloc(sizeof(char) * MAX_BUFFER);
    sprintf(buffer, "o %s %d", filename, mode);
    if (write(clientSocket, buffer, strlen(buffer)) != strlen(buffer)) 
        perror("Error cliente no write/open\n");
    free(buffer);
    return 0;
}

int tfsClose(int fd) { 
    char* buffer = (char*) malloc(sizeof(char) * MAX_BUFFER);
    sprintf(buffer, "x %d", fd);
    if (write(clientSocket, buffer, strlen(buffer)) != strlen(buffer)) 
        perror("Error cliente no write/close\n");
    free(buffer);
    return 0;
}

int tfsRead(int fd, char* receiveBuffer, int len) {
    char* buffer = (char*) malloc(sizeof(char) * MAX_BUFFER);
    sprintf(buffer, "l %d %d", fd, len);
    if (write(clientSocket, buffer, strlen(buffer)) != strlen(buffer)) 
        perror("Error cliente no write/read\n");
    free(buffer);
    return 0;
}

int tfsWrite(int fd, char* sendBuffer, int len) { 
    char* buffer = (char*) malloc(sizeof(char) * MAX_BUFFER);
    sprintf(buffer, "w %d %s", fd, sendBuffer);
    if (write(clientSocket, buffer, strlen(buffer)) != strlen(buffer))
        perror("Error cliente no write/write");
    free(buffer);
    return 0;
}

int tfsUnmount() {
    if (clientSocket < 0)
        perror("Socket nao foi criado\n");
    if (close(clientSocket))
        perror("Error no unmout\n");
    return 0;
}
