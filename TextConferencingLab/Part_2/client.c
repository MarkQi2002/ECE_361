// Compile Macro
#define fprintf_message

// Important Library
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include <pthread.h>
#include <stdbool.h>
#include <semaphore.h>

// Custome Header File
#include "message.h"

// Get Sockaddr, IPv4 or IPv6:
void * get_in_addr(struct sockaddr * sa) {
    if (sa -> sa_family == AF_INET) {
        return &(((struct sockaddr_in *)sa) -> sin_addr);
    }
    return &(((struct sockaddr_in6 *)sa) -> sin6_addr);
}

// Define Constants
#define INVALID_SOCKET -1

// Buffer For Sending And Receiving
char buffer[BUFFER_SIZE];
char recv_buffer[BUFFER_SIZE];

// Supported Commands
const char * LOGIN_COMMAND = "/login";
const char * LOGOUT_COMMAND = "/logout";
const char * JOIN_SESSION_COMMAND = "/joinsession";
const char * LEAVE_SESSION_COMMAND = "/leavesession";
const char * CREATE_SESSION_COMMAND = "/createsession";
const char * LIST_COMMAND = "/list";
const char * QUIT_COMMAND = "/quit";
const char * PRIVATE_MESSAGE_COMMAND = "/dm";

// Semaphore
sem_t semaphore_join_session;
sem_t semaphore_create_session;
sem_t semaphore_list;
sem_t semaphore_message;

// User Name
char my_username[MAX_NAME];

// Structure To Remember Session Joined
typedef struct session_linked_list {
    unsigned int session_ID;
    struct session_linked_list * next;
} session_linked_list;

// Print Session Linked List
void session_linked_list_print(session_linked_list * in_session_list) {
    // Variable Declaration
    session_linked_list * current_ptr = in_session_list;
    session_linked_list * previous_ptr = NULL;
    
    // Iterate Through Entire List
    while (current_ptr != NULL) {
        fprintf(stdout, "Session ID Already Joined: %d\n", current_ptr -> session_ID);

        previous_ptr = current_ptr;
        current_ptr = current_ptr -> next;
    }

    // Return Nothing
    return;
}

// Insert Into Session Linked List
session_linked_list * session_linked_list_insert(session_linked_list * in_session_list, unsigned int session_ID) {
    // Create New Session
    session_linked_list * new_session_linked_list = (session_linked_list *) malloc(sizeof(session_linked_list));
    new_session_linked_list -> session_ID = session_ID;
    new_session_linked_list -> next = in_session_list;

    return new_session_linked_list;
}

// User In Session Boolean
bool in_session = false;
session_linked_list * in_session_list = NULL;

