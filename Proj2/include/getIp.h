#ifndef GET_IP_H
#define GET_IP_H

#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include "download.h"

/**
 * Resolves the IP address from a given hostname.
 * @param host The hostname to be resolved.
 * @param url The URL struct where the resolved IP address will be stored.
 * @return 0 on success, -1 on failure.
 */
int getIP(const char *host, struct URL *url);

/**
 * Extracts the file name from the resource path in the URL struct.
 * @param url The URL struct containing the resource path.
 * @return 0 on success, -1 on failure.
 */
int getFileName(struct URL *url);

/**
 * Parses the input FTP URL and populates the URL struct with its components.
 * @param input The FTP URL to be parsed.
 * @param url The URL struct where the parsed components will be stored.
 * @return 0 on success, -1 on failure.
 */
int parse(char *input, struct URL *url);

#endif // GET_IP_H
