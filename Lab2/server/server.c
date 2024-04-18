/* File: server.c
 * Authors: Bryce Bejlovec and Owen Sapp
 * Threaded server for a chat service that can host multiple users with file transport capability
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
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>


#define MESSAGE_LEN 1024
#define MAX_CLIENTS 5
#define RECV_F_FLAGS O_CREAT | O_WRONLY | O_EXCL
#define RECV_UH S_IRUSR | S_IWUSR

char* fileexisting = "A file with this name already exists.\nPlease rename and try again.\n";
char* filenotfound= "File does not exist. Please try Again.\n";

int g_keepgoing = 1;
int pathlenmod = 0;
int clients[MAX_CLIENTS] = {0};
char freeclients[MAX_CLIENTS] = {0};
char trans[MAX_CLIENTS] = {0};
int received[MAX_CLIENTS] = {0};
char (*client_bufs)[MAX_CLIENTS][MESSAGE_LEN] = NULL;
int server_socket, client_socket;
bool sendbool[MAX_CLIENTS] = {false};
pthread_mutex_t client_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t buf_lock[MAX_CLIENTS];
pthread_cond_t buf_cond[MAX_CLIENTS];
pthread_t rthreads[MAX_CLIENTS];
pthread_t sthreads[MAX_CLIENTS];

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
            sendbool[client_num] = true;
            pthread_cond_broadcast(buf_cond + client_num);
            pthread_mutex_unlock(buf_lock + client_num);
            printf("Lost Connection from client %d\n",client_num);
            break;
        }
        buf[received[client_num]] = '\0';

        if(strncmp(buf,"uTake ",6) == 0){
            pathlenmod = received[client_num] - 1;
            char (*pathbuf) = malloc(sizeof(char[MESSAGE_LEN]));
            while (buf[pathlenmod] != '/' && pathlenmod > 5) {
                pathlenmod--;
            } 
            pathlenmod = pathlenmod - 5;
            printf("%d\n",pathlenmod);
            strcpy(pathbuf, "./store");
            
            if (pathlenmod > 0) {
                strcat(pathbuf, buf + 6);
                strcpy(buf, pathbuf);
                buf[pathlenmod + 7] = 0;
                printf("%s\n",buf);
                struct stat pathstat;
                if (stat(buf, &pathstat) == -1) {
                    mkdir(buf, 0700);
                    pathlenmod = 0;
                }
            } else {
                strcat(pathbuf, "/default/");
                strcat(pathbuf, buf + 6);
            }
            

            int file = open(pathbuf, RECV_F_FLAGS, RECV_UH);
            free(pathbuf);
            if (file < 0) {

                if (errno == EEXIST) {
                    if (send(clients[client_num], fileexisting, strlen(fileexisting),0) < 0) {
                        sendbool[client_num] = true;
                        pthread_cond_broadcast(buf_cond + client_num);
                        pthread_mutex_unlock(buf_lock + client_num);
                        printf("Failed to send to client %d\n",client_num);
                        break;
                    }
                }
            } else {
                if (send(clients[client_num], "R", 1,0) < 0) {
                    sendbool[client_num] = true;
                    pthread_cond_broadcast(buf_cond + client_num);
                    pthread_mutex_unlock(buf_lock + client_num);
                    printf("Failed to send to client %d\n",client_num);
                    break;
                }

                trans[client_num] = 1;
                
                memset(buf, 0, MESSAGE_LEN);
                received[client_num] = read(clients[client_num], buf, MESSAGE_LEN);
                if (received[client_num] <= 0) {
                    sendbool[client_num] = true;
                    pthread_cond_broadcast(buf_cond + client_num);
                    pthread_mutex_unlock(buf_lock + client_num);
                    printf("Lost Connection from client %d\n",client_num);
                    break;
                }
                
                while (received[client_num] == MESSAGE_LEN) {
                    write(file, buf, received[client_num]);
                    memset(buf, 0, MESSAGE_LEN);
                    received[client_num] = read(clients[client_num], buf, MESSAGE_LEN);

                    if (received[client_num] <= 0) {
                        sendbool[client_num] = true;
                        pthread_cond_broadcast(buf_cond + client_num);
                        pthread_mutex_unlock(buf_lock + client_num);
                        printf("Lost Connection from client %d\n",client_num);
                        break;
                    }
                    printf("%d\n",received[client_num]);
                    
                }
                printf("%d\n",received[client_num] - pathlenmod);
                write(file, buf, received[client_num] - pathlenmod);
                trans[client_num] = 0;
            }
            trans[client_num] = 0;
            close(file);
            pthread_mutex_unlock(buf_lock + client_num);
            continue;
        }

        if(strncmp(buf,"iWant ", 6) == 0) {
            pathlenmod = received[client_num] - 1;
            char (*pathbuf) = malloc(sizeof(char[MESSAGE_LEN]));
            while (buf[pathlenmod] != '/' && pathlenmod > 5) {
                pathlenmod--;
            } 
            pathlenmod = pathlenmod - 5;
            printf("%d\n",pathlenmod);
            strcpy(pathbuf, "./store");
            
            if (pathlenmod > 0) {
                strcat(pathbuf, buf + 6);
                strcpy(buf, pathbuf);
                buf[pathlenmod + 7] = 0;
            } else {
                strcat(pathbuf, "/default/");
                strcat(pathbuf, buf + 6);
            }
            
            
            FILE* file = fopen(pathbuf, "rb");
            
            if (file == NULL) {
                if (send(clients[client_num], filenotfound, strlen(filenotfound),0) < 0) {
                    sendbool[client_num] = true;
                    pthread_cond_broadcast(buf_cond + client_num);
                    pthread_mutex_unlock(buf_lock + client_num);
                    printf("Failed to send to client %d\n",client_num);
                    break;
                }
                continue;
            }
            trans[client_num] = 1;

            free(pathbuf);

            if (send(clients[client_num], "R", 1,0) < 0) {
                fclose(file);
                trans[client_num] = 0;
                sendbool[client_num] = true;
                pthread_cond_broadcast(buf_cond + client_num);
                pthread_mutex_unlock(buf_lock + client_num);
                printf("Failed to send to client %d\n",client_num);
                break;
            }

            memset(buf, 0, MESSAGE_LEN);

            received[client_num] = read(clients[client_num], buf, MESSAGE_LEN - 1);
        
            if (received[client_num] <= 0) {
                fclose(file);
                trans[client_num] = 0;
                sendbool[client_num] = true;
                pthread_cond_broadcast(buf_cond + client_num);
                pthread_mutex_unlock(buf_lock + client_num);
                printf("Lost Connection from client %d\n",client_num);
                break;
            }

            if (strcmp(buf,"R") != 0) {
                printf("Received from server: %s\n",buf);
            } else {
                long fileread = 0;
                while (fileread = fread(buf, 1, MESSAGE_LEN, file)) {
                    if (send(clients[client_num], buf, fileread,0) < 0) {
                        fclose(file);
                        pthread_cond_broadcast(buf_cond + client_num);
                        pthread_mutex_unlock(buf_lock + client_num);
                        printf("Failed to send to client %d\n",client_num);
                        break;
                    }
                    memset(buf, 0, MESSAGE_LEN);
                }

                printf("File upload complete.\n");
            }
            fclose(file);
            trans[client_num] = 0;

        }
        
        printf("Data from client %d:\n     \"%s\"\n", client_num, buf);

        if(strcmp(buf, ";;;") == 0){
            g_keepgoing = 0;
            printf("Client finished, now waiting to service another client...\n");
        }
        
        for (int i = 0; i < received[client_num]; i++) {
            buf[i] = toupper(buf[i]);
        } // set buffer to all uppercase

        sendbool[client_num] = true;
        
        pthread_cond_broadcast(buf_cond + client_num);
        pthread_mutex_unlock(buf_lock + client_num);
    } // continue to receive client data and convert to uppercase
    
    pthread_mutex_lock(&client_lock);
    close(clients[client_num]);
    freeclients[client_num] = 0;
    trans[client_num] = 0;
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
            if (freeclients[i] && !trans[i]){
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