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
        printf("Error parsing. Correct usage: ./download ftp://[<user>:<password>@]<host>/<url-path>\n");
        exit(-1);
    }

    printf("Host: %s\nUser: %s\nPassword: %s\nResource: %s\nFile name: %s\nIP Address: %s\n", url.host, url.user, url.password, url.resource, url.file, url.ip);

    char response[SIZE_MAX];
    int socket1 = createSocket(url.ip, FTP_PORT);

    if (socket1 < 0 || readResponse(socket1, response) != SERVER_LOGINREADY) {
        printf("Failed socket to '%s' and port %d\n", url.ip, FTP_PORT);
        exit(-1);
    }

    if (loginConnection(socket1, url.password, url.user) != SERVER_LOGINSUCCESS) {
        printf("Failed authentication with username = '%s' and password = '%s'.\n", url.user, url.password);
        exit(-1);
    }

    char ip[SIZE_MAX]; 
    int port;

    if (passive(socket1, &port, ip) != SERVER_PASSIVE) {
        printf("Failed passive mode\n");
        exit(-1);
    }

    int socket2 = createSocket(ip, port);

    if (socket2 < 0) {
        printf("Failed socket to '%s:%d'\n", ip, port);
        exit(-1);
    }

    if (requestResource(socket1, url.resource) != SERVER_TRANSFERREADY) {
        printf("Unknown resource '%s' in '%s:%d'\n", url.resource, ip, port);
        exit(-1);
    }

    if (getResource(socket1, socket2, url.file) != SERVER_TRANSFER_COMPLETE) {
        printf("Error transfering file '%s' from '%s:%d'\n", url.file, ip, port);
        exit(-1);
    }

    if (closeConnection(socket1, socket2) != 0) {
        printf("Error closing sockets\n");
        exit(-1);
    }

    return 0;
}
