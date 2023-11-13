// Application layer protocol implementation

#include "application_layer.h"
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include "link_layer.h"

struct applicationLayer
{
int fileDescriptor; /*Descritor correspondente à porta série*/
int status; /*TRANSMITTER | RECEIVER*/
};

#define END_PACKET 0x03
#define START_PACKET 0x02
#define DATA_PACKET 0x01

// Function to send control packet with file information
int sendCPacket(int fd, unsigned char packetType, const char *filename) 
{
    FILE *f = fopen(filename, "rb");
    fseek(f, 0L, SEEK_END);
    int sizeF = ftell(f);
    fclose(f);
    unsigned char buf[1024];
    unsigned char b3 = sizeF >> 8;
    unsigned char b4 = (unsigned char)sizeF;
    
    buf[0] = packetType;
    buf[1] = 0x00;
    buf[2] = 0x02;
    buf[3] = b3;
    buf[4] = b4;
    buf[5] = 0x01;
    buf[6] = strlen(filename);
    
    memcpy(buf + 7, filename, strlen(filename));
    llwrite(buf,strlen(filename) + 7);
    
    return 0;
}

// Function to send data packets with file content
int sendDPacket(int fd, const char *filename) 
{
    FILE *f = fopen(filename, "rb");
    unsigned char buf[MAX_PAYLOAD_SIZE + 4];
    unsigned int bytesRead = 0;
    int packetNumber = 0;

    while ((bytesRead = fread(buf + 4, 1, MAX_PAYLOAD_SIZE, f)) > 0) 
    {
        buf[0] = DATA_PACKET;
        buf[1] = packetNumber;
        buf[2] = bytesRead / 256;
        buf[3] = bytesRead % 256;

        if (llwrite(buf, bytesRead + 4) == -1) 
        {
            printf("Maximum tries reached\n");
            exit(-1);
        }
        packetNumber++;
    }

    fclose(f);
    return 0;
}

// Function to direct packet send request to appropriate function based on packet type
int sendPacket(int fd, unsigned char packetType, const char *filename) 
{
    switch (packetType) {
        case START_PACKET:
        
        case END_PACKET:
            return sendCPacket(fd, packetType, filename);
        
        case DATA_PACKET:
            return sendDPacket(fd, filename);

        default:
            return -1;
    }
}

// Function to receive packets and write data to file as per packet type
int receivePacket(int fd, const char *filename) 
{   
    FILE *f;
    unsigned int addSize, bytesRead;
    unsigned char buf[(MAX_PAYLOAD_SIZE+4)*2];
    int packetNumber = 0;
   
    while (1) {
        bytesRead = llread(buf);

        if (buf[0] == START_PACKET) 
        {
            f = fopen(filename, "wb");
        } else if (bytesRead > 0 && buf[0] == DATA_PACKET) 
        {
            addSize = buf[2] * 256 + buf[3];
            fwrite(buf + 4, 1, addSize, f);
            if (buf[1] == packetNumber) 
            {
                packetNumber++;
            }
        } else if (buf[0] == END_PACKET)
        {
            printf("ENDING\n");
            break;
        }
    }
    
    fclose(f);
    return fd;
}

void applicationLayer(const char *port, const char *role, int baudRate,
                      int retries, int timeout, const char *filename) 
{
    struct applicationLayer applicationLayer;
    
    LinkLayerRole linkRole;
    linkRole = (strcmp(role, "tx") == 0) ? LlTx : LlRx;
    
    applicationLayer.status = (strcmp(role, "tx") == 0) ? 1 : 0;
    
    LinkLayer linkLayer;
    linkLayer.role = linkRole;
    linkLayer.baudRate = baudRate;
    linkLayer.nRetransmissions = retries;
    strcpy(linkLayer.serialPort, port);
    linkLayer.timeout = timeout;
    
    int fd = llopen(linkLayer);
    
    if (fd == -1)
        return;
    
    applicationLayer.fileDescriptor = fd;
    
    switch (applicationLayer.status)
    {   
        case 1:
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
    llclose(0);
}

