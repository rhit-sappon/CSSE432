/* File: server.c
 * Author: Bryce Bejlovec and Owen Sapp
 * Threaded http proxy server
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
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include "proxy_parse.h"


typedef int bool;
#define false 0
#define true 1



#define MESSAGE_LEN 1024
#define MAX_CLIENTS 10

#define RECV_F_FLAGS O_CREAT | O_WRONLY | O_EXCL
#define RECV_UH S_IRUSR | S_IWUSR

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

char* replace_character(char* string, char find, char replace){
    char* index = strchr(string, find);
    while(index){
        *index = '.';
        index = strchr(string, find);
    }
    return string;
}

void * server_receive_thread(void * clinum){
    int client_num = (int) clinum;
    char* buf = (*client_bufs)[client_num];
    while (g_keepgoing) {

        pthread_mutex_lock(buf_lock + client_num);
        char request[MESSAGE_LEN];
        char lastfour[4];
        while(strcmp(request+strlen(request)-4, "\r\n\r\n") != 0){
            received[client_num] = read(clients[client_num], buf, MESSAGE_LEN - 1);
            if (received[client_num] <= 0) {
                pthread_mutex_unlock(buf_lock + client_num);
                printf("Lost Connection from client %d\n",client_num);
                break;
            }
            strcat(request, buf);
            memset(buf, 0, MESSAGE_LEN);
            printf("%s",request+strlen(request)-4);
            printf("STRCMP %d\n",strcmp(request+strlen(request)-4, "\r\n\r\n"));
        }
        strcpy(buf, request);
        printf("%s\n",buf);
        struct ParsedRequest *req = ParsedRequest_create();
        if (ParsedRequest_parse(req, buf, strlen(buf)) < 0) {
            printf("parse failed, malformed request\n");
            break;
        }
        printf("Recieved from client:\n");
        printf("Method:%s\n", req->method);
        printf("Host:%s\n", req->host);
        printf("Protocol:%s\n", req->protocol);
        printf("Path:%s\n",req->path);
        printf("Version:%s\n",req->version);
        ParsedHeader_set(req, "Connection","close\n");
        //ParsedHeader_remove(req, "Accept-Encoding");
        char head[ParsedHeader_headersLen(req)];
        ParsedRequest_unparse_headers(req, head,sizeof(head));
        printf("Headers:\n%s\n",head);
        

        
        //check if desired file exists
        char pathbuf[100];
        strcpy(pathbuf, "./repo/");
        strcat(pathbuf, req->host);
        int temp = strlen(pathbuf);
        char reqpath[100];
        strcpy(reqpath, req->path);
        replace_character(reqpath,'/','.');
        strcat(pathbuf, reqpath);
        pathbuf[temp] = '.';
        pathbuf[strlen(pathbuf)] = 0;

        struct stat statbuf;
        if(stat(pathbuf, &statbuf) == 0){
            //printf("----File Exists\n");
            //file exists
            if(time(NULL) - (statbuf.st_mtime) < 60){
                //recent enough, open and send
                printf("----File Recent\n");
                FILE* website_file = fopen(pathbuf,"rb");
                long fileread = 0;
                while (fileread = fread(buf, 1, MESSAGE_LEN, website_file)) {
                    if (send(clients[client_num], buf, fileread,0) < 0) {
                        fclose(website_file);
                        pthread_cond_broadcast(buf_cond + client_num);
                        pthread_mutex_unlock(buf_lock + client_num);
                        printf("Failed to send to client %d\n",client_num);
                        break;
                    }
                    memset(buf, 0, MESSAGE_LEN);
                }
                fclose(website_file);
                continue;
            }
            else{printf("----file not recent\n");}
        }
        printf("----File DNE\n");
        printf("%s\n",pathbuf);
        //Else file exists or is too old, create file or delete what was there
        FILE* website_file = fopen(pathbuf, "wb");
        fclose(website_file);

        //connect to website
        char web_port[5] = "80";
        if(!strcmp("http",req->protocol)){
            strcpy(web_port,"80");
        }
        
        int website_sockfd;
        struct addrinfo hints, *sites, *site;
        memset (&hints, 0, sizeof(struct addrinfo));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_ADDRCONFIG;
        int ret;
        if((ret = getaddrinfo(req->host, web_port, &hints, &sites)) != 0){
            printf("getaddrinfo: %s \n", gai_strerror(ret));
            break;
        }
        for (site = sites; site!= NULL; site = site->ai_next){
            if ((website_sockfd = socket(site->ai_family, site->ai_socktype, site->ai_protocol)) == -1){
                perror("client: socket");
            }
            if (connect(website_sockfd, site->ai_addr, site->ai_addrlen) == -1){
                perror("client connect");
            }
            break;
        }
        if(site == NULL){
            printf("Cli failed to connect");
            break;
        }
        printf("Connection with host %s successful on port %s\n",req->host, web_port);

        //send website the get request (buf)
        memset(buf,0,MESSAGE_LEN);
        //printf("1BUF: %s\n",buf);
        strcpy(buf, "GET ");
        //printf("2BUF: %s\n",buf);
        strcat(buf, req->path);
        //printf("3BUF: %s\n",buf);
        strcat(buf," HTTP/1.0\r\n");
        strcat(buf,head);
        
        printf("---------------\nSending to external website\n%s",buf);
        if (send(website_sockfd, buf, MESSAGE_LEN, 0) < 0){
            printf("Failed to send GET Request to external website\n");
            break;
        }
        memset(buf, 0, MESSAGE_LEN);
        strcat(buf,"\r\n");
        if (send(website_sockfd, buf, sizeof(buf), 0) < 0){
            printf("Failed to send GET Request to external website\n");
            break;
        }       
        //receive back the get request
        
        while((received[client_num] = read(website_sockfd, buf, MESSAGE_LEN)) == 1024){
            printf("Received back\n");
            if (received[client_num] < 0) {
                sendbool[client_num] = true;
                pthread_cond_broadcast(buf_cond + client_num);
                pthread_mutex_unlock(buf_lock + client_num);
                printf("Lost Connection from client %d\n",client_num);
                break;
            }
            printf("saving to file\n");   
            //save contents received to file 
            FILE* website_file = fopen(pathbuf, "ab");
            fwrite(buf, 1, MESSAGE_LEN, website_file);
            fclose(website_file);
            printf("sending to client\n");  
            //send get request back to client
            printf("Sending back to local browser\n");
            if (send(clients[client_num], buf, MESSAGE_LEN, 0) < 0){
                printf("Failed to send GET response to client\n");
                break;
            }
            memset(buf, 0, MESSAGE_LEN);
        }
        //Send remainder
        website_file = fopen(pathbuf, "ab");
        fwrite(buf, 1, received[client_num], website_file);
        fclose(website_file);
        if (send(clients[client_num], buf, MESSAGE_LEN, 0) < 0){
            printf("Failed to send GET response to client\n");
            break;
        }
        //err check
        if (received[client_num] <= 0) { 
            sendbool[client_num] = true;
            pthread_cond_broadcast(buf_cond + client_num);
            pthread_mutex_unlock(buf_lock + client_num);
            printf("Lost Connection from client %d\n",client_num);
            break;
        }
        //close website
        printf("Closing website connection\n");
        if (close(website_sockfd) < 0){
            printf("Failed to close website socket\n");
        }

        close(clients[client_num]);
        pthread_cond_broadcast(buf_cond + client_num);
        pthread_mutex_unlock(buf_lock + client_num);

    } // continue to receive client data and convert to uppercase
    
    pthread_mutex_lock(&client_lock);
    
    freeclients[client_num] = 0;
    pthread_mutex_unlock(&client_lock);
    // Free client slot.

    pthread_cancel(rthreads[client_num]);
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


    signal(SIGINT, signal_handler);

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


    } // Listen for new connections and establish threads to handle communication.
        
    close(server_socket);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        pthread_cancel(rthreads[i]);
        pthread_cancel(sthreads[i]);
    }
    pthread_exit(0);
}