// Link layer protocol implementation
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <termios.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "link_layer.h"

// Finite state machine states
typedef enum
{START, FLAG_RCV,A_RCV,C_RCV, BCC_NORMAL, BCC_DATA, DONE} stateMachine;

// Various constants and macros
#define C_RECEIVER 0x07
#define FALSE 0
#define TRUE 1
#define SIZE_SET 5
#define RECEIVER 0
#define BCC(n, m) (n ^ m)
#define A 0x03
#define SIZE_UA 5
#define _POSIX_SOURCE 1 // POSIX compliant source
#define F 0x7e
#define NACK(n) ((n) << 7 | 0x01)
#define ACK(n) ((n) << 7 | 0x05)
#define ESC 0x7D
#define TRANSMITER 1
#define FLAG 0x7e
#define C 0x03
#define REPEATED_MSG_CODE 2

stateMachine state;
int fd;     // file descriptor
int sequenceNum = 0;
int hasFailed = 0;
int alarmCount = 0;
int alarmOn;

volatile int STOP = FALSE;
struct termios oldtio; // old terminal IO settings

int nRetransmissions = 0;
LinkLayer linkLayer;

// Manager for alarm signal
void alarmManager(int signal)
{
    printf("<No answer from receiving end>\n");
    alarmOn = FALSE;
    alarmCount++;
    hasFailed = 1;
}

// Determine the current state of the state machine
void stateDetermine(stateMachine *state, char byte, int user)
{
    unsigned char FLAG_A = 0x03;                 // Address flag constant
    unsigned char FLAG_C = (user) ? 0x07 : 0x03; // Control flag constant, varies based on user role (transmitter or receiver)

    // Switch through the different states of the state machine
    switch (*state)
    {
    case START:
        // If FLAG is received, transition to the FLAG_RCV state
        if (byte == FLAG)
            *state = FLAG_RCV;
        break;

    case FLAG_RCV:
        // If another FLAG is received, stay in FLAG_RCV state
        if (byte == FLAG)
            *state = FLAG_RCV;
        // If FLAG_A (address byte) is received, transition to the A_RCV state
        else if (byte == FLAG_A)
            *state = A_RCV;
        // Any other byte, revert back to the START state
        else
            *state = START;
        break;

    case A_RCV:
        // If FLAG is received, transition to the FLAG_RCV state
        if (byte == FLAG)
            *state = FLAG_RCV;
        // If FLAG_C (control byte) is received, transition to the C_RCV state
        else if (byte == FLAG_C)
            *state = C_RCV;
        // Any other byte, revert back to the START state
        else
            *state = START;
        break;

    case C_RCV:
        // If FLAG is received, transition to the FLAG_RCV state
        if (byte == FLAG)
            *state = FLAG_RCV;
        // If BCC (checksum of address and control bytes) is received, transition to the BCC_NORMAL state
        else if (byte == BCC(FLAG_A, FLAG_C))
            *state = BCC_NORMAL;
        // Any other byte, revert back to the START state
        else
            *state = START;
        break;

    case BCC_NORMAL:
        // If FLAG is received, marking the end of the frame, transition to the DONE state
        if (byte == FLAG)
            *state = DONE;
        // Any other byte, revert back to the START state
        else
            *state = START;
        break;

    default:
        break;
    }
}

// LLOPEN

