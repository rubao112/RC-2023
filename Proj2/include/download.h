#ifndef DOWNLOAD_H
#define DOWNLOAD_H
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <termios.h>
#include <ctype.h>
#include <netinet/in.h>
#include <regex.h>

#define FTP_PORT 21
#define SIZE_MAX 512

/* Server responses */
#define SERVER_PASSIVE 227
#define SERVER_LOGINREADY 220
#define SERVER_LOGINSUCCESS 230
#define SERVER_PASSREADY 331
#define SERVER_TRANSFER_COMPLETE 226
#define SERVER_TRANSFERREADY 150
#define SERVER_GOODBYE 221

/* Default login for case 'ftp://<host>/<url-path>' */
#define DEFAULT_USER "anonymous"
#define DEFAULT_PASSWORD "anonymous"

/* Parser output */
struct URL
{
    char host[SIZE_MAX];
    char user[SIZE_MAX];
    char password[SIZE_MAX];
    char resource[SIZE_MAX]; 
    char file[SIZE_MAX];
    char ip[SIZE_MAX];
};

/* Machine states for server response */
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
 * @param password The password for authentication.
 * @param user The username for authentication.
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
 * @param port Pointer to store data connection port.
 * @param ip Buffer for the data connection ip.
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
 * @param socket1 File descriptor for the control connection.
 * @param socket2 File descriptor for the data connection.
 * @param filename Name of the file for saving the resource.
 * @return Server´s response code.
 */
int getResource(const int socket1, const int socket2, char *filename);

/**
 * Closes FTP connection and all sockets associated to it.
 * @param socket1 File descriptor for the control connection.
 * @param socket2 File descriptor for the for the data connection.
 * @return 0 if success, else -1 on error.
 */
int closeConnection(const int socket1, const int socket2);

#endif