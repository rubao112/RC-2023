#include "../include/download.h"
#include "../include/getip.h"

int createSocket(char *ip, int port)
{

    int sockfd;
    struct sockaddr_in server_addr;

    bzero((char *)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);
    server_addr.sin_port = htons(port);

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket()");
        exit(-1);
    }
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("connect()");
        exit(-1);
    }

    return sockfd;
}

int authConn(const int socket, const char *user, const char *pass)
{

    char userCommand[5 + strlen(user) + 1];
    sprintf(userCommand, "user %s\n", user);
    char passCommand[5 + strlen(pass) + 1];
    sprintf(passCommand, "pass %s\n", pass);
    char answer[MAX_LENGTH];

    write(socket, userCommand, strlen(userCommand));
    if (readResponse(socket, answer) != SV_READY4PASS)
    {
        printf("Unknown user '%s'. Abort.\n", user);
        exit(-1);
    }

    write(socket, passCommand, strlen(passCommand));
    return readResponse(socket, answer);
}

int passiveMode(const int socket, char *ip, int *port)
{
    char answer[MAX_LENGTH];
    write(socket, "pasv\n", 5);
    if (readResponse(socket, answer) != SV_PASSIVE)
        return -1;

    char *token;
    int ip_parts[4], port_parts[2];

    token = strchr(answer, '(');
    if (token == NULL)
        return -1;
    token++;

    for (int i = 0; i < 4; i++)
    {
        token = strtok(i == 0 ? token : NULL, ",");
        if (token == NULL)
            return -1;
        ip_parts[i] = atoi(token);
    }

    for (int i = 0; i < 2; i++)
    {
        token = strtok(NULL, i == 0 ? "," : ")");
        if (token == NULL)
            return -1;
        port_parts[i] = atoi(token);
    }

    sprintf(ip, "%d.%d.%d.%d", ip_parts[0], ip_parts[1], ip_parts[2], ip_parts[3]);

    *port = port_parts[0] * 256 + port_parts[1];

    return SV_PASSIVE;
}

int readResponse(const int socket, char *buffer)
{
    char byte;
    int index = 0, responseCode;
    ResponseState state = START;
    memset(buffer, 0, MAX_LENGTH);

    while (state != END)
    {
        read(socket, &byte, 1);
        switch (state)
        {
        case START:
            if (byte == ' ')
                state = SINGLE;
            else if (byte == '-')
                state = MULTIPLE;
            else if (byte == '\n')
                state = END;
            else
                buffer[index++] = byte;
            break;
        case SINGLE:
            if (byte == '\n')
                state = END;
            else
                buffer[index++] = byte;
            break;
        case MULTIPLE:
            if (byte == '\n')
            {
                memset(buffer, 0, MAX_LENGTH);
                state = START;
                index = 0;
            }
            else
                buffer[index++] = byte;
            break;
        case END:
            break;
        default:
            break;
        }
    }

    if (index >= 3 && isdigit(buffer[0]) && isdigit(buffer[1]) && isdigit(buffer[2]))
    {
        responseCode = (buffer[0] - '0') * 100 + (buffer[1] - '0') * 10 + (buffer[2] - '0');
    }
    else
    {
        responseCode = -1;
    }

    return responseCode;
}

int requestResource(const int socket, char *resource)
{

    char fileCommand[5 + strlen(resource) + 1], answer[MAX_LENGTH];
    sprintf(fileCommand, "retr %s\n", resource);
    write(socket, fileCommand, sizeof(fileCommand));
    return readResponse(socket, answer);
}

int getResource(const int socketA, const int socketB, char *filename)
{

    FILE *fd = fopen(filename, "wb");
    if (fd == NULL)
    {
        printf("Error opening or creating file '%s'\n", filename);
        exit(-1);
    }

    char buffer[MAX_LENGTH];
    int bytes;
    do
    {
        bytes = read(socketB, buffer, MAX_LENGTH);
        if (fwrite(buffer, bytes, 1, fd) < 0)
            return -1;
    } while (bytes);
    fclose(fd);

    return readResponse(socketA, buffer);
}

int closeConnection(const int socketA, const int socketB)
{

    char answer[MAX_LENGTH];
    write(socketA, "quit\n", 5);
    if (readResponse(socketA, answer) != SV_GOODBYE)
        return -1;
    return close(socketA) || close(socketB);
}