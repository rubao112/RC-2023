#include "../include/download.h"
#include "../include/getip.h"
#include "../include/clientTCP.h"

int createSocket(char *ip, int port) {

    int sockfd;
    struct sockaddr_in server_addr;

    bzero((char *)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);
    server_addr.sin_port = htons(port);

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket()");
        exit(-1);
    }
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect()");
        exit(-1);
    }

    return sockfd;
}

int loginConnection(const int socket, const char *password, const char *user) {

    char response[SIZE_MAX];

    char inputPassword[strlen(password) + 6];
    sprintf(inputPassword, "pass %s\n", password);

    char inputUser[strlen(user) + 6];
    sprintf(inputUser, "user %s\n", user);

    write(socket, inputUser, strlen(inputUser));

    if (readResponse(socket, response) != SERVER_PASSREADY) {
        printf("Abort. Unknown user '%s'.\n", user);
        exit(-1);
    }

    write(socket, inputPassword, strlen(inputPassword));
    return readResponse(socket, response);
}

int passive(const int socket, int *port, char *ip) {
    
    char response[SIZE_MAX];
    write(socket, "pasv\n", 5);

    if (readResponse(socket, response) != SERVER_PASSIVE) {
        return -1;
    }

    int partsIp[4], partsPort[2];
    char *token;
    token = strchr(response, '(');

    if (token == NULL) {
        return -1;
    }

    token++;

    for (int i = 0; i < 4; i++) {
        token = strtok(i == 0 ? token : NULL, ",");

        if (token == NULL) {
            return -1;
        }

        partsIp[i] = atoi(token);
    }

    for (int i = 0; i < 2; i++) {
        token = strtok(NULL, i == 0 ? "," : ")");

        if (token == NULL) {
            return -1;
        }

        partsPort[i] = atoi(token);
    }

    sprintf(ip, "%d.%d.%d.%d", partsIp[0], partsIp[1], partsIp[2], partsIp[3]);

    *port = partsPort[0] * 256 + partsPort[1];

    return SERVER_PASSIVE;
}

int readResponse(const int socket, char *buffer) {

    char byte;
    int responseCode;
    int index = 0;
    ResponseState state = START;
    memset(buffer, 0, SIZE_MAX);

    while (state != END) {
        read(socket, &byte, 1);
        switch (state) {
        case START:
            if (byte == ' ') {
                state = SINGLE;
                } else if (byte == '-') {
                state = MULTIPLE;
                } else if (byte == '\n') {
                state = END; 
                } else {
                buffer[index++] = byte;
                }
            break;
        case MULTIPLE:
            if (byte == '\n') {
                memset(buffer, 0, SIZE_MAX);
                state = START;
                index = 0;
            } else { 
                buffer[index++] = byte;
                }
            break;
        case SINGLE:
            if (byte == '\n')
                state = END;
            else
                buffer[index++] = byte;
            break;
        case END:
            break;
        default:
            break;
        }
    }

    if (index >= 3 && isdigit(buffer[0]) && isdigit(buffer[1]) && isdigit(buffer[2])) {
        responseCode = (buffer[0] - '0') * 100 + (buffer[1] - '0') * 10 + (buffer[2] - '0');
    } else {
        responseCode = -1;
    }

    return responseCode;
}

int requestResource(const int socket, char *resource) {

    char inputFile[5 + strlen(resource) + 1], response[SIZE_MAX];
    sprintf(inputFile, "retr %s\n", resource);
    write(socket, inputFile, sizeof(inputFile));
    return readResponse(socket, response);
}

int getResource(const int socket1, const int socket2, char *filename) {

    FILE *fd = fopen(filename, "wb");

    if (fd == NULL) {
        printf("Opening or creating file error '%s'\n", filename);
        exit(-1);
    }

    char buffer[SIZE_MAX];
    int bytes;

    do {
        bytes = read(socket2, buffer, SIZE_MAX);
        if (fwrite(buffer, bytes, 1, fd) < 0)
            return -1;
    } while (bytes);

    fclose(fd);
    return readResponse(socket1, buffer);
}

int closeConnection(const int socket1, const int socket2) {

    char response[SIZE_MAX];
    write(socket1, "quit\n", 5);

    if (readResponse(socket1, response) != SERVER_GOODBYE) {
        return -1;
    }

    return close(socket1) || close(socket2);
}