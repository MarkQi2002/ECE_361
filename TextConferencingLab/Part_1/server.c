// Important Library
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <signal.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>

// Custome Header File
#include "message.h"
#include "user.h"

// Username And Password File
#define USER_LIST_FILE "user_list.txt"
// Number Of Pending Connections Queue Will Hold
#define BACKLOG 10

// List Of All User And Password
User * user_list = NULL;
// List Of Users Logged In
User * user_logged_in = NULL;
// Count of users connected
int user_connected_count = 0;

// List Of All Sessions Created
Session * session_list;
// Session Count Begin From 1
int session_count = 1;

// Enforce Synchronization
pthread_mutex_t session_list_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t user_logged_in_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t session_count_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t user_connected_count_mutex = PTHREAD_MUTEX_INITIALIZER;

// Get Sockaddr, IPv4 or IPv6:
void * get_in_addr(struct sockaddr * sa) {
    if (sa -> sa_family == AF_INET) {
        return &(((struct sockaddr_in *)sa) -> sin_addr);
    }
    return &(((struct sockaddr_in6 *)sa) -> sin6_addr);
}

// New Client Function To Handle New Client Connections
void * new_client(void * arg) {
    return NULL;
}

// Main Function
int main(int argc, char * argv[]) {
    // Input Arguments
    if (argc != 2) {
        fprintf(stderr, "Usage: server <TCP port number to listen on>\n");
        exit(1);
    }

    // Extract Port Number
    int port = atoi(argv[1]);

    // Indicating TCP Port Number
    fprintf(stdout, "TCP Port Number: %d\n", port);

    // Load User List From User List File
    FILE *fp;
    if ((fp = fopen(USER_LIST_FILE, "r")) == NULL) {
        fprintf(stderr, "Failed To Open Input File %s\n", USER_LIST_FILE);
        exit(1);
    }
    user_list = initiate_userlist_from_file(fp);
    fclose(fp);
    fprintf(stdout, "Server: User List Loaded Successfully From File %s\n", USER_LIST_FILE);

    // Setup Server
    int socketFD;

    // Socket Information
    struct addrinfo hints;
    struct addrinfo * server_info;
    struct addrinfo * ptr;
    struct sockaddr_storage client_address;
    socklen_t sin_size;
    struct sigaction sa;
    int yes = 1;
    int rv;
    char s[INET6_ADDRSTRLEN];

    // Bind Associates Socket With An Address
    // SOCK_STREAM -> Use TCP
    // AI_PASSIVE -> Use My IP
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if ((rv = getaddrinfo(NULL, argv[1], &hints, &server_info)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }

    // Loop Through All The Results And Bind To The First We Can
    for (ptr = server_info; ptr != NULL; ptr = ptr -> ai_next) {
        if ((socketFD = socket(ptr -> ai_family, ptr -> ai_socktype, ptr -> ai_protocol)) == -1) {
            perror("Server: socket Error\n");
            continue;
        }
        if (setsockopt(socketFD, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == 01) {
            perror("Server: setsockopt Error\n");
            exit(1);
        }
        if (bind(socketFD, ptr -> ai_addr, ptr -> ai_addrlen) == -1) {
            close(socketFD);
            perror("Server: bind Error\n");
            continue;
        }
        break;
    }

    // Finished Using Server Info
    freeaddrinfo(server_info);

    // Check If Bind Successfully
    if (ptr == NULL) {
        fprintf(stderr, "Server: Failed To Bind\n");
        exit(1);
    }

    // Server Exit After No User Online For Five Minutes
    struct timeval timeout;
    timeout.tv_sec = 300;
    timeout.tv_usec = 0;
    if (setsockopt(socketFD, SOL_SOCKET, SO_RCVTIMEO, (void *) &timeout, sizeof(timeout)) < 0) {
        fprintf(stderr, "Server: setsockopt Error\n");
        exit(1);
    }

    // Listen Incoming Port
    if (listen(socketFD, BACKLOG) == -1) {
        fprintf(stderr, "Server: listen Error\n");
        exit(1);
    }
    fprintf(stdout, "Server: Waiting For Connections...\n");

    // Main Loop
    do {
        while (1) {
            // Accept New Incoming Connections
            User * new_user = (User *) malloc(sizeof(User));
            sin_size = sizeof(client_address);
            new_user -> socketFD = accept(socketFD, (struct sockaddr *) &client_address, &sin_size);
            if (new_user -> socketFD == -1) {
                fprintf(stderr, "Server: accept Error\n");
                break;
            }
            inet_ntop(client_address.ss_family, get_in_addr((struct sockaddr *) &client_address), s, sizeof(s));
            fprintf(stdout, "Server: Got Connection From %s\n", s);

            // Increment user_connected_count
            pthread_mutex_lock(&user_connected_count_mutex);
            ++user_connected_count;
            pthread_mutex_unlock(&user_connected_count_mutex);

            // Create New Thread To Handle New Socket
            fprintf(stdout, "A Client Has Successfully Connect!\n");
            pthread_create(&(new_user -> thread_id), NULL, new_client, (void *) new_user);
        }
    } while (user_connected_count > 0);

    // Free Allocated Data
    free_user_list(user_list);
    free_user_list(user_logged_in);
    free_session_list(session_list);
    close(socketFD);
    fprintf(stdout, "Server Closed\n");

    // Successfully Executed Main Function
    return 0;
}