// Receive Subroutine To Handle Receiving Message From Server
void * receive(void * socketFD_void_ptr) {
    // Receive May Get Types: JN_ACK (5), JN_NAK (6), NS_ACK (9), MESSAGE (10), QU_ACK (12)
    int * socketFD_ptr = (int *) socketFD_void_ptr;
    int bytes_received;
    message message_received;

    // Receive Loop
    for (;;) {
        // Clear Buffer
        memset(recv_buffer, 0, BUFFER_SIZE);
        memset(&message_received, 0, sizeof(message));

        // Receiving Message From Socket
        if ((bytes_received = recv(* socketFD_ptr, recv_buffer, BUFFER_SIZE - 1, 0)) == -1) {
            fprintf(stderr, "Client: recv Error\n");
            return NULL;
        }

        // Receiving Empty Message
        if (bytes_received == 0) continue;

        // Set Null Terminator To Buffer
        recv_buffer[bytes_received] = 0;

        // Convert String Message To Message
        int index = 0;

        // Multiple Messages Might Be Stuck In Receiving Buffer, Loop To Extract All Individual Message
        while ((index = deserialization_multiple(recv_buffer, &message_received, index)) != -1) {
            // Print Received Message
            #ifdef fprintf_message
                fprintf(stdout, "Received Message: \"%s\"\n", recv_buffer);
            #endif
            
            // Check Message Type
            // Join Session Acknowledgement
            if (message_received.type == 5) {
                fprintf(stdout, "Client: Successfully Joined Session %s\n", message_received.data);
                in_session_list = session_linked_list_insert(in_session_list, atoi(message_received.data));
                fprintf(stdout, "Client: Below Are Sessions You Have Already Joined\n");
                session_linked_list_print(in_session_list);
                in_session = true;
                sem_post(&semaphore_join_session);
            // Join Session Negative Acknowledgement
            } else if (message_received.type == 6) {
                fprintf(stdout, "Client: Join Session Failed. Detail: %s\n", message_received.data);
                sem_post(&semaphore_join_session);
            // New Session Acknowledgement
            } else if (message_received.type == 9) {
                fprintf(stdout, "Client: Successfully Created And Joined Session %s\n", message_received.data);
                in_session_list = session_linked_list_insert(in_session_list, atoi(message_received.data));
                fprintf(stdout, "Client: Below Are Sessions You Have Already Joined\n");
                session_linked_list_print(in_session_list);
                in_session = true;
                sem_post(&semaphore_create_session);
            // List Acknowledgement
            } else if (message_received.type == 12) {
                fprintf(stdout, "User ID\t\tSession IDs\n%s", message_received.data);
                sem_post(&semaphore_list);
            // Message Acknowledgement
            } else if (message_received.type == 10) {
                fprintf(stdout, "%s: %s\n", message_received.source, message_received.data);

                // Extract Sender Name
                char sender_username[MAX_NAME];
                strncpy(sender_username, message_received.source, strlen(my_username));
                sender_username[strlen(my_username)] = '\0';

                // Sender Is User Itself
                if (strcmp(sender_username, my_username) == 0) {
                    // Check Semaphore Value
                    int semaphore_message_value;
                    if (sem_getvalue(&semaphore_message, &semaphore_message_value) != 0) {
                        fprintf(stderr, "Client: Sem GetValue Error\n");
                    }

                    // Synchronization
                    if (semaphore_message_value == 0) {
                        sem_post(&semaphore_message);
                    }
                }
            // Private Message Acknowledgement
            } else if (message_received.type == 14) {
                fprintf(stdout, "Client: Private Message Failed. Detail: %s\n", message_received.data);
            // Invalid Message From Server
            } else {
                fprintf(stdout, "Client: Unexpected Message Received: Type: %d, Source: %d, Data: %s\n", message_received.type, message_received.source, message_received.data);
            }

            // Flush Standard Output Stream
            fflush(stdout);

            // Clear Message Received
            memset(&message_received, 0, sizeof(message));
        }
    } 

    // Finished Executing Receive Function
    return NULL;
}

