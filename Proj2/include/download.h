#ifndef DOWNLOAD_H
#define DOWNLOAD_H
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <regex.h>
#include <termios.h>

#define MAX_LENGTH 500
#define FTP_PORT 21

/* Server responses */
#define SV_READY4AUTH 220
#define SV_READY4PASS 331
#define SV_LOGINSUCCESS 230
#define SV_PASSIVE 227
#define SV_READY4TRANSFER 150
#define SV_TRANSFER_COMPLETE 226
#define SV_GOODBYE 221

/* Default login for case 'ftp://<host>/<url-path>' */
#define DEFAULT_USER "anonymous"
#define DEFAULT_PASSWORD "password"

/* Parser output */
struct URL
{
    char host[MAX_LENGTH];     // 'ftp.up.pt'
    char resource[MAX_LENGTH]; // 'parrot/misc/canary/warrant-canary-0.txt'
    char file[MAX_LENGTH];     // 'warrant-canary-0.txt'
    char user[MAX_LENGTH];     // 'username'
    char password[MAX_LENGTH]; // 'password'
    char ip[MAX_LENGTH];       // 193.137.29.15
};

/* Machine states that receives the response from the server */
typedef enum
{
    START,
    SINGLE,
    MULTIPLE,
    END
} ResponseState;

#endif