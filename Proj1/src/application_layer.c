// Application layer protocol implementation

#include "application_layer.h"
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include "link_layer.h"

struct applicationLayer{
int fileDescriptor; /*Descritor correspondente à porta série*/
int status; /*TRANSMITTER | RECEIVER*/
};

#define END_PACKET 0x03
#define START_PACKET 0x02
#define DATA_PACKET 0x01

int sendControlPacket(int fd, unsigned char packetType, const char *filename) 
{
    FILE *file = fopen(filename, "rb");
    fseek(file, 0L, SEEK_END);
    int fileSize = ftell(file);
    fclose(file);

    unsigned char buffer[1000];
    buffer[0] = packetType;
    buffer[1] = 0x00;
    buffer[2] = 0x02;
    buffer[3] = fileSize >> 8;
    buffer[4] = (unsigned char)fileSize;
    buffer[5] = 0x01;
    buffer[6] = strlen(filename);
    strcpy(buffer + 7, filename);

    llwrite(buffer, 7 + strlen(filename));
    return 0;
}

int sendDataPacket(int fd, const char *filename) 
{
    FILE *file = fopen(filename, "rb");
    int packetNumber = 0;
    unsigned char buffer[1000];
    unsigned int bytesRead = 0;
    while ((bytesRead = fread(buffer + 4, 1, 996, file)) > 0) {
        buffer[0] = DATA_PACKET;
        buffer[1] = packetNumber;
        buffer[2] = bytesRead / 256;
        buffer[3] = bytesRead % 256;

        if (llwrite(buffer, bytesRead + 4) == -1) {
            printf("Max number of tries reached\n");
            exit(-1);
        }
        packetNumber++;
    }

    fclose(file);
    return 0;
}

int receivePacket(int fd, const char *filename) 
{
    unsigned char buffer[2000];
    int packetNumber = 0;
    FILE *file;
    unsigned int fileSize, appendSize, bytesRead;
    while (1) {
        bytesRead = llread(buffer);

        if (buffer[0] == START_PACKET) {
            file = fopen(filename, "wb");
            fileSize = buffer[3] << 8 | buffer[4];
        } else if (buffer[0] == DATA_PACKET && bytesRead > 0) {
            appendSize = buffer[2] * 256 + buffer[3];
            fwrite(buffer + 4, 1, appendSize, file);
            if (buffer[1] == packetNumber) {
                packetNumber++;
            }
        } else if (buffer[0] == END_PACKET) {
            printf("END\n");
            break;
        }
    }
    fclose(file);
    return fd;
}

int sendPacket(int fd, unsigned char packetType, const char *filename) 
{
    switch (packetType) {
        case START_PACKET:
        case END_PACKET:
            return sendControlPacket(fd, packetType, filename);
        case DATA_PACKET:
            return sendDataPacket(fd, filename);

        default:
            return -1;
    }
}

void applicationLayer(const char *port, const char *role, int baudRate,
                      int retries, int timeout, const char *filename) {
    struct applicationLayer appLayer;
    LinkLayerRole linkRole;
    linkRole = (strcmp(role, "tx") == 0) ? LlTx : LlRx;
    appLayer.status = (strcmp(role, "tx") == 0) ? 1 : 0;
    LinkLayer linkLayer;
    linkLayer.baudRate = baudRate;
    linkLayer.nRetransmissions = retries;
    linkLayer.role = linkRole;
    strcpy(linkLayer.serialPort, port);
    linkLayer.timeout = timeout;
    int fd = llopen(linkLayer);
    if (fd == -1)
        return;
    appLayer.fileDescriptor = fd;
    switch (appLayer.status)
    {   case 1:
            printf("Sending file\n");
            sendPacket(fd, START_PACKET, filename);
            sendPacket(fd, DATA_PACKET, filename);
            sendPacket(fd, END_PACKET, filename);
            break;
        case 0:
            printf("Receiving file\n");
            receivePacket(fd, filename);
            break;
        default:
            printf("Invalid role\n");
    }

    printf("END\n");
    llclose(0, linkLayer);}
