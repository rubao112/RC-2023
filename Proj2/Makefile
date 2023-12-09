CC = gcc
CFLAGS = -Wall

SRC = src/
INCLUDE = include/
BIN = bin/

# Source files
SOURCES = $(SRC)/download.c $(SRC)/getIp.c $(SRC)/clientTCP.c

.PHONY: downloader
downloader: $(SOURCES)
	$(CC) $(CFLAGS) -o download $^

.PHONY: clean
clean:
	rm -rf download
