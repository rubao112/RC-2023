#ifndef CLIENT_TCP_H
#define CLIENT_TCP_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

/**
 * Creates a socket and connects to the specified IP address and port.
 * @param ip The IP address to connect to.
 * @param port The port number to connect to.
 * @return The socket file descriptor on success, or -1 on error.
 */
int createSocket(char *ip, int port);

/**
 * Authenticates the connection with the given user credentials.
 * @param socket The socket file descriptor.
 * @param user The username for authentication.
 * @param pass The password for authentication.
 * @return The server response code.
 */
int authConn(const int socket, const char *user, const char *pass);

/**
 * Switches the FTP connection to passive mode and parses the server's response for data connection details.
 * @param socket The socket file descriptor.
 * @param ip A buffer to store the parsed IP address for data connection.
 * @param port A pointer to store the parsed port number for data connection.
 * @return The server response code.
 */
int passiveMode(const int socket, char *ip, int *port);

/**
 * Reads and parses the server's response.
 * @param socket The socket file descriptor.
 * @param buffer A buffer to store the server's response.
 * @return The server response code.
 */
int readResponse(const int socket, char *buffer);

/**
 * Sends a request to the server to retrieve a specific resource.
 * @param socket The socket file descriptor.
 * @param resource The name of the resource to request.
 * @return The server response code.
 */
int requestResource(const int socket, char *resource);

/**
 * Retrieves the requested resource from the server and saves it to a file.
 * @param socketA The socket file descriptor for the control connection.
 * @param socketB The socket file descriptor for the data connection.
 * @param filename The name of the file to save the resource to.
 * @return The server response code.
 */
int getResource(const int socketA, const int socketB, char *filename);

/**
 * Closes the FTP connection and associated sockets.
 * @param socketA The socket file descriptor for the control connection.
 * @param socketB The socket file descriptor for the data connection.
 * @return 0 on success, -1 on error.
 */
int closeConnection(const int socketA, const int socketB);

#endif // CLIENT_TCP_H
