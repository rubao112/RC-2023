#ifndef CLIENT_TCP_H
#define CLIENT_TCP_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

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

#endif // CLIENT_TCP_H