// Login Subroutine
void login(char * character_ptr, int * socketFD_ptr, pthread_t * receive_thread_ptr) {
    // Variable Declaration
    char * client_username, * client_password, * server_IP, * server_port;

    // Tokenize Command
    character_ptr = strtok(NULL, " ");
    client_username = character_ptr;
    character_ptr = strtok(NULL, " ");
    client_password = character_ptr;
    character_ptr = strtok(NULL, " ");
    server_IP = character_ptr;
    character_ptr = strtok(NULL, " ");
    server_port = character_ptr;

    // Input Check
    if (client_username == NULL || client_password == NULL || server_IP == NULL || server_port == NULL) {
        fprintf(stdout, "Client: Invalid Login Command, Usage: /login <client_username> <client password> <server_ip> <server_port>\n");
        return;
    } else if (* socketFD_ptr != INVALID_SOCKET) {
        fprintf(stdout, "Client: You Can Only Login To 1 Server Simultaneously\n");
        return;
    } else {
        // Prepare To Connect Through TCP Protocol
        int rv;
        struct addrinfo hints;
        struct addrinfo * server_info;
        struct addrinfo * p;
        char s[INET6_ADDRSTRLEN];
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;

        // Get Server Address Info
        if ((rv = getaddrinfo(server_IP, server_port, &hints, &server_info)) != 0) {
            fprintf(stderr, "Client: getaddrinfo: %s\n", gai_strerror(rv));
            return;
        }

        // TCP Handshake
        for (p = server_info; p != NULL; p = p -> ai_next) {
            if ((* socketFD_ptr = socket(p -> ai_family, p -> ai_socktype, p -> ai_protocol)) == -1) {
                fprintf(stderr, "Client: socket Error\n");
                continue;
            }

            if (connect(* socketFD_ptr, p -> ai_addr, p -> ai_addrlen) == -1) {
                close(* socketFD_ptr);
                fprintf(stderr, "Client: connect Error\n");
                continue;
            }

            break;
        }

        // Failed To Connect
        if (p == NULL) {
            fprintf(stderr, "Client: Failed To Connect From addrinfo\n");
            close(* socketFD_ptr);
            * socketFD_ptr = INVALID_SOCKET;
            return;
        }

        // Convert IP Structure
        inet_ntop(p -> ai_family, get_in_addr((struct sockaddr *) p -> ai_addr), s, sizeof(s));
        fprintf(stdout, "Client: Connecting To %s\n", s);
        freeaddrinfo(server_info);

        // Send Login Request
        int bytes_sent;
        int bytes_received;
        message message_sent;
        message message_received;
        message_sent.type = 0;
        strncpy(message_sent.source, client_username, MAX_NAME);
        strncpy(message_sent.data, client_password, MAX_DATA);
        message_sent.size = strlen(message_sent.data);

        serialization(&message_sent, buffer);
        buffer[strlen(buffer)] = '\0';

        // Sending Message Through TCP Connection
        if ((bytes_sent = send(* socketFD_ptr, buffer, strlen(buffer) + 1, 0)) == -1) {
            fprintf(stderr, "Client: send Error\n");
            close(* socketFD_ptr);
            * socketFD_ptr = INVALID_SOCKET;
            return;
        }

        // Receiving Server Reply
        if ((bytes_received = recv(* socketFD_ptr, buffer, BUFFER_SIZE - 1, 0)) == -1) {
            fprintf(stderr, "Client: recv Error\n");
            close(* socketFD_ptr);
            * socketFD_ptr = INVALID_SOCKET;
            return;
        }

        // Convert Buffer To Message
        buffer[bytes_received] = '\0';
        deserialization(buffer, &message_received);

        // Check Message
        if (message_received.type == 1 && pthread_create(receive_thread_ptr, NULL, receive, socketFD_ptr) == 0) {
            fprintf(stdout, "Client: Login Successfully\n");
            strncpy(my_username, message_received.source, MAX_NAME);
        } else if (message_received.type == 2) {
            fprintf(stdout, "Client: Login Failed. Detail: %s\n", message_received.data);
            close(* socketFD_ptr);
            * socketFD_ptr = INVALID_SOCKET;
            return;
        } else {
            fprintf(stdout, "Client: Unexpected Message Received: Type: %d, Source: %d, Data: %s\n", message_received.type, message_received.source, message_received.data);
            close(* socketFD_ptr);
            * socketFD_ptr = INVALID_SOCKET;
            return;
        }
    }

    // DEBUGGING USE
    fprintf(stdout, "Client: Login Function Finished Executing\n");
}

// Logout Subroutine
void logout(int * socketFD_ptr, pthread_t * receive_thread_ptr) {
    // Check If TCP Connection Established
    if (* socketFD_ptr == INVALID_SOCKET) {
        fprintf(stdout, "Client: You Have Not Logged Into Any Server\n");
        return;
    }

    // Variable Declaration
    int bytes_sent;
    message message_sent;
    memset(&message_sent, 0, sizeof(message));
    message_sent.type = 3;
    message_sent.size = 0;
    serialization(&message_sent, buffer);
    buffer[strlen(buffer)] = '\0';

    // Sending Message To Server
    if ((bytes_sent = send(* socketFD_ptr, buffer, strlen(buffer) + 1, 0)) == -1) {
        fprintf(stderr, "Client: send Error\n");
        return;
    }

    // Close Receive Thread
    if (pthread_cancel(* receive_thread_ptr)) {
        fprintf(stderr, "Client: pthread_cancel Error\n");
    } else {
        fprintf(stdout, "Client: pthread_cancel Successfully\n");
    }

    // Close Socket
    in_session = false;
    close(* socketFD_ptr);
    *socketFD_ptr = INVALID_SOCKET;
}

// Join Session Subroutine
void join_session(char * character_ptr, int * socketFD_ptr) {
    // Check If TCP Connection Established
    if (* socketFD_ptr == INVALID_SOCKET) {
        fprintf(stdout, "Client: You Have Not Logged Into Any Server\n");
        return;
    } 
    else if (in_session == true) {
        fprintf(stdout, "Client: You Have Already Joined A Session\n");
    }

    // Variable Declaration
    char * session_ID;
    character_ptr = strtok(NULL, " ");
    session_ID = character_ptr;

    // Check Session ID
    if (session_ID == NULL) {
        fprintf(stdout, "Client: Usage: /joinsession <session_id>\n");
    } else {
        // Variable Declaration
        int bytes_sent;
        message message_sent;
        memset(&message_sent, 0, sizeof(message));
        message_sent.type = 4;
        strncpy(message_sent.data, session_ID, MAX_DATA);
        message_sent.size = strlen(message_sent.data);

        serialization(&message_sent, buffer);
        buffer[strlen(buffer)] = '\0';

        // Send Message To Server
        if ((bytes_sent = send(* socketFD_ptr, buffer, strlen(buffer) + 1, 0)) == -1) {
            fprintf(stderr, "Client: send Error\n");
            return;
        }

        // Synchronization
        sem_wait(&semaphore_join_session);
    }
}

