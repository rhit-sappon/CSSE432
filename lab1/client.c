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

#define MAXDATASIZE 1024

bool readLine(char** line, size_t* size, size_t* length){
    while(1){
        printf("String for the server> ");
        size_t len = getline(line,size,stdin);
        if(len == -1){
            return false;
        } /*end if end of file*/
        if((*line)[len-1]=='\n'){
            (*line)[--len] = '\0';
        }/*end if last char is newline*/
        *length = len;

        if(len == 0)
            continue;
        
        return len > 1 || **line != '.';
    }
}

int main(int argc, char *argv[]){
    int sockfd, numbytes;
    char buff[MAXDATASIZE];
    struct addrinfo hints, *serverinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];


    if (argc != 3){
        fprintf(stderr, "usage: client hostname port\n");
    }
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    
    if((rv = getaddrinfo(argv[1], argv[2], &hints, &serverinfo)) != 0){
        fprintf(stderr, "getaddrinfo: %s \n", gai_strerror(rv));
        return 1;
    }

    for(p = serverinfo; p!= NULL; p = p->ai_next){
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
            perror("client: socket");
        }
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1){
            perror("client: connect");
        }
        break;
    }

    if(p == NULL){
        printf("Cli failed to connect");
        return 2;
    }
    printf("Connection successful. enter ';;;' to exit\n");
    while(strcmp(buff,";;;")){
        printf(" -> ");
        memset(buff, 0, MAXDATASIZE);
        fgets(buff,MAXDATASIZE,stdin);
        buff[strlen(buff)-1] = '\0';
        if(write(sockfd,buff,strlen(buff)) < 0){
            printf("Err writing\n");
        }
        memset(buff,0,MAXDATASIZE);
        int ind;
        ind = read(sockfd,buff, MAXDATASIZE);
        if(ind < 0){
            printf("err read\n");
        }
        buff[ind] = '\0';
        printf("Received from Server: %s\n", buff);
    }
    printf("Recieved exit code, closing connection\n");
    return 0;
}