/* File: server.c
 * Author: Bryce Bejlovec
 * Threaded server for a chat service that can host multiple users
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
//#include <stdbool.h>
#include <netdb.h>
#include "proxy_parse.h"

typedef int bool;
#define false 0
#define true 1



#define MESSAGE_LEN 1024
#define MAX_CLIENTS 5

int g_keepgoing = 1;
int clients[MAX_CLIENTS] = {0};
char freeclients[MAX_CLIENTS] = {0};
int received[MAX_CLIENTS] = {0};
char (*client_bufs)[MAX_CLIENTS][MESSAGE_LEN] = NULL;
int server_socket, client_socket;
bool sendbool[MAX_CLIENTS] = {false};
pthread_mutex_t client_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t buf_lock[MAX_CLIENTS];
pthread_cond_t buf_cond[MAX_CLIENTS];
pthread_t rthreads[MAX_CLIENTS];
pthread_t sthreads[MAX_CLIENTS];

char address[MESSAGE_LEN];

void signal_handler(int sig){
	printf( "\nCtrl-C pressed, closing socket and exiting...\n" );
	g_keepgoing = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (freeclients[i]){
            close(clients[i]);
            freeclients[i] = 0;
        }
        pthread_cancel(rthreads[i]);
        pthread_cancel(sthreads[i]);
    } // Close all client sockets and kill threads

    close(server_socket);
}

void * server_receive_thread(void * clinum){
    int client_num = (int) clinum;
    char* buf = (*client_bufs)[client_num];
    while (g_keepgoing) {

        pthread_mutex_lock(buf_lock + client_num);
        received[client_num] = read(clients[client_num], buf, MESSAGE_LEN - 1);
        
        if (received[client_num] <= 0) {
            pthread_mutex_unlock(buf_lock + client_num);
            printf("Lost Connection from client %d\n",client_num);
            break;
        }
        
        struct ParsedRequest *req = ParsedRequest_create();
        if (ParsedRequest_parse(req, buf, strlen(buf)) < 0) {
            printf("parse failed\n");
            return -1;
        }
        printf("Recieved from client:\n");
        printf("Method:%s\n", req->method);
        printf("Host:%s\n", req->host);
        printf("Protocol:%s\n", req->protocol);
        printf("Path:%s\n",req->path);
        printf("Version:%s\n",req->version);
        
        //check if desired file exists

        //connect to website
        char web_port[5] = "80";
        if(!strcmp("http",req->protocol)){
            strcpy(web_port,"80");
        }
        struct addrinfo website_hints,*website, *site;
        int website_sockfd, rv;
        memset(&website_hints, 0, sizeof(website_hints));
        website_hints.ai_family = AF_UNSPEC;
        website_hints.ai_socktype = SOCK_STREAM;
    
        if((rv = getaddrinfo(req->host, web_port, &website_hints, &website)) != 0){
            printf("getaddrinfo: %s \n", gai_strerror(rv));
            return 1;
        }
        for(site = website; site!= NULL; site = site->ai_next){
            if ((website_sockfd = socket(site->ai_family, site->ai_socktype, site->ai_protocol)) == -1){
                perror("extern site socket");
            }
            if (connect(website_sockfd, site->ai_addr, site->ai_addrlen) == -1){
                perror("extern site connect");
            }
            break;
        }
        if(site == NULL){
            printf("failed to connect to external client\n");
            return 2;
        }
        printf("Connection with host %s successful on port %s\n",req->host, web_port);

        //send website the get request (buf)
        memset(buf,0,MESSAGE_LEN);
        //printf("1BUF: %s\n",buf);
        strcat(buf, "GET ");
        //printf("2BUF: %s\n",buf);
        strcat(buf, req->path);
        //printf("3BUF: %s\n",buf);
        //strcat(buf, " ");
        //printf("4BUF: %s\n",buf);
        //strcat(buf, req->version);
        printf("Sending to external website %s\n",buf);
        if (send(website_sockfd, buf, sizeof(buf), 0) < 0){
            printf("Failed to send GET Request to external website\n");
            continue;
        }

        //receive back the get request
        received[client_num] = read(website_sockfd,buf,MESSAGE_LEN);
        if (received[client_num] <= 0) {
            sendbool[client_num] = true;
            pthread_cond_broadcast(buf_cond + client_num);
            pthread_mutex_unlock(buf_lock + client_num);
            printf("Lost Connection from client %d\n",client_num);
            break;
        }
        printf("received from website: \n");
        printf("%s\n",buf);
        //save contents received to file (store on timelimit?)

        //send get request back to client
        if (send(clients[client_num], buf, sizeof(buf), 0) < 0){
            printf("Failed to send GET response to client\n");
            continue;
        }

        sendbool[client_num] = true;
        
        pthread_cond_broadcast(buf_cond + client_num);
        pthread_mutex_unlock(buf_lock + client_num);
    } // continue to receive client data and convert to uppercase
    
    pthread_mutex_lock(&client_lock);
    close(clients[client_num]);
    freeclients[client_num] = 0;
    pthread_mutex_unlock(&client_lock);
    // Free client slot.

    pthread_cancel(rthreads[client_num]);
}

void * server_send_thread(void * clinum){
    int client_num = (int) clinum;
    char* buf = (*client_bufs)[client_num];
    while (g_keepgoing && freeclients[client_num]) {
        while(!sendbool[client_num]);
        pthread_mutex_lock(buf_lock + client_num);

        while (!sendbool[client_num]) {
            pthread_cond_wait(buf_cond + client_num, buf_lock + client_num);
        }

        sendbool[client_num] = false;
        if (!(freeclients[client_num] || g_keepgoing)) {
            pthread_mutex_unlock(buf_lock + client_num);
            break;
        }

        pthread_mutex_lock(&client_lock);
        for (int i = 0; i < MAX_CLIENTS; i++){
            if (freeclients[i]){
                if (send(clients[i], buf, received[client_num],0) < 0) {
                    printf("Failed to send to client %d\n",i);
                    continue;
                }
            }
        } // Send to all open client sockets
        

        pthread_mutex_unlock(&client_lock);
        pthread_mutex_unlock(buf_lock + client_num);

        
    }
    pthread_cancel(sthreads[client_num]);
}

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
    struct addrinfo server_address, *server_info;
    struct sockaddr client_address;
    char host_ip[100] = {0};
    char client_ip[100] = {0};
    int retval;
    int threadreturn;

    int clientcount = 0;
    client_bufs = malloc(sizeof(char[MAX_CLIENTS][MESSAGE_LEN]));

    if (argc != 2) {
        printf("Usage: ./server <PORT>\n");
        return 1;
    }


    // signal(SIGINT, signal_handler);

    port = (unsigned short)atoi(argv[1]);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        pthread_mutex_init(buf_lock + i, NULL);
        pthread_cond_init(buf_cond + i,NULL);
    } // initialize mutex and condition arrays for buffer modification between send and receive threads.

    memset(&server_address, 0, sizeof(server_address));
    server_address.ai_family = AF_INET6;
    server_address.ai_socktype = SOCK_STREAM;
    server_address.ai_flags = AI_PASSIVE;
    if ((retval = getaddrinfo(NULL, argv[1], &server_address, &server_info)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(retval));
		return 1;
	}

    // server_socket = socket(server_info->ai_family, server_address.ai_socktype, server_address.ai_protocol);
    
    // if (server_socket < 0){
    //     printf("Socket unable to be created!\n");
    //     return 1;
    // }

    for (struct addrinfo* probe = server_info; probe != NULL; probe = probe->ai_next){
        
        server_socket = socket(server_info->ai_family, probe->ai_socktype, probe->ai_protocol);
        
        if (server_socket < 0){
            printf("Socket unable to be created!\n");
            return 1;
        }
        
        if (retval = bind(server_socket, probe->ai_addr, probe->ai_addrlen) < 0) {
            continue;
        }
        else{
            inet_ntop(AF_INET6, &probe->ai_addr, host_ip, 100);
            printf("Waiting for a connection on %s on port %s\n", host_ip, argv[1]);
            break;
        }
    } // addrinfo list iterator
    
    if (retval < 0) {
        printf("Cannot Bind! Error code: %d\n", retval);
        perror("Error binding");
        exit(1);
    }
    
    
    listen(server_socket,MAX_CLIENTS);
    while (g_keepgoing && (client_socket = accept(server_socket, &client_address, &client_len))) {
        int startnum = clientcount;
        
        pthread_mutex_lock(&client_lock);
        while (freeclients[clientcount]) {
            clientcount = (clientcount + 1) % MAX_CLIENTS;
            if (startnum == clientcount) { 
                printf("Max Clients Reached!\n");
                break;
            }
        }// Handle in-use slot and attempt to find another
        
        if (freeclients[clientcount]) {
            pthread_mutex_unlock(&client_lock);
            continue;
        }
        pthread_mutex_unlock(&client_lock);
        
        inet_ntop(client_address.sa_family, get_in_addr(&client_address), client_ip, 100); // Obtain client IP string

        printf("Connection %d made from %s\n",clientcount, client_ip);

        // acquire lock to modify client information arrays
        pthread_mutex_lock(&client_lock);
        clients[clientcount] = client_socket;
        freeclients[clientcount] = 1;
        pthread_mutex_unlock(&client_lock);
        
        if (threadreturn = pthread_create(rthreads + clientcount, NULL, server_receive_thread, (void *) clientcount)) {
                printf("ERROR: Return Code from pthread_create() is %d\n", threadreturn);
                perror("ERROR creating thread");
                exit(1);
        } // Error if receive thread fails to instanciate

        if (threadreturn = pthread_create(sthreads + clientcount, NULL, server_send_thread, (void *) clientcount)) {
                printf("ERROR: Return Code from pthread_create() is %d\n", threadreturn);
                perror("ERROR creating thread");
                exit(1);
        } // Error if send thread fails to instanciate  
    } // Listen for new connections and establish threads to handle communication.
        
    close(server_socket);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        pthread_cancel(rthreads[i]);
        pthread_cancel(sthreads[i]);
    }
    pthread_exit(0);
}