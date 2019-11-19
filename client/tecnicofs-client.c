#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>

int tfsMount(char* address) {
    int sockfd;
    struct sockaddr_un end_serv;
    if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
        perror("Erro ao criar socket clinte");
    bzero((char*) &end_serv, sizeof(end_serv));
    end_serv.sun_family = AF_UNIX;
    strcpy(end_serv.sun_path, "/tmp/socket.unix.stream");
    dim_serv = strlen(end_serv.sun_path) + sizeof(end_serv.sun_family);
    if (connect(sockfd, (struct sockaddr*) &end_serv, dim_serv) < 0)
        perror("Error ao fazer connect no client");
    return 0;
}

int tfsUnmout() {
    
