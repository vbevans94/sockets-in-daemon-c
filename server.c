#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <syslog.h>
#include <stdlib.h>

#define DISTRIBUTE 0
#define FINDER     1

#define START 0
#define STOP  1
#define CHECK 2

#define PID_FILE  "pid"

int ports[100]; // finder servers
int sockets[100]; // finder sockets

int length; // finder count

int client_sock;

char* send_to(int port, char* message);
char* find(char* line);

void signal_handler(int sig) {
    switch(sig) {
        case SIGTERM:
            // free resources
            for (int i = 0; i < length; i++) {
                close(sockets[i]);
            }
            unlink(PID_FILE);
            exit(0);
            break;
    }
}
 
int main(int argc, char *argv[]) {
    if (argc < 2) {
    	printf("usage: %s <0: start, 1: stop, 2: check running><port>", argv[0]);
    	exit(1);
    }

    int action = atoi(argv[1]);

    FILE *file; // pid file
    if (action == STOP) {
        if (file = fopen(PID_FILE, "r"))
        {
            puts("stopping");
            // stop server if running
            int running;
            fscanf(file, "%d", &running);
            char* command[100];
            sprintf(command, "kill %d", running);
            system(command);
            fclose(file);
        } else {
            puts("not running");
        }
        return 0;
    } else if (action == CHECK) {
        if (file = fopen(PID_FILE, "r"))
        {
            puts("running");
            fclose(file);
        } else {
            puts("not running");
        }
        return 0;
    }

    // Make daemon
    pid_t pid;

    /* Fork off the parent process */
    pid = fork();
    /* An error occurred */
    if (pid < 0)
        exit(EXIT_FAILURE);

    /* Success: Let the parent terminate */
    if (pid > 0)
        exit(EXIT_SUCCESS);

    /* On success: The child process becomes session leader */
    if (setsid() < 0)
        exit(EXIT_FAILURE);

    /* handle signals */
    signal(SIGTERM, signal_handler);

    /* Fork off for the second time*/
    pid = fork();

    /* An error occurred */
    if (pid < 0)
        exit(EXIT_FAILURE);

    /* Success: Let the parent terminate */
    if (pid > 0)
        exit(EXIT_SUCCESS);

    /* Set new file permissions */
    umask(0);
    /* Open the log file */
    openlog ("server", LOG_PID, LOG_DAEMON);

    syslog (LOG_NOTICE, "Server started.");

    // let monitor know we are started
    FILE *f = fopen(PID_FILE, "w");
    if (f == NULL) {
        printf("Error opening file!\n");
        exit(1);
    }

    /* print my pid */
    fprintf(f, "%d\n", getpid());

    fclose(f);
    
    int port = atoi(argv[2]); // distributor port
    
	// get ports for every finder server
	file = fopen("config", "r");
	int i = 0;
	int num;
	while (fscanf(file, "%d", &num) > 0) {
	    ports[i++] = num;
	}
	fclose(file);
	
	// start finder processes
	length = i;
    int number = 0;
	for (i = 0; i < length; i++) {
        pid = fork();
        if (pid) {
            continue;
        } else if (pid == 0) {
            number += i;
            break;
        } else {
            printf("fork error\n");
            exit(1);
        }
    }

    
    if (pid == 0) {
        // child process idling
        receive(FINDER, ports[number]);
    } else if (pid > 0) {
        // parent process
        receive(DISTRIBUTE, port);
    }
    
    return 0;
}

int receive(int mode, int port) {
	int socket_desc, c, read_size;
    struct sockaddr_in server, client;
    char client_message[2000];
    //Create socket
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_desc == -1) {
        printf("Could not create socket");
    }
     
    //Prepare the sockaddr_in structure
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(port);
     
    //Bind
    if (bind(socket_desc, (struct sockaddr *)&server, sizeof(server)) < 0) {
        //print the error message
        perror("bind failed. Error");
        return 1;
    }
    
    
    listen(socket_desc, 3);
    c = sizeof(struct sockaddr_in);
    client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c);

    if (client_sock < 0) {
        perror("accept failed");
        return 1;
    }

    // in distribute mode we create socket for each finder to not do this all the time
    if (mode == DISTRIBUTE) {
        struct sockaddr_in server;
        for (int i = 0; i < length; i++) {
            //Create socket
            sockets[i] = socket(AF_INET, SOCK_STREAM, 0);
            if (sockets[i] == -1) {
                printf("Could not create socket");
            }
             
            server.sin_addr.s_addr = inet_addr("127.0.0.1");
            server.sin_family = AF_INET;
            server.sin_port = htons(ports[i]);

            //Connect to remote server
            if (connect(sockets[i], (struct sockaddr *)&server, sizeof(server)) < 0) {
                perror("connect failed. Error");
                return 1;
            }
        }
    }
    
    //Receive a message from client
    while ((read_size = recv(client_sock, client_message, 2000, 0)) > 0) {
        //Send the message back to client
        char* response;
        if (mode == DISTRIBUTE) {
        	// we have to propagate request to finder servers
        	for (int i = 0; i < length; i++) {
        		response = send_to(sockets[i], client_message);
        		write(client_sock, response, strlen(response));
        	}
        } else {
        	// search for string in files
        	response = find(&client_message);
        	write(client_sock, response, strlen(response));
        }
    }
     
    if (read_size == 0) {
        fflush(stdout);
    } else if (read_size == -1) {
        perror("recv failed");
    }

    return 0;
}

char* send_to(int sock, char* message) {
    char server_reply[2000];
    //Send some data
	if (send(sock, message, strlen(message), 0) < 0) {
		puts("Send failed");
		return "";
	}
	 
	//Receive a reply from the server
	if (recv(sock, server_reply, 2000, 0) < 0) {
		puts("recv failed");
	}
	
	return server_reply;
}

char* find(char* line) {
	FILE *fp;
	char path[1035];

	/* Open the command for reading. */
	char* command[100];
	sprintf(command, "/usr/bin/grep -s %s server.c", line);

	fp = popen(command, "r");
	if (fp == NULL) {
		printf("Failed to run command\n" );
		exit(1);
	}

	char* result[7000];
	while (fgets(path, sizeof(path) - 1, fp) != NULL) {
		strcat(result, "\n");
		strcat(result, path);
	}

	/* close */
	pclose(fp);
	
	return result;
}
