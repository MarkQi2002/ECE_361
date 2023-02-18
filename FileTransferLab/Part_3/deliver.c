// Include Important Modules
#include "common.h"

// Function Prototype
void packetToMessage(packet * packet, char * message);

// Function To Convert Packet To Message
void packetToMessage(packet * packet, char * message) {
    // Inserting Data Into String
    int index = 0;
    sprintf(message, "%d", packet -> total_frag);
    index = strlen(message);
    sprintf(message + index, "%c", ':');
    ++index;

    sprintf(message + index, "%d", packet -> frag_no);
    index = strlen(message);
    sprintf(message + index, "%c", ':');
    ++index;

    sprintf(message + index, "%d", packet -> size);
    index = strlen(message);
    sprintf(message + index, "%c", ':');
    ++index;

    sprintf(message + index, "%s", packet -> filename);
    index += strlen(packet -> filename);
    sprintf(message + index, "%c", ':');
    ++index;

    memcpy(message + index, packet -> filedata, sizeof(char) * packet -> size);
}

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
    char * buffer = (char *) malloc(sizeof(char) * BUFFER_SIZE);
    char * filename = (char *) malloc(sizeof(char) * BUFFER_SIZE);

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

    // Start Time
    struct timespec start_time;
    clock_gettime(CLOCK_REALTIME, &start_time);

    // Sending Message
    int number_bytes;
    if ((number_bytes = sendto(socketFD, "ftp", strlen("ftp"), 0, (struct sockaddr *) &server_addr, sizeof(server_addr))) == -1) {
        fprintf(stderr, "Sendto Error\n");
        exit(1);
    }

    // Recvfrom
    memset(buffer, 0, sizeof(char) * BUFFER_SIZE);
    socklen_t server_addr_length;
    if (recvfrom(socketFD, buffer, BUFFER_SIZE, 0, (struct sockaddr *) &server_addr, &server_addr_length) == -1) {
        fprintf(stderr, "Recvfrom Error\n");
        exit(1);
    }

    // End Time
    struct timespec end_time;
    clock_gettime(CLOCK_REALTIME, &end_time);
    double elapsed_time = (end_time.tv_sec - start_time.tv_sec) + (end_time.tv_nsec - start_time.tv_nsec) / 1e9;
    fprintf(stdout, "Elapsed Time: %f Seconds\n", elapsed_time);

    // File Transfer Can Start
    if (strcmp(buffer, "yes") == 0) {
        fprintf(stdout, "A file transfer can start.\n");
    }
    
    // Open File
    FILE * fp;
    if ((fp = fopen(filename, "r")) == NULL) {
        perror("Deliver: cannot open the target file");
        exit(1);
    }

    // Calculate total number of packets required
    fseek(fp, 0, SEEK_END);
    long int total_len = ftell(fp);
    int total_frag = total_len / 1000 + 1;
    fseek(fp, 0, SEEK_SET); 

    // Transfer File
    packet * packet_list = (packet *) malloc(sizeof(packet) * total_frag);
    char ** message_list = (char **) malloc(sizeof(char *) * total_frag);
    int ret = -1;
    int frag_no = 1;

    // Extracting File Until Empty
    for (int i = 0; i < total_frag; ++i) {
        // Organize Packet
        packet_list[frag_no - 1].total_frag = total_frag;
        packet_list[frag_no - 1].frag_no = frag_no;
        packet_list[frag_no - 1].size = (int) fread(packet_list[frag_no - 1].filedata, sizeof(char), 1000, fp);
        packet_list[frag_no - 1].filename = filename;
        
        // Initialize Message String
        message_list[frag_no - 1] = (char *) malloc(sizeof(char) * MAX_MESSAGE_LENGTH);
        memset(message_list[frag_no - 1], 0, sizeof(char) * MAX_MESSAGE_LENGTH);

        // Convert Packet To Message
        packetToMessage(&packet_list[frag_no - 1], message_list[frag_no - 1]);

        // Increment Frag Number
        ++frag_no;
    }

    // Sending File
    for (int i = 0; i < total_frag; ++i) {
        // Sending Message
        if ((number_bytes = sendto(socketFD, message_list[i], MAX_MESSAGE_LENGTH, 0, (struct sockaddr *) &server_addr, sizeof(server_addr))) == -1) {
            fprintf(stderr, "Sendto Error\n");
            exit(1);
        }

        // Recvfrom
        memset(buffer, 0, sizeof(char) * BUFFER_SIZE);
        socklen_t server_addr_length;
        if (recvfrom(socketFD, buffer, BUFFER_SIZE, 0, (struct sockaddr *) &server_addr, &server_addr_length) == -1) {
            fprintf(stderr, "Recvfrom Error\n");
            exit(1);
        }

        // File Transfer Can Start
        if (strcmp(buffer, "ACK") != 0) {
            --i;
        }
    }

    // Indicate File Sent Successfully
    fprintf(stdout, "File %s Sent Successfully\nTerminating Program...\n", filename);

    // Close Socket
    close(socketFD);
    
    // Close File
    fclose(fp);

    // Free Allocated Memory
    free(buffer);
    free(filename);
    free(packet_list);
    for (int i = 0; i < total_frag; ++i) {
        free(message_list[i]);
    }
    free(message_list);

    // Successfully Executed
    return 0;
}