// Leave Session Subroutine
void leave_session(int * socketFD_ptr) {
    // Check If TCP Connection Established
    if (* socketFD_ptr == INVALID_SOCKET) {
        fprintf(stdout, "Client: You Have Not Logged Into Any Server\n");
        return;
    } else if (in_session == false) {
        fprintf(stdout, "Client: You Have Not Joined Any Session\n");
        return;
    }

    // Variable Declaration
    int bytes_sent;
    message message_sent;
    memset(&message_sent, 0, sizeof(message));
    message_sent.type = 7;
    message_sent.size = 0;

    serialization(&message_sent, buffer);
    buffer[strlen(buffer)] = '\0';

    // Send Message To Server
    if ((bytes_sent = send(* socketFD_ptr, buffer, strlen(buffer) + 1, 0)) == -1) {
        fprintf(stderr, "Client: send Error\n");
        return;
    }

    // User No Longer In Session
    in_session = false;
}

// Create Session Subroutine
void create_session(char * character_ptr, int * socketFD_ptr) {
    // Check If TCP Connection Established
    if (* socketFD_ptr == INVALID_SOCKET) {
        fprintf(stdout, "Client: You Have Not Logged Into Any Server\n");
        return;
    } else if (in_session == true) {
        fprintf(stdout, "Client: You Have Already Joined A Session\n");
        return;
    }

    // Variable Declaration
    char * session_ID;
    character_ptr = strtok(NULL, " ");
    session_ID = character_ptr;

    // Check Session ID
    if (session_ID == NULL) {
        fprintf(stdout, "Client: Usage: /createsession <session_id>\n");
    } else {
        // Variable Declaration
        int bytes_sent;
        message message_sent;
        memset(&message_sent, 0, sizeof(message));
        message_sent.type = 8;
        strncpy(message_sent.data, session_ID, MAX_DATA);
        message_sent.size = strlen(message_sent.data);

        serialization(&message_sent, buffer);
        buffer[strlen(buffer)] = '\0';

        // Send Message To Server
        if ((bytes_sent = send(* socketFD_ptr, buffer, strlen(buffer) + 1, 0)) == -1) {
            fprintf(stderr, "Client: send Error\n");
            return;
        }

        // Synchronization
        sem_wait(&semaphore_create_session);
    }
}

// Private Message Subroutine
void private_message(char * character_ptr, int * socketFD_ptr) {
    // Check If TCP Connection Established
    if (* socketFD_ptr == INVALID_SOCKET) {
        fprintf(stdout, "Client: You Have Not Logged Into Any Server\n");
        return;
    }

    // Variable Declaration
    int bytes_sent;
    message message_sent;
    memset(&message_sent, 0, sizeof(message));
    message_sent.type = 13;

    // Extract Private Message
    character_ptr = strtok(NULL, "\"");
    fprintf(stdout, "Client: Character Ptr: %s\n", character_ptr);
    strncpy(message_sent.source, character_ptr, MAX_DATA);
    character_ptr = strtok(NULL, "\"");
    fprintf(stdout, "Client: Character Ptr: %s\n", character_ptr);
    character_ptr = strtok(NULL, "\"");
    fprintf(stdout, "Client: Character Ptr: %s\n", character_ptr);
    strncpy(message_sent.data, character_ptr, MAX_DATA);
    message_sent.size = strlen(message_sent.data);

    serialization(&message_sent, buffer);
    buffer[strlen(buffer)] = '\0';

    // Send Message To Server
    if ((bytes_sent = send(* socketFD_ptr, buffer, strlen(buffer) + 1, 0)) == -1) {
        fprintf(stderr, "Client: send Error\n");
        return;
    }
}

