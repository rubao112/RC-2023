#include "../include/download.h"
#include "../include/getIp.h"
#include "../include/clientTCP.h"


int getIP(const char *host, struct URL *url)
{
    struct hostent *h;
    if ((h = gethostbyname(host)) == NULL)
    {
        herror("gethostbyname()");
        return -1;
    }
    strcpy(url->ip, inet_ntoa(*((struct in_addr *)h->h_addr)));
    return 0;
}

int getFileName(struct URL *url)
{
    char *token = strrchr(url->resource, '/');
    if (token != NULL)
    {
        strcpy(url->file, token + 1);
    }
    else
    {
        // Handle the case where no '/' is present in the resource path
        strcpy(url->file, url->resource);
    }
    return 0;
}

int parse(char *input, struct URL *url)
{
    char *ftp = strtok(input, "//");
    if (strcmp(ftp, "ftp:") != 0)
    {
        printf("Error: Not using ftp protocol\n");
        return -1;
    }

    char *urlrest = strtok(NULL, "/");
    char *path = strtok(NULL, "");

    char *user = strtok(urlrest, ":");
    char *pass = strtok(NULL, "@");

    if (pass == NULL)
    {
        strcpy(url->user, "anonymous");
        strcpy(url->password, "password");
        strcpy(url->host, urlrest);
    }
    else
    {
        strcpy(url->user, user);
        strcpy(url->password, pass);
        strcpy(url->host, strtok(NULL, ""));
    }

    if (path != NULL)
    {
        strcpy(url->resource, path);
    }
    else
    {
        url->resource[0] = '\0'; // Empty string if path is not present
    }

    if (getIP(url->host, url) != 0)
    {
        printf("Error: Unable to resolve IP address\n");
        return -1;
    }

    if (getFileName(url) != 0)
    {
        printf("Error: Unable to extract file name\n");
        return -1;
    }

    return 0;
}