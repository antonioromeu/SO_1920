#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>


int socket = 0;

int tfsMount(char* address) {
    int sockfd, dim_serv;
    struct sockaddr_un end_serv;
    if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
        perror("Erro ao criar socket clinte");
    socket = sockfd;
    bzero((char*) &end_serv, sizeof(end_serv));
    end_serv.sun_family = AF_UNIX;
    strcpy(end_serv.sun_path, address);
    dim_serv = strlen(end_serv.sun_path) + sizeof(end_serv.sun_family);
    if (connect(sockfd, (struct sockaddr*) &end_serv, dim_serv) < 0)
        perror("Error ao fazer connect no client");
    return 0;
}

int tfsCreate(char* filename, permission ownerPermissons, permission othersPermissions) {}

int tfsDelete(char *filename) {}

int tfsRename(char *filenameOld, char *filenameNew) {}

int tfsOpen(char *filename, permission mode) {}

int tfsClose(int fd) {}

int tfsRead(int fd, char *buffer, int len) {}

int tfsWrite(int fd, char *buffer, int len) {}

int tfsUnmout() {}

void envia_recebe_stream(FILE* fp, int sockfd) {
    int n;
    char buffer[MAXLINHA + 1];
    if ((fgets(buffer, MAXLINHA, fp) == NULL) && ferror(fp))
        perror("Error cliente ao ler input");
    char* command = buffer[0];
    int numTokens = 0;
    int fd, len;
    char* filename,
    permissions ownerPermissions, otherPermissions;
    switch (command) {
        case 'c':
            numTokens = sscanf(buffer, "%c %s %d%d", &command, filename, ownerPermissions, othersPermissions);
            if (numTokens != 4) {
                perror("Erro no c");
                exit(EXIT_FAILURE);
            }
            tfsCreate(filename, ownerPermissions, otherPermissions);
            break;
        case 'd':
            numTokens = sscanf(buffer, "%c %s", &command, filename);
            if (numTokens != 2) {
                perror("Erro no d");
                exit(EXIT_FAILURE);
            }
            tfsDelete(filename);
            break;
        case 'r':
            numTokens = sscanf(buffer, "%c %s %s", &command, oldFileName, newFileName);
            if (numTokens != 3) {
                perror("Erro no r");
                exit(EXIT_FAILURE);
            }
            tfsRename(filename, ownerPermissions, otherPermissions);
            break;
        case 'o':
            numTokens = sscanf(buffer, "%c %s %d", &command, filename, permission);
            if (numTokens != 3) {
                perror("Erro no o");
                exit(EXIT_FAILURE);
            }
            tfsOpen(filename, premission);
            break;
        case 'x':
            numTokens = sscanf(buffer, "%c %d", &command, fd);
            if (numTokens != 2) {
                perror("Erro no x");
                exit(EXIT_FAILURE)x;
            }
            tfsClose(fd);
            break;
        case 'l': 
            numTokens = sscanf(buffer, "%c %d %s %d", &command, buffer, len);
            if (numTokens != 4) {
                perror("Erro no l");
                exit(EXIT_FAILURE);
            }
            tfsRead(filename, ownerPErmissions, otherPermissions);
            break;
        case 'w':
            numTokens = sscanf(buffer, "%c %d %s %d", &command, fd, buffer, len);
            if (numTokens != 4) {
                perror("Erro no w");
                exit(EXIT_FAILURE);
            }
            tfsWrite(filename, ownerPErmissions, otherPermissions);
            break;
        default:
            perror("Erro no command");
            exit(EXIT_FAILURE);
        }
}

int main() {
    return 0;
}
