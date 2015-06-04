CC=gcc

all: client server

server: server.c
	$(CC) server.c -o server

client: client.c
	$(CC) client.c -o client