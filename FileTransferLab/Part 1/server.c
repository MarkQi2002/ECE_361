// Include Important Modules
#include "common.h"

// Main Function
int main(int argc, char * argv[]) {
    // Input Arguments
    if (argc != 2) {
        fprintf(stdout, "Usage: server <UDP listen port>\n");
        exit(0);
    }

    // Extract Port Number
    int port = atoi(argv[1]);

    // Open UDP Socket
    int socketFD;
    if ((socketFD = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        fprintf(stdout, "Socket File Descriptor Error\n");
        exit(0);
    }
    
    // Bind Associates Socket With An Address
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Binding To Socket
    if (bind(socketFD, (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1) {
        fprintf(stdout, "Binding Error\n");
        exit(0);
    }

    // Recvfrom
    const int buffer_size = 1024;
    char * buffer = (char *) malloc(sizeof(char) * buffer_size);

    struct sockaddr_in client_addr;
    socklen_t client_addr_length = sizeof(client_addr);
    if (recvfrom(socketFD, buffer, buffer_size, 0, (struct sockaddr *) &client_addr, &client_addr_length) == -1) {
        fprintf(stdout, "Recvfrom Error\n");
        exit(0);
    }

    // Send Message Back To Client Accordingly
    if (strcmp(buffer, "ftp") == 0) {
        if ((sendto(socketFD, "yes", strlen("yes"), 0, (struct sockaddr *) &client_addr, client_addr_length)) == -1) {
            fprintf(stdout, "Yes Sendto Error\n");
            exit(0);
        }
    } else {
        if (sendto(socketFD, "no", strlen("no"), 0, (struct sockaddr *) &client_addr, client_addr_length) == -1) {
            fprintf(stdout, "No Sendto Error\n");
            exit(0);
        }
    }

    // Close Socket
    close(socketFD);

    // Successfully Executed
    return 0;
}