// Include Important Modules
#include "common.h"

// Main Function
int main(int argc, char* argv[]) {
    // Input Arguments
    if (argc != 3) {
        fprintf(stdout, "Usage: deliver <server address> <server port number>\n");
        exit(0);
    }

    // Extract Port Number
    int port = atoi(argv[2]);

    // Open UDP Socket
    int socketFD;
    if ((socketFD = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        fprintf(stderr, "Socket File Descriptor Error\n");
        exit(1);
    }

    // Bind Associates Socket With An Address
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) == 0) {
        fprintf(stderr, "inet_pton Error\n");
        exit(1);
    }

    // Recvfrom
    const int buffer_size = 1024;
    char * buffer = (char *) malloc(sizeof(char) * buffer_size);
    char * filename = (char *) malloc(sizeof(char) * buffer_size);

    // Get User Input
    fprintf(stdout, "Please Input: ftp <file name>\n");
    scanf("%s", buffer);
    scanf("%s", filename);

    // Input Control
    if (strcmp(buffer, "ftp") != 0) {
        fprintf(stderr, "Input Error\n");
        exit(1);
    } 

    // Check File Exist
    if (access(filename, F_OK) == -1) {
        fprintf(stderr, "File %s Doesn't Exist\n", filename);
        exit(1);
    } else {
        fprintf(stdout, "File '%s' Exist\n", filename);
    }

    // Free Allocated Memory
    free(filename);

    // Sending Message
    int number_bytes;
    if ((number_bytes = sendto(socketFD, "ftp", strlen("ftp"), 0, (struct sockaddr *) &server_addr, sizeof(server_addr))) == -1) {
        fprintf(stderr, "Sendto Error\n");
        exit(1);
    }

    // Recvfrom
    memset(buffer, 0, sizeof(char) * buffer_size);
    socklen_t server_addr_length;
    if (recvfrom(socketFD, buffer, buffer_size, 0, (struct sockaddr *) &server_addr, &server_addr_length) == -1) {
        fprintf(stderr, "Recvfrom Error\n");
        exit(1);
    }

    // File Transfer Can Start
    if (strcmp(buffer, "yes") == 0) {
        fprintf(stdout, "A file transfer can start.\n");
    }

    // Close Socket
    close(socketFD);

    // Free Allocated Memory
    free(buffer);

    // Successfully Executed
    return 0;
}