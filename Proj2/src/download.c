#include "../include/download.h"
#include "../include/getIp.h"
#include "../include/clientTCP.h"


int main(int argc, char *argv[]) {

    if (argc != 2) {
        printf("Correct usage: ./download ftp://[<user>:<password>@]<host>/<url-path>\n");
        exit(-1);
    }

    struct URL url;
    memset(&url, 0, sizeof(url));

    if (parse(argv[1], &url) != 0) {
        printf("Parse error. Usage: ./download ftp://[<user>:<password>@]<host>/<url-path>\n");
        exit(-1);
    }

    printf("Host: %s\nResource: %s\nFile: %s\nUser: %s\nPassword: %s\nIP Address: %s\n", url.host, url.resource, url.file, url.user, url.password, url.ip);

    char answer[MAX_LENGTH];
    int socketA = createSocket(url.ip, FTP_PORT);
    
    if (socketA < 0 || readResponse(socketA, answer) != SV_READY4AUTH) {
        printf("Socket to '%s' and port %d failed\n", url.ip, FTP_PORT);
        exit(-1);
    }

    if (loginConnection(socketA, url.user, url.password) != SV_LOGINSUCCESS) {
        printf("Authentication failed with username = '%s' and password = '%s'.\n", url.user, url.password);
        exit(-1);
    }

    int port;
    char ip[MAX_LENGTH];

    if (passive(socketA, ip, &port) != SV_PASSIVE) {
        printf("Passive mode failed\n");
        exit(-1);
    }

    int socketB = createSocket(ip, port);

    if (socketB < 0) {
        printf("Socket to '%s:%d' failed\n", ip, port);
        exit(-1);
    }

    if (requestResource(socketA, url.resource) != SV_READY4TRANSFER) {
        printf("Unknown resouce '%s' in '%s:%d'\n", url.resource, ip, port);
        exit(-1);
    }

    if (getResource(socketA, socketB, url.file) != SV_TRANSFER_COMPLETE) {
        printf("Error transfering file '%s' from '%s:%d'\n", url.file, ip, port);
        exit(-1);
    }

    if (closeConnection(socketA, socketB) != 0) {
        printf("Sockets close error\n");
        exit(-1);
    }

    return 0;
}