// Opens the logical link layer communication.
int llopen(LinkLayer connectionParameters)
{
    // Setup for signal handling
    struct sigaction action;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    action.sa_handler = alarmManager;
    sigaction(SIGALRM, &action, NULL);
    struct termios newtio;
    linkLayer = connectionParameters;

    // Open the serial port with read/write access
    fd = open(connectionParameters.serialPort, O_RDWR | O_NOCTTY);

    if (fd < 0)
    {
        perror(connectionParameters.serialPort);
        exit(-1);
    }

    // Save current settings of the port
    if (tcgetattr(fd, &oldtio) == -1)
    {
        perror("tcgetattr");
        exit(-1);
    }

    // Clear the structure for the new port settings
    memset(&newtio, 0, sizeof(newtio));

    // Set parameters for the serial port connection
    newtio.c_iflag = IGNPAR;
    newtio.c_cflag = connectionParameters.baudRate | CS8 | CLOCAL | CREAD;
    newtio.c_lflag = 0;
    newtio.c_oflag = 0;
    newtio.c_cc[VTIME] = 1; // Inter-character timer is not used
    newtio.c_cc[VMIN] = 0;  // Read blocks until a character arrives
    tcflush(fd, TCIOFLUSH);

    // Apply new settings to the port
    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    printf("Set new TermIOs struct\n");

    // If operating in transmitter mode
    if (connectionParameters.role == LlTx)
    {
        // Prepare the SET message for transmission
        unsigned char buf[256] = {0};
        buf[0] = FLAG;
        buf[1] = A;
        buf[2] = C;
        buf[3] = BCC(buf[1], buf[2]);
        buf[4] = F;
        hasFailed = 0;
        int bytesNum = 0;

        // Attempt to send the SET message and wait for UA response
        do
        {
            STOP = FALSE;
            bytesNum = write(fd, buf, SIZE_SET);
            printf("Sent SET: ");
            for (int i = 0; i < bytesNum; i++)
            {
                printf("%02X ", buf[i]); // Print each element of message as a hexadecimal value
            }
            printf("\n");
            alarm(connectionParameters.timeout);
            alarmOn = TRUE;
            printf("Attempt nº%d\n", alarmCount);
            state = START;
            hasFailed = 0;
            unsigned char aux = 0;

            while (STOP == FALSE)
            {
                bytesNum = read(fd, &aux, 1);
                if (bytesNum > 0)
                {
                    stateDetermine(&state, aux, 1);
                }
                if (state == DONE || hasFailed == 1)
                {
                    alarm(0);
                    STOP = TRUE;
                }
            }
        } while (alarmCount < connectionParameters.nRetransmissions && state != DONE);

        if (state == DONE)
        {
            printf("Received UA\n");
        }
        else
        {
            printf("Didn´t receive UA\n");
            return -1;
        }
    }
    else
    {
        // If operating in receiver mode
        unsigned char aux = 0;
        state = START;

        // Wait for a SET message
        while (STOP == FALSE)
        {
            read(fd, &aux, 1);
            stateDetermine(&state, aux, 0);

            if (state == DONE)
                STOP = TRUE;
        }

        printf("Received SET\n");

        // Prepare and send a UA message in response
        unsigned char message[256] = {0};
        message[0] = FLAG;
        message[1] = 0x03;
        message[2] = 0x07;
        message[3] = BCC(0x03, 0x07);
        message[4] = FLAG;

        int bytesNum = write(fd, message, SIZE_UA);
        printf("Sent UA: ");
        for (int i = 0; i < bytesNum; i++)
        {
            printf("%02X ", message[i]); // Print each element of message as a hexadecimal value
        }
        printf("\n");
    }

    return fd;
}

// Process received ACK messages
void receiveACK(stateMachine *state, unsigned char byte, unsigned char *ack, int sequenceNum)
{
    switch (*state)
    {
    case START:
        // If the byte is a FLAG, transition to the FLAG_RCV state.
        if (byte == FLAG)
        {
            *state = FLAG_RCV;
        }
        break;

    case FLAG_RCV:
        // On receiving a FLAG, remain in the FLAG_RCV state.
        if (byte == FLAG)
        {
            *state = FLAG_RCV;
        }
        // On receiving A, transition to the A_RCV state.
        else if (byte == A)
        {
            *state = A_RCV;
        }
        // Any other byte, revert to the START state.
        else
        {
            *state = START;
        }
        break;

    case A_RCV:
        // If a FLAG is received, return to the FLAG_RCV state.
        if (byte == FLAG)
        {
            *state = FLAG_RCV;
        }
        // If the byte matches the expected ACK, transition to the C_RCV state.
        else if (byte == ACK(sequenceNum))
        {
            *state = C_RCV;
            *ack = ACK(sequenceNum);
        }
        // If the byte matches the expected NACK, also transition to the C_RCV state.
        else if (byte == NACK(sequenceNum))
        {
            *state = C_RCV;
            *ack = NACK(sequenceNum);
        }
        // Any other byte, revert to the START state.
        else
        {
            *state = START;
        }
        break;

    case C_RCV:
        // If the byte matches the expected BCC value, transition to the BCC_NORMAL state.
        if (byte == BCC(A, *ack))
        {
            *state = BCC_NORMAL;
        }
        // If a FLAG is received, reset the acknowledgment and return to the FLAG_RCV state.
        else if (byte == FLAG_RCV)
        {
            *ack = FALSE;
            *state = FLAG_RCV;
        }
        // For any other byte, reset acknowledgment and revert to the START state.
        else
        {
            *state = START;
            *ack = FALSE;
        }
        break;

    case BCC_NORMAL:
        // If a FLAG is received after BCC, transition to the DONE state, indicating a complete ACK/NACK frame.
        if (byte == FLAG)
        {
            *state = DONE;
        }
        // Any other byte, reset the acknowledgment and revert to the START state.
        else
        {
            *ack = FALSE;
            *state = START;
        }
        break;

    default:
        break;
    }
}

