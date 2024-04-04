/* File: client.c
 * Author: Bryce Bejlovec
 * Threaded client for a chat service that can chat with multiple users.
*/

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


#define MESSAGE_LEN 1024

int g_keepgoing = 1;
int server_socket, client_socket;

char server_ip[INET6_ADDRSTRLEN];

void signal_handler(int sig){
	printf( "\nCtrl-C pressed, closing socket and exiting...\n" );
	g_keepgoing = 0;
    close(server_socket);
}

void * client_receive_thread(){
    char buf[MESSAGE_LEN] = {0};
    int received = 0;
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
        buf[written] = '\0';
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
    pthread_t rthread,sthread;
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