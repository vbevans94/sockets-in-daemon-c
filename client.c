#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
 
int main(int argc , char *argv[]) {
	if (argc < 2) {
		printf("usage: %s <server_port>", argv[0]);
		exit(1);
	}

    int sock;
    struct sockaddr_in server;
    char message[1000], server_reply[2000];
     
    //Create socket
    sock = socket(AF_INET , SOCK_STREAM , 0);
    if (sock == -1) {
        printf("Could not create socket");
    }
    puts("Socket created");
    
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_family = AF_INET;
    server.sin_port = htons(atoi(argv[1]));
 
    //Connect to remote server
    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("connect failed. Error");
        return 1;
    }
     
    //keep communicating with server
    while (1) {
        printf("Enter string to be searched: ");
        scanf("%s", message);
         
        //Send some data
        if (send(sock, message, strlen(message), 0) < 0) {
            puts("Send failed");
            return 1;
        }
         
        //Receive a reply from the server
        if (recv(sock, server_reply, 2000, 0) < 0) {
            puts("recv failed");
            break;
        }
        
        puts("Server reply :");
        puts(server_reply);
    }
     
    close(sock);
    return 0;
}