// Sends a data buffer over the link layer, handles byte stuffing, and acknowledges receipt.
int llwrite(const unsigned char *buf, int bufSize)
{
    signal(SIGALRM, alarmManager); // Register the alarm signal manager
    state = START;
    int attemptNum = 0;         // Counter for retry attempts

    // Count bytes that need stuffing
    unsigned int counter = 0;
    for (int i = 0; i < bufSize; i++)
    {
        if (buf[i] == FLAG || buf[i] == ESC)
        {
            counter++;
        }
    }

    unsigned int size = bufSize + 6 + counter; // Calculate total size after stuffing
    unsigned char message[size];
    memset(message, 0, size); // Initialize the message buffer

    // Frame header
    message[0] = FLAG;
    message[1] = A;
    message[2] = sequenceNum << 7;
    message[3] = BCC(A, sequenceNum << 7);

    unsigned int BCC2 = 0, i = 0;

    // Byte stuffing for the message
    for (int j = 0; j < bufSize; j++)
    {
        switch (buf[j])
        {
        case ESC:
            message[i + j + 4] = ESC;
            message[i + j + 5] = ESC ^0x20;
            BCC2 = BCC(BCC2, ESC);
            i++;
            break;

        case FLAG:
            message[i + j + 4] = ESC;
            message[i + j + 5] = FLAG ^0x20;
            BCC2 = BCC(BCC2, FLAG);
            i++;
            break;

        default:
            message[i + j + 4] = buf[j];
            BCC2 = BCC(BCC2, buf[j]);
        }
    }

    message[i + bufSize + 4] = BCC2; // Frame footer with BCC and FLAG
    message[i + bufSize + 5] = FLAG;

    STOP = FALSE;
    alarmOn = FALSE;

    // Loop until the frame is acknowledged or the maximum number of attempts is reached
    while (STOP != TRUE && state != DONE)
    { 
        unsigned char ack;      // ACK byte
        unsigned char receivedByte; // Byte read from the link

        // Send the frame if the alarm is not already set
        if (alarmOn == FALSE)
        {
            if (attemptNum > linkLayer.nRetransmissions)
            {
                return -1; // Max attempts reached
            }
            attemptNum++;

            write(fd, message, size);          // Send the message
            signal(SIGALRM, alarmManager); // Set the alarm signal manager again
            alarm(3);                      // Set an alarm for 3 seconds
            alarmOn = TRUE;
        }

        // Read a byte from the link
        unsigned int bytesNum = read(fd, &receivedByte, 1);

        if (bytesNum > 0)
        {
            receiveACK(&state, receivedByte, &ack, 1 - sequenceNum);

            // Check if the acknowledgment is as expected
            if (state == DONE && ack == ACK(1 - sequenceNum))
            {
                sequenceNum = 1 - sequenceNum;
                alarm(0);    // Cancel the alarm
                STOP = TRUE; // Stop the loop
                printf("RECEIVED ACK aka RR...\n");
            }
            // Resend the frame if NACK received
            else if (state == DONE)
            {
                printf("RECEIVED NACK aka RREJ...\n");
                write(fd, message, size);
                state = START;
            }
        }
    }
    return 0; // Return success
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////

#define HEADER_ERROR_PROB   0.1
#define DATA_ERROR_PROB     0.8 / 1020

// Function to process received data and handle byte stuffing return true when it must return ack, and false for nack
int receiveData(unsigned char *packet, int sequenceNum, size_t *size_read)
{
    state = START;
    unsigned char REPLY_C = (1 - sequenceNum) << 7;
    unsigned char CONTROL_C = sequenceNum << 7;          // Construct control byte using sequence number
    unsigned int BCC2 = 0, i = 0, stuffing = 0; // Stuffing flag: set to 1 every time an ESC is encountered

    // Keep processing bytes until the end flag is received
    while (state != DONE)
    {
        unsigned char receivedByte;
        unsigned int bytesNum = read(fd, &receivedByte, 1);

        if (bytesNum > 0)
        {
            // Check if the byte is in the data part of the frame
            
            if (state == BCC_DATA)
            {
                // Simulate an error in the data part with a specific probability (FER)
                if ((double)rand() / RAND_MAX < DATA_ERROR_PROB)
                {
                    receivedByte ^= (1 << (rand() % 8)); // Flip a random bit in the data
                }
            }
            

            switch (state)
            {
            case START:
                if (receivedByte == FLAG)
                {
                    state = FLAG_RCV;
                }
                break;

            case A_RCV:
                if (receivedByte == FLAG)
                {
                    state = FLAG_RCV;
                }
                else if (receivedByte == CONTROL_C)
                {
                    state = C_RCV;
                }
                // Handling case of receiving repeated message and waiting for the next one
                else if (REPLY_C == receivedByte)
                {
                    return REPEATED_MSG_CODE; // Return code indicating a repeated message was received
                }
                else
                {
                    state = START;
                }
                break;

            case FLAG_RCV:
                if (receivedByte == FLAG)
                {
                    state = FLAG_RCV;
                }
                else if (receivedByte == A)
                {
                    state = A_RCV;
                }
                else
                {
                    state = START;
                }
                break;

            case C_RCV:
                if (receivedByte == FLAG)
                {
                    state = FLAG_RCV;
                }
                else if (receivedByte == BCC(A, CONTROL_C))
                {
                    state = BCC_DATA;
                }
                else
                {
                    state = START;
                }
                break;

            // Handle byte stuffing and frame de-stuffing
            case BCC_DATA:
                if (!stuffing)
                {
                    if (receivedByte == FLAG)
                    {
                        state = DONE;
                        BCC2 = BCC(BCC2, packet[i - 1]);
                        *size_read = i - 1;             // Update the size of the read data
                        return (BCC2 == packet[i - 1]); // Return TRUE if BCC matches, otherwise FALSE
                    }
                    else if (receivedByte == ESC)
                    {
                        stuffing = TRUE; // Set the stuffing flag if ESC is encountered
                    }
                    else
                    {
                        BCC2 = BCC(BCC2, receivedByte);
                        packet[i] = receivedByte;
                        i++;
                    }
                }
                else
                {
                    stuffing = FALSE; // Reset the stuffing flag
                    BCC2 = BCC(BCC2, receivedByte ^ 0x20);
                    packet[i] = receivedByte ^ 0x20;
                    i++;
                }
                break;

            default:
                break;
            }
        }
    }

    return FALSE;
}


// Reads data from the link layer and acknowledges the received data.
int llread(unsigned char *packet)
{
    int readStatus;
    size_t size_read;


    // Continuously try to receive data until successful
    while ((readStatus = receiveData(packet, sequenceNum, &size_read)) != TRUE)
    {
        // If the reply indicates an error (e.g., checksum mismatch)
        if (readStatus == 0)
        {
            printf("Sending RRej or NACK\n");

            // Send a NACK (negative acknowledgment)
            unsigned char NACK_C = NACK(1 - sequenceNum);
            unsigned char buf[] = {FLAG, A, NACK_C, BCC(A, NACK_C), F};
            write(fd, buf, 5);
        }
        // If the received message is a duplicate (e.g., retransmission)
        else
        {
            printf("Repeated message, sending ACK\n");

            // Send an ACK to prevent further retransmissions
            unsigned char ACK_C = ACK(sequenceNum);
            unsigned char buf[] = {FLAG, A, ACK_C, BCC(A, ACK_C), F};
            write(fd, buf, 5);
        }
    }

    usleep(10000);

    /*
    printf("|Received packet (size: %zu):\n", size_read);
        for (size_t i = 0; i < size_read; i++) {
            printf("%02X ", packet[i]); // Print each byte as a hexadecimal value
        }
        printf("|\n");
        */
    // Once data is received correctly, send an ACK
    printf("Everything in order, sending ACK\n");
    sequenceNum = 1 - sequenceNum; // Toggle the sequence number
    unsigned char ACK_C = ACK(sequenceNum);
    unsigned char buf[] = {FLAG, A, ACK_C, BCC(A, ACK_C), F};
    write(fd, buf, 5);

    return size_read;
}

// LLCLOSE

// Determine the next state of the state machine based on the received byte.
void DISCStateDetermine(stateMachine *state, char byte)
{
    // Define the flags for the state transitions
    unsigned char FLAG_C = 0x0B;
    unsigned char FLAG_A = 0x03;

    // Determine the current state and process the incoming byte
    switch (*state)
    {
    // Waiting for the initial FLAG byte
    case START:
        if (byte == FLAG)
        {
            *state = FLAG_RCV;
        }
        break;

    // A FLAG byte has been received and waiting for the FLAG_A
    case FLAG_RCV:
        if (byte == FLAG)
        {
            *state = FLAG_RCV;
        }
        else if (byte == FLAG_A)
        {
            *state = A_RCV;
        }
        else
        {
            *state = START;
        }
        break;

    // FLAG_C has been received, waiting for BCC of FLAG_A and FLAG_C
    case C_RCV:
        if (byte == FLAG)
        {
            *state = FLAG_RCV;
        }
        else if (byte == BCC(FLAG_A, FLAG_C))
        {
            *state = BCC_NORMAL;
        }
        else
        {
            *state = START;
        }
        break;

    // FLAG_A has been received, waiting for FLAG_C
    case A_RCV:
        if (byte == FLAG)
        {
            *state = FLAG_RCV;
        }
        else if (byte == FLAG_C)
        {
            *state = C_RCV;
        }
        else
        {
            *state = START;
        }
        break;

    // BCC for FLAG_A and FLAG_C received, waiting for final FLAG
    case BCC_NORMAL:
        if (byte == FLAG)
        {
            *state = DONE;
        }
        else
        {
            *state = START;
        }
        break;

    default:
        break;
    }
}

// Closes the logical link layer communication.
int llclose(int statistics)
{
    int bytesNum = 0;
    unsigned char aux = 0;
    unsigned char message[256] = {0};

    switch (linkLayer.role)
    {
    case LlTx:
        // Send DISC (Disconnect Frame)
        message[0] = FLAG;
        message[1] = 0x03;
        message[2] = 0x0B;
        message[3] = BCC(0x03, 0x0B);
        message[4] = FLAG;
        bytesNum = write(fd, message, 5);
        printf("Sent DISC: ");
        for (int i = 0; i < bytesNum; i++)
        {
            printf("%02X ", message[i]); // Print each element of message as a hexadecimal value
        }
        printf("\n");
        state = START;
        STOP = FALSE;

        // Wait for DISC acknowledgment
        while (STOP == FALSE)
        {
            read(fd, &aux, 1);
            DISCStateDetermine(&state, aux);
            if (state == DONE)
                STOP = TRUE;
        }

        // Send UA (Unnumbered Acknowledgment Frame)
        unsigned char ua[256] = {0};
        ua[0] = FLAG;
        ua[1] = 0x03;
        ua[2] = 0x07;
        ua[3] = BCC(0x03, 0x07);
        ua[4] = FLAG;
        bytesNum = write(fd, ua, SIZE_UA);
        printf("UA sent: ");
        for (int i = 0; i < bytesNum; i++)
        {
            printf("%02X ", ua[i]); // Print each element of message as a hexadecimal value
        }
        printf("\n");
        break;

    case LlRx:
        STOP = FALSE;
        state = START;

        // Wait for DISC from transmitter
        while (STOP == FALSE)
        {
            read(fd, &aux, 1);
            DISCStateDetermine(&state, aux);

            if (state == DONE)
                STOP = TRUE;
        }
        printf("Received DISC\n");

        // Send DISC as acknowledgment
        message[0] = FLAG;
        message[1] = 0x03;
        message[2] = 0x0B;
        message[3] = BCC(0x03, 0x0B);
        message[4] = FLAG;
        bytesNum = write(fd, message, 5);

        printf("Sent DISC to acknowledge: ");

        for (int i = 0; i < bytesNum; i++)
        {
            printf("%02X ", message[i]); // Print each element of message as a hexadecimal value
        }
        printf("\n");

        // Wait for UA acknowledgment from transmitter
        state = START;
        STOP = FALSE;
        while (STOP == FALSE)
        {
            read(fd, &aux, 1);
            stateDetermine(&state, aux, 1);

            if (state == DONE)
                STOP = TRUE;
        }
        printf("Received UA\n");
        break;
    }

    // Restore old terminal settings
    if (tcsetattr(fd, TCSANOW, &oldtio) != 0)
    {
        perror("llclose() - Error on tcsetattr()");
        return -1;
    }

    // Close the file descriptor
    if (close(fd) != 0)
    {
        perror("llclose() - Error on close()");
        return -1;
    }
    return 1;
}
