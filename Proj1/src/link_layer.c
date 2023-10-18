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
typedef enum {START, FLAG_RCV, A_RCV, C_RCV, BCC_NORMAL, BCC_DATA, DONE} stateMachine;

// Various constants and macros
#define C_RECEIVER 0x07
#define FALSE 0
#define SET_SIZE 5
#define RECEIVER 0
#define BCC(n,m) (n ^ m)
#define A 0x03
#define UA_SIZE 5
#define _POSIX_SOURCE 1 // POSIX compliant source
#define F 0x7e
#define TRUE 1
#define NACK(n) ((n)<<7 | 0x01)
#define ESC 0x7D
#define TRANSMITER 1
#define ACK(n) ((n)<<7 | 0x05)
#define FLAG 0x7e
#define C 0x03
#define REPEATED_MESSAGE 2

stateMachine state;
int fd; // file descriptor
int sn = 0; // sequence number
int failed = 0;
int alarm_count = 0;
int alarm_enabled;

volatile int STOP = FALSE;
struct termios oldtio; // old terminal IO settings

// Handler for alarm signal
void alarmHandler(int signal)
{   
    printf("<Receiver didn't Answer>\n");
    alarm_enabled = FALSE;
    alarm_count++;
    failed = 1;
}

// Determine the current state of the state machine
void determineState(stateMachine *state, char byte, int user)
{
    unsigned char A_FLAG = 0x03; // Address flag constant
    unsigned char C_FLAG = (user)? 0x07 : 0x03; // Control flag constant, varies based on user role (transmitter or receiver)
    
    // Switch through the different states of the state machine
    switch(*state)
    {
        case START:
            // If FLAG is received, transition to the FLAG_RCV state
            if(byte == FLAG) *state = FLAG_RCV;
            break;

        case FLAG_RCV:
            // If another FLAG is received, stay in FLAG_RCV state
            if(byte == FLAG) *state = FLAG_RCV;
            // If A_FLAG (address byte) is received, transition to the A_RCV state
            else if(byte == A_FLAG) *state = A_RCV;
            // Any other byte, revert back to the START state
            else *state = START;
            break;

        case A_RCV:
            // If FLAG is received, transition to the FLAG_RCV state
            if(byte == FLAG) *state = FLAG_RCV;
            // If C_FLAG (control byte) is received, transition to the C_RCV state
            else if(byte == C_FLAG) *state = C_RCV;
            // Any other byte, revert back to the START state
            else *state = START;
            break;

        case C_RCV:
            // If FLAG is received, transition to the FLAG_RCV state
            if(byte == FLAG) *state = FLAG_RCV;
            // If BCC (checksum of address and control bytes) is received, transition to the BCC_NORMAL state
            else if(byte == BCC(A_FLAG, C_FLAG)) *state = BCC_NORMAL;
            // Any other byte, revert back to the START state
            else *state = START;
            break;

        case BCC_NORMAL:
            // If FLAG is received, marking the end of the frame, transition to the DONE state
            if(byte == FLAG)  *state = DONE;
            // Any other byte, revert back to the START state
            else *state = START;
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
    action.sa_handler = alarmHandler;
    sigaction(SIGALRM, &action, NULL);
    struct termios newtio;
    
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
    
    printf("New termios structure set\n");
    
    // If operating in transmitter mode
    if(connectionParameters.role == LlTx)
    {
        // Prepare the SET message for transmission
        unsigned char buf[256] = {0};
        buf[0] = FLAG;
        buf[1] = A;
        buf[2] = C;
        buf[3] = BCC(buf[1], buf[2]);
        buf[4] = F;
        failed = 0;
        int bytes = 0;
        
        // Attempt to send the SET message and wait for UA response
        do
        {
            STOP = FALSE;
            bytes = write(fd, buf, SET_SIZE);
            printf("%d bytes written\n", bytes);
            alarm(connectionParameters.timeout);
            alarm_enabled = TRUE;
            printf("Attempt %d\n", alarm_count);
            state = START;
            failed = 0;
            unsigned char aux = 0;

            while (STOP == FALSE)
            {
                bytes = read(fd, &aux, 1);
                if (bytes > 0)
                {
                    determineState(&state, aux, 1);
                }
                if (state == DONE || failed == 1)
                {
                    alarm(0);
                    STOP = TRUE;
                }
            }
        } while(alarm_count < connectionParameters.nRetransmissions && state != DONE);
        
        if(state == DONE) 
        {
            printf("UA Received\n");
        } else
        {
            printf("UA Not Received\n");
            return -1;
        }
    } else
    {  
        // If operating in receiver mode
        unsigned char aux = 0;
        state = START;
        
        // Wait for a SET message
        while (STOP == FALSE)
        {
            read(fd, &aux, 1);
            determineState(&state, aux, 0);
            
            if(state == DONE) STOP = TRUE;
        }

        printf("Received SET");
        
        // Prepare and send a UA message in response
        unsigned char msg[256] = {0};
        msg[0] = FLAG;
        msg[1] = 0x03;
        msg[2] = 0x07;
        msg[3] = BCC(0x03, 0x07);
        msg[4] = FLAG;
        
        int bytes= write(fd, msg, UA_SIZE);
        printf(":%s:%d\n", msg, bytes);
    }
    
    return fd;
}

// Process received ACK messages
void receiveACK(stateMachine* state, unsigned char byte, unsigned char * ack, int sn) 
{
    switch(*state) 
    {
        case START:
            // If the byte is a FLAG, transition to the FLAG_RCV state.
            if(byte == FLAG) 
            {
                *state = FLAG_RCV;
            }
            break;

        case FLAG_RCV:
            // On receiving a FLAG, remain in the FLAG_RCV state.
            if(byte == FLAG) 
            {
                *state = FLAG_RCV;
            }
            // On receiving A, transition to the A_RCV state.
            else if(byte == A) 
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
            if(byte == FLAG) 
            {
                *state = FLAG_RCV;
            }
            // If the byte matches the expected ACK, transition to the C_RCV state.
            else if(byte == ACK(sn)) 
            {
                *state = C_RCV;
                *ack = ACK(sn);
            }
            // If the byte matches the expected NACK, also transition to the C_RCV state.
            else if(byte == NACK(sn)) 
            {
                *state = C_RCV;
                *ack = NACK(sn);
            }
            // Any other byte, revert to the START state.
            else 
            {
                *state = START;
            }
            break;

        case C_RCV:
            // If the byte matches the expected BCC value, transition to the BCC_NORMAL state.
            if(byte == BCC(A, *ack)) 
            {
                *state = BCC_NORMAL;
            }
            // If a FLAG is received, reset the acknowledgment and return to the FLAG_RCV state.
            else if(byte == FLAG_RCV) 
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
            if(byte == FLAG) 
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
    }
}

//Sends a data buffer over the link layer, handles byte stuffing, and acknowledges receipt.
int llwrite(const unsigned char *buf, int bufSize) 
{
    state = START;
    int done = FALSE;       // Flag to indicate if data is sent successfully
    int attemptNumber = 0;   // Counter for retry attempts
    signal(SIGALRM, alarmHandler); // Register the alarm signal handler
    state = START;

    // Count bytes that need stuffing
    unsigned int count = 0;
    for (int i = 0; i < bufSize; i++) 
    {
        if (buf[i] == FLAG || buf[i] == ESC)
        {
            count++;
        }    
    }

    unsigned int size = bufSize + 6 + count; // Calculate total size after stuffing
    unsigned char msg[size];
    memset(msg, 0, size);  // Initialize the message buffer

    // Frame header
    msg[0] = FLAG;
    msg[1] = A;
    msg[2] = sn << 7;
    msg[3] = BCC(A, sn << 7);

    unsigned int i = 0, BCC2 = 0;

    // Byte stuffing for the message
    for (int j = 0; j < bufSize; j++) 
    {
        switch (buf[j]) 
        {
            case ESC:
                msg[j + i + 4] = ESC;
                msg[j + i + 5] = ESC;
                BCC2 = BCC(BCC2, ESC);
                i++;
                break;

            case FLAG:
                msg[j + i + 4] = ESC;
                msg[j + i + 5] = FLAG;
                BCC2 = BCC(BCC2, FLAG);
                i++;
                break;

            default:
                msg[j + i + 4] = buf[j];
                BCC2 = BCC(BCC2, buf[j]);
        }
    }

    msg[i + bufSize + 4] = BCC2;   // Frame footer with BCC and FLAG
    msg[i + bufSize + 5] = FLAG;

    STOP = FALSE;
    alarm_enabled = FALSE;

    // Loop until the frame is acknowledged or the maximum number of attempts is reached
    while (state != DONE && STOP != TRUE) 
    {
        unsigned char received;  // Byte read from the link
        unsigned char ack;       // ACK byte
        
        // Send the frame if the alarm is not already set
        if (alarm_enabled == FALSE) 
        {
            if (attemptNumber == 4) 
            {
                return -1; // Max attempts reached
            } 
            attemptNumber++;

            write(fd, msg, size); // Send the message
            signal(SIGALRM, alarmHandler); // Set the alarm signal handler again
            alarm(3);             // Set an alarm for 3 seconds
            alarm_enabled = TRUE;
        }

        // Read a byte from the link
        unsigned int bytes = read(fd, &received, 1);

        if (bytes > 0) 
        {
            receiveACK(&state, received, &ack, 1 - sn);

            // Check if the acknowledgment is as expected
            if (state == DONE && ack == ACK(1 - sn)) 
            {
                sn = 1 - sn;
                alarm(0);       // Cancel the alarm
                STOP = TRUE;    // Stop the loop
                printf("RECEIVED ACK aka RR...\n");
            } 
            // Resend the frame if NACK received
            else if (state == DONE) 
            {
                printf("RECEIVED NACK aka RREJ...\n");
                write(fd, msg, size);
                state = START;
            }
        }
    }
    return 0;  // Return success
}


////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////


// Function to process received data and handle byte stuffing return true when it must return ack, and false for nack
int receiveData(unsigned char *packet, int sn, size_t *size_read) 
{
    state = START;  
    unsigned char C_REPLY = (1-sn) << 7;
    unsigned char C_CONTROL = sn << 7;  // Construct control byte using sequence number
    unsigned int  BCC2 = 0, i = 0, stuffing = 0;  // Stuffing flag: set to 1 every time an ESC is encountered
    
    // Keep processing bytes until the end flag is received
    while(state != DONE) 
    {
        unsigned char received;
        unsigned int bytes = read(fd, &received, 1);

        if(bytes > 0) 
        {
            switch(state) 
            {
                case START:
                    if(received == FLAG) 
                    {
                        state = FLAG_RCV;
                    }
                    break;

                case FLAG_RCV:
                    if(received == FLAG) 
                    {
                        state = FLAG_RCV;
                    }
                    else if(received == A) 
                    {
                        state = A_RCV;
                    }
                    else 
                    {
                        state = START;
                    }
                    break;

                case A_RCV:
                    if(received == FLAG) 
                    {
                        state = FLAG_RCV;
                    }
                    else if(received == C_CONTROL) 
                    {
                        state = C_RCV;
                    }
                    // Handling case of receiving repeated message and waiting for the next one
                    else if(C_REPLY == received) 
                    {
                        return REPEATED_MESSAGE;  // Return code indicating a repeated message was received
                    }
                    else 
                    {
                        state = START;
                    }
                    break;

                case C_RCV:
                    if(received == FLAG) 
                    {
                        state = FLAG_RCV;
                    }
                    else if(received == BCC(A, C_CONTROL)) 
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
                    if(!stuffing) 
                    {
                        if(received == FLAG) 
                        {
                            state = DONE;
                            BCC2 = BCC(BCC2, packet[i-1]);
                            *size_read = i-1;  // Update the size of the read data
                            return (BCC2 == packet[i-1]);  // Return TRUE if BCC matches, otherwise FALSE
                        } 
                        else if(received == ESC) 
                        {
                            stuffing = TRUE; // Set the stuffing flag if ESC is encountered
                        }  
                        else 
                        {
                            BCC2 = BCC(BCC2, received);
                            packet[i] = received;
                            i++;
                        }
                    }
                    else 
                    {
                        stuffing = FALSE;  // Reset the stuffing flag
                        BCC2 = BCC(BCC2, received);
                        packet[i] = received;
                        i++;
                    }
                    break;
            }
        }
    }

    return FALSE;
}

//Reads data from the link layer and acknowledges the received data.
int llread(unsigned char *packet)
{
    int reply;
    size_t size_read;

    // Continuously try to receive data until successful
    while( (reply = receiveData(packet, sn, &size_read)) != TRUE)
    {
        // If the reply indicates an error (e.g., checksum mismatch)
        if(reply == 0)
        {
            printf("Sending NACK or RRej...\n");

            // Send a NACK (negative acknowledgment)
            unsigned char C_NACK = NACK(1-sn);
            unsigned char buf[] = {FLAG, A, C_NACK, BCC(A, C_NACK), F};
            write(fd, buf, 5);
        }
        // If the received message is a duplicate (e.g., retransmission)
        else
        {
            printf("Sending ACK because repeated message...\n");
            
            // Send an ACK to prevent further retransmissions
            unsigned char C_ACK = ACK(sn);
            unsigned char buf[] = {FLAG, A, C_ACK, BCC(A, C_ACK), F};
            write(fd, buf, 5);
        }
    }

    // Once data is received correctly, send an ACK
    printf("Sending ACK everything in order...\n");
    sn = 1-sn; // Toggle the sequence number
    unsigned char C_ACK = ACK(sn);
    unsigned char buf[] = {FLAG, A, C_ACK, BCC(A, C_ACK), F};
    write(fd, buf, 5);

    return size_read;
}

// LLCLOSE

//Determine the next state of the state machine based on the received byte.
void determineStateDISC(stateMachine *state, char byte)
{
    // Define the flags for the state transitions
    unsigned char A_FLAG = 0x03;
    unsigned char C_FLAG = 0x0B;

    // Determine the current state and process the incoming byte
    switch(*state)
    {
        //Waiting for the initial FLAG byte
        case START:
            if( byte == FLAG) *state = FLAG_RCV;
            break;

        //A FLAG byte has been received and waiting for the A_FLAG
        case FLAG_RCV:
            if(byte == FLAG) *state = FLAG_RCV;
            else if(byte == A_FLAG) *state = A_RCV;
            else *state = START;
            break;

        //A_FLAG has been received, waiting for C_FLAG
        case A_RCV:
            if(byte == FLAG) *state = FLAG_RCV;
            else if(byte == C_FLAG) *state = C_RCV;
            else *state = START;
            break;

        //C_FLAG has been received, waiting for BCC of A_FLAG and C_FLAG
        case C_RCV:
            if(byte == FLAG) *state = FLAG_RCV;
            else if(byte == BCC(A_FLAG, C_FLAG)) *state = BCC_NORMAL;
            else *state = START;
            break;
        //BCC for A_FLAG and C_FLAG received, waiting for final FLAG
        case BCC_NORMAL:
            if(byte == FLAG)  *state = DONE;
            else *state = START;
            break;
    }
}

// Closes the logical link layer communication.
int llclose(int statistics, LinkLayer linkLayer)
{
    int bytes = 0;
    unsigned char aux = 0;
    unsigned char msg[256] = {0};

    switch (linkLayer.role)
    {
        case LlTx:
            // Send DISC (Disconnect Frame)
            msg[0] =  FLAG;
            msg[1] = 0x03;
            msg[2] = 0x0B;
            msg[3] = BCC(0x03,0x0B);
            msg[4] = FLAG;
            bytes= write(fd, msg, 5);
            printf("Sent Disconnect Flag\n", msg, bytes);
            state = START;
            STOP = FALSE;

            // Wait for DISC acknowledgment
            while (STOP == FALSE)
            {   
                read(fd, &aux, 1);
                determineStateDISC(&state, aux);
                if(state == DONE) STOP = TRUE;
            }

            // Send UA (Unnumbered Acknowledgment Frame)
            unsigned char ua[256] = {0};
            ua[0] =  FLAG;
            ua[1] = 0x03;
            ua[2] = 0x07;
            ua[3] = BCC(0x03,0x07);
            ua[4] = FLAG;
            int bytes= write(fd, ua, UA_SIZE);
            printf("Sent UA\n");
            break;

        case LlRx:
            STOP = FALSE;
            state = START;

            // Wait for DISC from transmitter
            while (STOP == FALSE)
            {   
                read(fd, &aux, 1);
                determineStateDISC(&state, aux);

                if(state == DONE) STOP = TRUE;
            }
            printf("Received DISC\n");

            // Send DISC as acknowledgment
            msg[0] =  FLAG;
            msg[1] = 0x03;
            msg[2] = 0x0B;
            msg[3] = BCC(0x03,0x0B);
            msg[4] = FLAG;
            bytes= write(fd, msg, 5);

            // Wait for UA acknowledgment from transmitter
            state = START;
            STOP = FALSE;
             while (STOP == FALSE)
            {   
                read(fd, &aux, 1);
                determineState(&state, aux, 1);

                if(state == DONE) STOP = TRUE;
            }
             printf("Received UA\n");
            break;
    }

    // Restore old terminal settings
    if (tcsetattr(fd,TCSANOW,&oldtio) != 0)
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
