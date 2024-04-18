/* File: client.c
 * Authors: Bryce Bejlovec and Owen Sapp
 * Threaded client for a chat service that can chat with multiple users with file transport capability.
*/

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <pthread.h>
#include <stdbool.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>


#define MESSAGE_LEN 1024
#define SEND_F_FLAGS O_RDONLY
#define RECV_F_FLAGS O_CREAT | O_WRONLY | O_EXCL
#define RECV_UH S_IRUSR | S_IWUSR

int g_keepgoing = 1;
int server_socket, client_socket;
pthread_t rthread,sthread;

char server_ip[INET6_ADDRSTRLEN];

void signal_handler(int sig){
	printf( "\nCtrl-C pressed, closing socket and exiting...\n" );
	g_keepgoing = 0;
    close(server_socket);
}

void * client_receive_thread(){
    char buf[MESSAGE_LEN] = {0};
    int received = 0;
    printf("Started RECV thread\n");
    while (g_keepgoing) {

        received = read(server_socket, buf, MESSAGE_LEN - 1);
        
        buf[received] = 0;
        if (received <= 0) {
            printf("Lost Connection from server\n");
            g_keepgoing = 0;
            break;
        }
        
        printf("Response from server:\n     \"%s\"\n", buf);
    } // Receive incoming data from server.
    
    close(server_socket);  

}

