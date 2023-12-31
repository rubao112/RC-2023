// Write to serial port in non-canonical mode
//
// Modified by: Eduardo Nuno Almeida [enalmeida@fe.up.pt]

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>

// Baudrate settings are defined in <asm/termbits.h>, which is
// included by <termios.h>
#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source

#define FALSE 0
#define TRUE 1

#define BUF_SIZE 256

volatile int STOP = FALSE;

#define FALSE 0
#define TRUE 1

#define FLAG 0x7E
#define A 0x03
#define C0 0x00
#define C1 0x40

int alarmEnabled = FALSE;
int alarmCount = 0;

int readCounter = 0;

// Alarm function handler
void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;

    printf("Alarm #%d\n", alarmCount);
}

int llwrite(int fd, char * buffer, int length){
    unsigned char buf[BUF_SIZE] = {0};

    buf[0] = 0x7E;
    buf[1] = 0x03;
    buf[2] = 0x03;
    buf[3] = buf[1] ^ buf[2];
    buf[4] = 0x7E;

    /*
    for (int i = 0; i < BUF_SIZE; i++)
    {
        buf[i] = 'a' + i % 26;
    }
    */

    // In non-canonical mode, '\n' does not end the writing.
    // Test this condition by placing a '\n' in the middle of the buffer.
    // The whole buffer must be sent even with the '\n'.
    // buf[5] = '\n';

    int readsCounter = 0;

    int notRead = TRUE;

    (void)signal(SIGALRM, alarmHandler);

    while (readsCounter < 3 && notRead)
    {
        if (alarmEnabled == FALSE)
        {
            int bytes = write(fd, buf, BUF_SIZE);
            printf("%d bytes written\n", bytes);

            // Wait until all bytes have been written to the serial port
            sleep(1);
            alarm(3); // Set alarm to be triggered in 3s
            alarmEnabled = TRUE;
        }

        unsigned char buf2[BUF_SIZE + 1] = {0};
        int i = 0;
        int bytes2 = read(fd, buf2, 1);
        if (bytes2 != 1){
            readsCounter++;
            continue;
        }

        while (buf2[0] != 0x7E)
        {
            bytes2 = read(fd, buf2, 1);
        }
        do
        {
            i++;
            bytes2 = read(fd, buf2, 1);
        } while (buf2[i] != 0x7E);
        buf2[i + 1] = '\0';
        printf(":%s:%d\n", buf, i + 1);

        printf("var = 0x%02X\n", buf2[2]);
        readsCounter++;
        notRead = FALSE;
    }

    alarm(0);
}

int main(int argc, char *argv[])
{
    // Program usage: Uses either COM1 or COM2
    const char *serialPortName = argv[1];

    if (argc < 2)
    {
        printf("Incorrect program usage\n"
               "Usage: %s <SerialPort>\n"
               "Example: %s /dev/ttyS1\n",
               argv[0],
               argv[0]);
        exit(1);
    }

    // Open serial port device for reading and writing, and not as controlling tty
    // because we don't want to get killed if linenoise sends CTRL-C.
    int fd = open(serialPortName, O_RDWR | O_NOCTTY);

    if (fd < 0)
    {
        perror(serialPortName);
        exit(-1);
    }

    struct termios oldtio;
    struct termios newtio;

    // Save current port settings
    if (tcgetattr(fd, &oldtio) == -1)
    {
        perror("tcgetattr");
        exit(-1);
    }

    // Clear struct for new port settings
    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    // Set input mode (non-canonical, no echo,...)
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 2; // Inter-character timer unused
    newtio.c_cc[VMIN] = 1;  // Blocking read until 5 chars received

    // VTIME e VMIN should be changed in order to protect with a
    // timeout the reception of the following character(s)

    // Now clean the line and activate the settings for the port
    // tcflush() discards data written to the object referred to
    // by fd but not transmitted, or data received but not read,
    // depending on the value of queue_selector:
    //   TCIFLUSH - flushes data received but not read.
    tcflush(fd, TCIOFLUSH);

    // Set new port settings
    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure set\n");
    char fake[10]={0x00,0x7E,0x05,0x7D, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16};

    llwrite(1, fake, 10);

    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    return 0;
}
