#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

int createSocketStream(char* address) {
    int sockfd, newsockfd, clilen, childpid, servlen;
    struct sockaddr_un cli_addr, serv_addr;
    if ((sockfd = socket(AF_UNIX,SOCK_STREAM,0) ) < 0)
        err_dump("Server: can't open stream socket");
    unlink(address);
    bzero((char*) &serv_addr, sizeof(serv_addr));
    serv_addr.sun_family = AF_UNIX;
    strcpy(serv_addr.sun_path, address);
    servlen = strlen(serv_addr.sun_path) + sizeof(serv_addr.sun_family);
    if (bind(sockfd, (struct sockaddr*) &serv_addr, servlen) < 0)
        err_dump("Server, can't bind local address");
    listen(sockfd, 5);
    for (;;) {
        clilen = sizeof(cli_addr);
        newsockfd = accept(sockfd, (struct sockaddr*) &cli_addr, &clilen);
        if (newsockfd < 0)
            err_dump("server: accept error");
        if ((childpid = fork()) < 0)
            err_dump("Server: fork error");
        else if (childpid == 0) {
            close(sockfd);
            /*faz cenas*/
            exit(0);
        }
    close(newsockfd);
    }
}