void * client_send_thread(){
    char (*buf) = malloc(sizeof(char[MESSAGE_LEN]));
    int written = 0;
    unsigned long len = MESSAGE_LEN;
    while (g_keepgoing) {

        written = getline(&buf, &len, stdin) - 1;
        buf[written] = 0;
        if(strncmp(buf,"uTake ",6) == 0) {
            char (*pathbuf) = malloc(sizeof(char[MESSAGE_LEN]));
            strcpy(pathbuf,"./upload/");
            strcat(pathbuf, buf + 6);
            
            
            FILE* file = fopen(pathbuf, "rb");

            if (file == NULL) {
                printf("File does not exist. Please try Again.\n");
                continue;
            }
            strcpy(pathbuf, buf + 6);
            printf("my\n");

            printf("What directory on the server would you like to save this file to?\n");
            written = getline(&pathbuf, &len, stdin) - 1;
            pathbuf[written] = '\0';
            strcat(pathbuf, buf + 6);

            buf[6] = 0;
            strcat(buf, pathbuf);
            buf[6+strlen(pathbuf)] = 0;
            free(pathbuf);

            pthread_cancel(rthread);

            if (send(server_socket, buf, strlen(buf),0) < 0) {
                printf("Failed to send to server\n");
                break;
            } // Break if connection lost
            

            int received = read(server_socket, buf, MESSAGE_LEN - 1);

            if (received <= 0) {
                printf("Lost Connection from server\n");
                g_keepgoing = 0;
                break;
            }

            buf[received] = 0;

            if (strcmp(buf,"R") != 0) {
                printf("Received from server: %s\n",buf);
            } else {
                long fileread = 0;
                while (fileread = fread(buf, 1, MESSAGE_LEN, file)) {
                    if (send(server_socket, buf, fileread,0) < 0) {
                        printf("Failed to send to server\n");
                        break;
                    } // Break if connection lost
                    memset(buf, 0, MESSAGE_LEN);
                }
                // if (send(server_socket, filesent, strlen(filesent),0) < 0) {
                //     printf("Failed to send to server\n");
                //     break;
                // } // Break if connection lost
                printf("File upload complete.\n");
            }
            fclose(file);

            int threadreturn = 0;
            if (threadreturn = pthread_create(&rthread, NULL, client_receive_thread, NULL)) {
                printf("ERROR: Return Code from pthread_create() is %d\n", threadreturn);
                perror("ERROR creating thread");
                break;
            }
        }

        if(strncmp(buf,"iWant ",6) == 0) {
            char (*pathbuf) = malloc(sizeof(char[MESSAGE_LEN]));
            char (*filename) = malloc(sizeof(char[MESSAGE_LEN]));

            strcpy(filename, buf + 6);

            printf("What directory on the server would you like to download this file from?\n");
            written = getline(&pathbuf, &len, stdin) - 1;
            pathbuf[written] = '\0';
            strcat(pathbuf, filename);

            buf[6] = 0;
            strcat(buf, pathbuf);
            buf[6+strlen(pathbuf)] = 0;

            pthread_cancel(rthread);

            if (send(server_socket, buf, strlen(buf),0) < 0) {
                printf("Failed to send to server\n");
                break;
            } // Break if connection lost
            

            int received = read(server_socket, buf, MESSAGE_LEN - 1);

            if (received <= 0) {
                printf("Lost Connection from server\n");
                g_keepgoing = 0;
                break;
            }

            buf[received] = 0;

            if (strcmp(buf,"R") != 0) {
                printf("Received from server: %s\n",buf);
            } else {
                memset(buf, 0, MESSAGE_LEN);
                printf("File found! What directory would you like to save '%s' to? Leave blanks for default.\n", filename);
reenterdir:
                written = getline(&pathbuf, &len, stdin) - 1;
                pathbuf[written] = '\0';

                if (strlen(pathbuf) > 1) {
                    struct stat pathstat;
                    if (stat(pathbuf, &pathstat) == -1) {
                        mkdir(pathbuf, 0700);
                    }
                    strcat(buf, pathbuf);
                } else {
                    strcpy(buf, "./received_files/");
                }
                strcat(buf, filename);
                int file = open(buf, RECV_F_FLAGS, RECV_UH);
                if (file < 0) {
                    if (errno == EEXIST) {
yesnoagain:
                        printf("A file with the same name already exists in that directory! Overwrite? (Y/n)");
                        written = getline(&buf, &len, stdin) - 1;
                        if (buf[0] == 'Y'){
                            strcpy(buf, pathbuf);
                            strcat(buf, filename);
                            file = open(buf, O_WRONLY);
                        } else if (tolower(buf[0]) == 'n') {
                            printf("Please enter new directory to save '%s' to.\n", filename);
                            goto reenterdir;
                        } else {
                            goto yesnoagain;
                        }
                    }
                }

                if (send(server_socket, "R", 1,0) < 0) {
                    printf("Failed to send to server\n");
                    break;
                } // Break if connection lost

                memset(buf, 0, MESSAGE_LEN);

                received = read(server_socket, buf, MESSAGE_LEN);
                while (received == MESSAGE_LEN) {
                    write(file, buf, received);
                    memset(buf, 0, MESSAGE_LEN);
                    received = read(server_socket, buf, MESSAGE_LEN);
                }
                write(file, buf, received);
                printf("Download complete!");
                free(pathbuf);
                free(filename);
            }
            
            continue;
            int threadreturn = 0;
            if (threadreturn = pthread_create(&rthread, NULL, client_receive_thread, NULL)) {
                printf("ERROR: Return Code from pthread_create() is %d\n", threadreturn);
                perror("ERROR creating thread");
                break;
            }
        }

        if(strcmp(buf, ";;;") == 0){
            g_keepgoing = 0;
            printf("User entered sentinel of \";;;\", now stopping client\n");
        }

        if (send(server_socket, buf, written,0) < 0) {
            printf("Failed to send to server\n");
            break;
        } // Break if connection lost
           
    } // Read client input and send to server
    free(buf);
    close(server_socket);

}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[]) {
    int client_len;
    unsigned short port;
    struct addrinfo server_address, *server_info, *probe;
    char host_ip[100] = {0};
    int retval = 0;
    int threadreturn;
    int clientcount = 0;

    if (argc != 3) {
        printf("Usage: ./client <SERVER IP> <PORT>\n");
        return 1;
    }
    printf("%d\n",argc);


    signal(SIGINT, signal_handler);

    port = (unsigned short)atoi(argv[2]);
    

    memset(&server_address, 0, sizeof(server_address));
    server_address.ai_family = AF_UNSPEC;
    server_address.ai_socktype = SOCK_STREAM; 

    if ((retval = getaddrinfo(argv[1], argv[2], &server_address, &server_info)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(retval));
		return 1;
	}

    // loop through all the results and connect to the first we can
	for (probe = server_info; probe != NULL; probe = probe->ai_next) {
		if ((server_socket = socket(probe->ai_family, probe->ai_socktype,
				probe->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}

		if (connect(server_socket, probe->ai_addr, probe->ai_addrlen) == -1) {
			perror("client: connect");
			close(server_socket);
			continue;
		}

		break;
	}

	if (probe == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		return 2;
	}

	inet_ntop(probe->ai_family, get_in_addr((struct sockaddr *)probe->ai_addr),
			server_ip, sizeof(server_ip));
    
    printf("Connecting to host at %s:%s\n",server_ip,argv[2]);


    // Init receive and send threads
    if (threadreturn = pthread_create(&rthread, NULL, client_receive_thread, NULL)) {
            printf("ERROR: Return Code from pthread_create() is %d\n", threadreturn);
            perror("ERROR creating thread");
            exit(1);
    }

    if (threadreturn = pthread_create(&sthread, NULL, client_send_thread, NULL)) {
            printf("ERROR: Return Code from pthread_create() is %d\n", threadreturn);
            perror("ERROR creating thread");
            exit(1);
    }

    freeaddrinfo(server_info); // all done with this structure

    while (g_keepgoing) { 
    }
        
    close(server_socket);
    pthread_cancel(rthread);
    pthread_cancel(sthread);
    pthread_exit(0);
}