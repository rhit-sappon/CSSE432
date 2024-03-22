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

#define DEFAULT_PORT 5500
#define BUFF_SIZE 256
#define MAX_CLIENTS 5

struct sockaddr_in server_addr, client_addr;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

char recieved_message[BUFF_SIZE];
void recieve_message(void * socket){
    int sockfd = (int) socket;
    while(1){
        memset(recieved_message, 0, BUFF_SIZE);
        if(read(sockfd, recieved_message, BUFF_SIZE) < 0){
            printf("Err read\n");
        }
        int i = 0;
        while(recieved_message[i]){
            recieved_message[i] = toupper(recieved_message[i]);
            i = i + 1;
        }
        printf("Recieved: \" %s  \"\n",recieved_message);
        if(write(sockfd, recieved_message, BUFF_SIZE) < 0){
            printf("Err write\n");
            break;
        }
        if(!strcmp(recieved_message, ";;;")){
            printf("Exit code read, closing connection\n");
            break;
        }
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

    printf("waiting for a connection\n");
    //Listen
    listen(socket_fd,5);
    int new_socket_fd;
    int client_number = 0;
    int cli_size = sizeof(client_addr);
    char client_name[BUFF_SIZE];
    while(new_socket_fd = accept(socket_fd, (struct sockaddr *) &client_addr, &cli_size)){
        inet_ntop(AF_INET,&(client_addr.sin_addr), client_name, BUFF_SIZE);
        printf("Connection made from: %s\n",client_name);
        int retval;
        if(retval = pthread_create(&server_thread,NULL, recieve_message, (void *) new_socket_fd) < 0 ){
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