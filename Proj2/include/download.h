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

/**
 * Creates a socket and connects to the specified port and IP address.
 * @param ip Specified IP Adress for connection.
 * @param port Specified port for connection.
 * @return File descriptor for the server connection if success, else -1 on error.
 */
int createSocket(char *ip, int port);

/**
 * Authenticates the connection with the given user credentials.
 * @param socket File descriptor for the server connection.
 * @param user The username for authentication.
 * @param pass The password for authentication.
 * @return Server´s response code.
 */
int loginConnection(const int socket, const char *password, const char *user);

/**
 * Reads server's response.
 * @param socket File descriptor for the server connection.
 * @param buffer Buffer for the server's response.
 * @return Server´s response code.
 */
int readResponse(const int socket, char *buffer);

/**
 * Swaps to passive mode and parses the response from the server for data connection details.
 * @param socket File descriptor for the server connection.
 * @param ip Buffer for the data connection ip.
 * @param port Pointer to store data connection port.
 * @return Server´s response code.
 */
int passive(const int socket, int *port, char *ip);

/**
 * Sends server request for specified resource.
 * @param socket File descriptor for the server connection.
 * @param resource Name of the specified resource.
 * @return Server´s response code.
 */
int requestResource(const int socket, char *resource);

/**
 * Gets and saves the requested resource from the server into a file.
 * @param socketA File descriptor for the control connection.
 * @param socketB File descriptor for the data connection.
 * @param filename Name of the file for saving the resource.
 * @return Server´s response code.
 */
int getResource(const int socketA, const int socketB, char *filename);

/**
 * Closes FTP connection and all sockets associated to it.
 * @param socketA File descriptor for the control connection.
 * @param socketB File descriptor for the for the data connection.
 * @return 0 if success, else -1 on error.
 */
int closeConnection(const int socketA, const int socketB);


#endif