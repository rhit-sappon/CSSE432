#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <ctype.h>
#include <pthread.h>
#include <signal.h>


/*
    Author: Owen Sapp
    Purpose: Simple socket server that recieves ascii and sends back upper-case characters to the client.
    Date: 3/25/24

    Attempted Incentive:
        1. Writen in C

    Note on Incentive: 
        While not written as a multi-user group chat, threaded functionality was added to allow for multiple
        connections simultaneously. This may be worth partial incentive?

*/


#define DEFAULT_PORT 5500
#define BUFF_SIZE 256
#define MAX_CLIENTS 5

struct sockaddr_in server_addr, client_addr;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

struct threadargs {
    int thread_id;
    void * socket;
    int message_number;
};

char recieved_message[BUFF_SIZE];
void recieve_message(void * input){
    int client_number = ((struct threadargs*)input)->thread_id;
    int message_number = ((struct threadargs*)input)->message_number;
    int sockfd = (int)((struct threadargs*)input)->socket;
    //int sockfd = (int) socket;
    while(1){
        printf("Listening for Incoming Message\n");
        memset(recieved_message, 0, BUFF_SIZE);
        if(read(sockfd, recieved_message, BUFF_SIZE) < 0){
            printf("Err read\n");
        }
        printf("Recieved from client%d: Message %d \" %s \"\n",client_number,message_number,recieved_message);
        int i = 0;
        while(recieved_message[i]){
            recieved_message[i] = toupper(recieved_message[i]);
            i = i + 1;
        }
        printf("Sending back having changed string to upper case...\n\n");
        if(write(sockfd, recieved_message, BUFF_SIZE) < 0){
            printf("Err write\n");
            break;
        }
        if(!strcmp(recieved_message, ";;;")){
            printf("Exit code read, closing connection\n\n");
            printf("****************************************************\n\n");
            break;
        }
        message_number = message_number + 1;
    }
    close(sockfd);

}


void server_program(int port){
    //Create Socket
    pthread_t server_thread;
    int socket_fd = socket(AF_INET,SOCK_STREAM,0);
    if(socket_fd < 0){ 
        printf("err opening socket\n");
        exit(0);
    } /*end if socket error*/
    memset(&server_addr, 0, sizeof(server_addr));

    int opt = 1; 
    setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    

    //Bind
    if(bind(socket_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0){
        printf("error binding socket\n");
        exit(1);
    }

    //Listen
    listen(socket_fd,5);
    printf("Server starting, listening on port %d\n",port);
    int new_socket_fd;
    int client_number = 0;
    int cli_size = sizeof(client_addr);
    char client_name[BUFF_SIZE];
    while(new_socket_fd = accept(socket_fd, (struct sockaddr *) &client_addr, &cli_size)){
        inet_ntop(AF_INET,&(client_addr.sin_addr), client_name, BUFF_SIZE);
        printf("Connection made from: %s\n",client_name);
        printf("****************************************************\n\n");
        int retval;

        struct threadargs *arguments = (struct threadargs *)malloc(sizeof(struct threadargs));
        arguments->thread_id = client_number;
        arguments->socket = (void *)new_socket_fd;
        arguments->message_number = 0;

        if(retval = pthread_create(&server_thread,NULL, recieve_message, (void *) arguments) < 0 ){
            printf("error creating thread: %d\n",retval);
            exit(1);
        }
        client_number = client_number + 1;



    }/*end while constantly accepting new connections*/

}


int main(int argc, char *argv[]){
    if(argc == 1){
        server_program(DEFAULT_PORT);
    }
    else{
        server_program(atoi(argv[1]));
    }
    return 0;
}