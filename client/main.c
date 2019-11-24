#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlin.h>
#include <string.h>
#include "unix.h"

int main(void) {
    int sockfd, servlen;
    struct sockaddr_un serv_addr;
    if ((sockfd= socket(AF_UNIX, SOCK_STREAM, 0) ) < 0)
        err_dump("client: can't open stream socket");
    /* Primeiro uma limpeza preventiva */
    bzero((char *) &serv_addr, sizeof(serv_addr));
    /* Dados para o socket stream: tipo + nome que identifica o servidor */
    serv_addr.sun_family = AF_UNIX;
    strcpy(serv_addr.sun_path, UNIXSTR_PATH);
    servlen = strlen(serv_addr.sun_path) + sizeof(serv_addr.sun_family);
}