// List Subroutine
void list(int * socketFD_ptr) {
    // Check If TCP Connection Established
    if (* socketFD_ptr == INVALID_SOCKET) {
        fprintf(stdout, "Client: You Have Not Logged Into Any Server\n");
        return;
    }

    // Variable Declaration
    int bytes_sent;
    message message_sent;
    memset(&message_sent, 0, sizeof(message));
    message_sent.type = 11;
    message_sent.size = 0;
    
    serialization(&message_sent, buffer);
    buffer[strlen(buffer)] = '\0';

    // Send Message To Server
    if ((bytes_sent = send(* socketFD_ptr, buffer, strlen(buffer) + 1, 0)) == -1) {
        fprintf(stderr, "Client: send Error\n");
        return;
    }

    // Synchronization
    sem_wait(&semaphore_list);
}

// Send Message Subroutine
void send_message(int * socketFD_ptr) {
    // Check If TCP Connection Established
    if (* socketFD_ptr == INVALID_SOCKET) {
        fprintf(stdout, "Client: You Have Not Logged Into Any Server\n");
        return;
    } else if (in_session == false) {
        fprintf(stdout, "Client: You Have Not Joined A Session Yet\n");
        return;
    }

    // Variable Declaration
    int bytes_sent;
    message message_sent;
    memset(&message_sent, 0, sizeof(message));
    message_sent.type = 10;
    strncpy(message_sent.data, buffer, MAX_DATA);
    message_sent.size = strlen(message_sent.data);
    
    serialization(&message_sent, buffer);
    buffer[strlen(buffer)] = '\0';

    // Send Message To Server
    if ((bytes_sent = send(* socketFD_ptr, buffer, strlen(buffer) + 1, 0)) == -1) {
        fprintf(stderr, "Client: send Error\n");
        return;
    }

    // Synchronization
    sem_wait(&semaphore_message);
}

// Main Function
int main(int argc, char * argv[]) {
    // Variable Declaration
    int toklen;
    int socketFD = INVALID_SOCKET;
    char * character_ptr;
    pthread_t receive_thread;
    
    // Initialize Semaphore
    sem_init(&semaphore_join_session, 0, 0);
    sem_init(&semaphore_create_session, 0, 0);
    sem_init(&semaphore_list, 0, 0);
    sem_init(&semaphore_message, 0, 0);

    // Continuously Take Client Command
    for (;;) {
        fgets(buffer, BUFFER_SIZE - 1, stdin);
        buffer[strcspn(buffer, "\n")] = 0;
        char buffer_copy[BUFFER_SIZE];
        strcpy(buffer_copy, buffer);
        character_ptr = buffer;

        // Iterate Through Buffer
        // Skip Leading White Space
        while (* character_ptr == ' ') character_ptr++;

        // Ignore Empty Command
        if (* character_ptr == 0) continue;

        // Parse Command
        character_ptr = strtok(buffer, " ");
        toklen = strlen(character_ptr);

        // Parsing Each Command
        // Login Command
        if (strcmp(character_ptr, LOGIN_COMMAND) == 0) {
            login(character_ptr, &socketFD, &receive_thread);
        // Logout Command
        } else if (strcmp(character_ptr, LOGOUT_COMMAND) == 0) {
            logout(&socketFD, &receive_thread);
        // Join Session Command
        } else if (strcmp(character_ptr, JOIN_SESSION_COMMAND) == 0) {
            join_session(character_ptr, &socketFD);
        // Leave Session Command
        } else if (strcmp(character_ptr, LEAVE_SESSION_COMMAND) == 0) {
            leave_session(&socketFD);
        // Create Session Command
        } else if (strcmp(character_ptr, CREATE_SESSION_COMMAND) == 0) {
            create_session(character_ptr, &socketFD);
        // List Command
        } else if (strcmp(character_ptr, LIST_COMMAND) == 0) {
            list(&socketFD);
        // Private Message Command
        } else if (strcmp(character_ptr, PRIVATE_MESSAGE_COMMAND) == 0) {
            strcpy(buffer, buffer_copy);
            fprintf(stdout,  "Client: Sending Message \"%s\"\n", buffer);

            private_message(character_ptr, &socketFD);
        // Quit Command
        } else if (strcmp(character_ptr, QUIT_COMMAND) == 0) {
            logout(&socketFD, &receive_thread);
            break;
        // Invalid Command
        } else if (character_ptr[0] == '/') {
            fprintf(stdout, "Client: Invalid Command \"%s\"\n", character_ptr);
        // Sending Message
        } else {
            strcpy(buffer, buffer_copy);
            fprintf(stdout,  "Client: Sending Message \"%s\"\n", buffer);
            send_message(&socketFD);
        }
    }

    // Indicating Quit Successfully
    fprintf(stdout, "Client: You Have Quit Successfully!\n");

    // Successfully Executed Main Function
    return 0;
}