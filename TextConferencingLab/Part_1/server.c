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
    // Variable Declaration
    // new_user -> User Corresponding To Client
    // session_joined -> List Of Sessions Joined
    // buffer -> Buffer For recv
    // source -> User Username
    User * new_user = (User *) arg;
    Session * session_joined = NULL;
    char buffer[BUFFER_SIZE];
    char source[MAX_NAME];
    int bytes_sent;
    int bytes_received;

    // Message Received And Message Sent
    message message_received;
    message message_sent;

    // Indicating Thread Created
    fprintf(stdout, "New Thread Created\n");

    // FSM States
    bool logged_in = 0;
    bool to_exit = 0;

    // Main Recv Loop
    while (1) {
        // Set Memory
        memset(buffer, 0, sizeof(char) * BUFFER_SIZE);
        memset(&message_received, 0, sizeof(message));

        // Receiving Message
        if ((bytes_received = recv(new_user -> socketFD, buffer, BUFFER_SIZE - 1, 0)) == -1) {
            fprintf(stderr, "Server: new_client recv Error\n");
            exit(1);
        }

        // Check Message Status
        if (bytes_received == 0) to_exit = 1;
        buffer[bytes_received] = '\0';

        // Indicating Message Received
        bool to_send = 0;
        fprintf(stdout, "Server: Message Received: \"%s\"\n", buffer);

        // Converting Message
        deserialization(buffer, &message_received);
        memset(&message_sent, 0, sizeof(message));

        // Check If Exit Message
        if (message_received.type == 3) {
            to_exit = 1;
        }

        // Before Login User Can Only Login Or Exit
        if (logged_in == 0) {
            if (message_received.type == 0) {
                // Parse Username And Password
                strcpy(new_user -> username, (char *) (message_received.source));
                strcpy(new_user -> password, (char *) (message_received.data));
                fprintf(stdout, "Server: Username: %s\n", new_user -> username);
                fprintf(stdout, "server: Password: %s\n", new_user -> password);

                // Check If User Already Joined
                bool already_joined = in_list(user_logged_in, new_user);
                bool valid_user = is_valid_user(user_list, new_user);

                // Check If Valid
                if (!already_joined && valid_user) {
                    // Indicating Valid User
                    fprintf(stdout, "Server: User Is Valid\n");

                    // Changing Variables
                    message_sent.type = 1;
                    to_send = 1;
                    logged_in = 1;

                    // Add User To user_logged_in List
                    User * copy_user = (User *) malloc(sizeof(User));
                    memcpy(copy_user, new_user, sizeof(User));
                    pthread_mutex_lock(&user_logged_in_mutex);
                    user_logged_in = add_user(user_logged_in, copy_user);
                    pthread_mutex_unlock(&user_logged_in_mutex);

                    // Save Username
                    strcpy(source, new_user -> username);
                    fprintf(stdout, "Server: User %s: Successfully Logged In\n", new_user -> username);
                } else {
                    // Indicating Invalid User
                    fprintf(stdout, "Server: User Is Invalid\n");

                    // Changing Variables
                    message_sent.type = 2;
                    to_send = 1;

                    // Respond With Reason For Failure
                    if (already_joined) {
                        strcpy((char *) (message_sent.data), "Multiple Login");
                    } else if (in_list(user_list, new_user)) {
                        strcpy((char *) (message_sent.data), "Wrong Password");
                    } else {
                        strcpy((char *) (message_sent.data), "User Not Registered");
                    }

                    fprintf(stdout, "Server: Login Failed From Anonymous User\n");

                    to_exit = 1;
                }
            } else {
                message_sent.type = 2;
                to_send = 1;
                strcpy((char *) (message_sent.data), "Please Login First");
            }
        // Message For Joining A Conference Session
        } else if (message_received.type == 4) {
            // Variable Declaration
            int session_ID = atoi((char *) message_received.data);
            fprintf(stdout, "Server: User %s: Trying To Join Session %d\n", new_user -> username, session_ID);

            // Session Not Exist -> Fail
            // Invalid Session
            if (is_valid_session(session_list, session_ID) == NULL) {
                message_sent.type = 6;
                to_send = 1;
                int cursor = sprintf((char *) (message_sent.data), "%d", session_ID);
                strcpy((char *) (message_sent.data + cursor), " Session Not Exist");
                fprintf(stdout, "Server: User %s: Failed To Join Session %d, Reason, Sesson Not Exist\n", new_user -> username, session_ID);
            // Already Joined Session
            } else if (in_session(session_list, session_ID, new_user)) {
                message_sent.type = 6;
                to_send = 1;
                int cursor = sprintf((char *) (message_sent.data), "%d", session_ID);
                strcpy((char *) (message_sent.data + cursor), " Session Already Joined");
                fprintf(stdout, "Server: User %s: Failed To Join Session %d, Reason, Sesson Already Joined\n", new_user -> username, session_ID);
            // Successfully Joined Session
            } else {
                message_sent.type = 5;
                to_send = 1;
                sprintf((char *) (message_sent.data), "%d", session_ID);

                // Update Global session_list
                pthread_mutex_lock(&session_list_mutex);
                session_list = join_session(session_list, session_ID, new_user);
                pthread_mutex_unlock(&session_list_mutex);

                // Update Private session_list
                session_joined = initiate_session(session_joined, session_ID);
                fprintf(stdout, "Server: User %s: Successfully Joined Session %d\n", new_user -> username, session_ID);

                // Update User Status In User Connected
                pthread_mutex_lock(&user_logged_in_mutex);
                for (User * current_user = user_logged_in; current_user != NULL; current_user = current_user -> next) {
                    if (strcmp(current_user -> username, source) == 0) {
                        current_user -> in_session = 1;
                        current_user -> session_joined = initiate_session(current_user -> session_joined, session_ID);
                    }
                }
                pthread_mutex_unlock(&user_logged_in_mutex);
            }
        // Message For Leave Session, No Reply
        } else if (message_received.type == 7) {
            fprintf(stdout, "Server: User %s: Trying To Leave All Sessions\n", new_user -> username);

            // Iterate All Joined Session
            while (session_joined != NULL) {
                int current_session_ID = session_joined -> session_ID;

                // Free Private Session Joined
                Session * current_session = session_joined;
                session_joined = session_joined -> next;
                free(current_session);

                // Free Global Session List
                pthread_mutex_lock(&session_list_mutex);
                session_list = leave_session(session_list, current_session_ID, new_user);
                pthread_mutex_unlock(&session_list_mutex);
            }

            fprintf(stdout, "Server: User %s: Left All Sessions\n", new_user -> username);
            serialization(&message_sent, buffer);

            // Update User Status In User Connected
            pthread_mutex_lock(&user_logged_in_mutex);
            for (User * current_user = user_logged_in; current_user != NULL; current_user = current_user -> next) {
                if (strcmp(current_user -> username, source) == 0) {
                    free_session_list(current_user -> session_joined);
                    current_user -> session_joined = NULL;
                    current_user -> in_session = 0;
                    break;
                }
            }
            pthread_mutex_unlock(&user_logged_in_mutex);
        // Create New Session
        } else if (message_received.type == 8) {
            fprintf(stdout, "Server: User %s: Trying To Create New Session\n", new_user -> username);

            // Update Global Session List
            serialization(&message_sent, buffer);
            pthread_mutex_lock(&session_list_mutex);
            session_list = initiate_session(session_list, session_count);
            pthread_mutex_unlock(&session_list_mutex);

            // User Join Created Session
            session_joined = initiate_session(session_joined, session_count);
            pthread_mutex_lock(&session_list_mutex);
            session_list = join_session(session_list, session_count, new_user);
            pthread_mutex_unlock(&session_list_mutex);

            // Update User Status In User Logged In
            pthread_mutex_lock(&user_logged_in_mutex);
            for (User * current_user = user_logged_in; current_user != NULL; current_user = current_user -> next) {
                if (strcmp(current_user -> username, source) == 0) {
                    current_user -> in_session = 1;
                    current_user -> session_joined = initiate_session(current_user -> session_joined, session_count);
                    break;
                }
            }
            pthread_mutex_unlock(&user_logged_in_mutex);

            // Update Message Sent NS_ACK
            message_sent.type = 9;
            to_send = 1;
            sprintf((char *) (message_sent.data), "%d", session_count);

            // Update Session Count
            pthread_mutex_lock(&session_count_mutex);
            ++session_count;
            pthread_mutex_unlock(&session_count_mutex);

            fprintf(stdout, "Server: User %s: Successfully Created Session %d\n", new_user -> username, session_count - 1);
        // Usert Send Message
        } else if (message_received.type == 10) {
            fprintf(stdout, "Server: User %s: Sending Message \"%s\"\n", new_user -> username, message_received.data);

            // Session To Send To
            int current_session_ID = atoi(message_received.source);

            // Prepare Message To Be Sent
            memset(&message_sent, 0, sizeof(message));
            message_sent.type = 10;
            strcpy((char *) (message_sent.source), new_user -> username);
            strcpy((char *) (message_sent.data), (char *) (message_received.data));
            message_sent.size = strlen((char *) (message_sent.data));

            // Use recv Buffer
            memset(buffer, 0, sizeof(char) * BUFFER_SIZE);
            serialization(&message_sent, buffer);
            fprintf(stderr, "Server: Broadcasting Message %s To Sessions\n", buffer);

            // Send Through Local Session List
            for (Session * current_session = session_joined; current_session != NULL; current_session = current_session -> next) {
                Session * session_to_send;
                if ((session_to_send = is_valid_session(session_list, current_session -> session_ID)) == NULL) continue;
                for (User * current_user = session_to_send -> user; current_user != NULL; current_user = current_user -> next) {
                    fprintf(stdout, "Server: Sending Message \"%s\" To User %s\n", buffer, current_user -> username);
                    buffer[strlen(buffer)] = '\0';
                    if ((bytes_sent = send(current_user -> socketFD, buffer, strlen(buffer) + 1, 0)) == -1) {
                        fprintf(stderr, "Server: Broadcasting send Error\n");
                        exit(1);
                    }
                }
            }   

            to_send = 0;
        // User Query
        } else if (message_received.type == 11) {
            fprintf(stdout, "Server: User %s: Making Query\n", new_user -> username);

            int cursor = 0;
            message_sent.type = 12;
            to_send = 1;

            for (User * current_user = user_logged_in; current_user != NULL; current_user = current_user -> next) {
                cursor += sprintf((char *) (message_sent.data) + cursor, "%s", current_user -> username);
                for (Session * current_session = current_user -> session_joined; current_session != NULL; current_session = current_session -> next) {
                    cursor += sprintf((char *) (message_sent.data) + cursor, "\t%d", current_session -> session_ID);
                }
                message_sent.data[cursor++] = '\n';
            }

            fprintf(stdout, "Server; Query Result: \n%s\n", message_sent.data);
        }

        // Sending Message To Client
        if (to_send) {
            // Add Source And Size For Message Sent And Send Message
            fprintf(stdout, "Server: Sending Message Source \"%s\" Message Data \"%s\" To User %s\n", message_sent.source, message_sent.data, new_user -> username);
            memcpy(message_sent.source, new_user -> username, USERNAME_LENGTH);
            message_sent.size = strlen((char *) (message_sent.data));

            memset(buffer, 0, BUFFER_SIZE);
            serialization(&message_sent, buffer);
            fprintf(stdout, "Server: Sending Message \"%s\" To User %s\n", buffer, new_user -> username);
            buffer[strlen(buffer)] = '\0';
            if ((bytes_sent = send(new_user -> socketFD, buffer, strlen(buffer) + 1, 0)) == -1) {
                fprintf(stderr, "Server: send Error\n");
            }
        }

        // Exit Recv Loop
        if (to_exit) break;
    }

    // Thread Exits No ACK Packet Sent, Close Socket
    close(new_user -> socketFD);

    // Only Clean Up Info For Successful Logins
    if (logged_in == 1) {
        // Leave All Sessions, Remove User From Session List
        fprintf(stdout, "Server: Removing User %s From Session List\n", source);
        for (Session * current_session = session_joined; current_session != NULL; current_session = current_session -> next) {
            pthread_mutex_lock(&session_list_mutex);
            session_list = leave_session(session_list, current_session -> session_ID, new_user);
            pthread_mutex_unlock(&session_list_mutex);
        }

        // Remove From user_logged_in
        fprintf(stdout, "Server: Removing User %s From User Logged In List\n", source);
        pthread_mutex_lock(&user_logged_in_mutex);
        for (User * current_user = user_logged_in; current_user != NULL; current_user = current_user -> next) {
            if (strcmp(source, current_user -> username) == 0) {
                free_session_list(current_user -> session_joined);
                break;
            }
        }
        pthread_mutex_unlock(&user_logged_in_mutex);

        pthread_mutex_lock(&user_logged_in_mutex);
        user_logged_in = remove_user(user_logged_in, new_user);
        pthread_mutex_unlock(&user_logged_in_mutex);

        // Remove Private Session List
        free_session_list(session_joined);
        free(new_user);

        // Decrement user_connected_count
        pthread_mutex_lock(&user_connected_count_mutex);
        --user_connected_count;
        pthread_mutex_unlock(&user_connected_count_mutex);
    }

    // Indicating User Exited
    if (logged_in) {
        fprintf(stdout, "Server: User %s Exiting...\n", source);
    } else {
        fprintf(stdout, "Server: User Exiting ...\n");
    }

    // Finished Executing New Client Function
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