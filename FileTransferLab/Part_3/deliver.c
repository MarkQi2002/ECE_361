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
    double initial_RTT = (end_time.tv_sec - start_time.tv_sec) + (end_time.tv_nsec - start_time.tv_nsec) / 1e9;
    fprintf(stdout, "Elapsed Time: %f Seconds\n", initial_RTT);

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

    // Timer Variables
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    if (setsockopt(socketFD, SOL_SOCKET, SO_RCVTIMEO, (void *) &timeout, sizeof(timeout)) < 0) {
        fprintf(stderr, "Set Socket Option Error\n");
        exit(1);
    }

    double estimated_RTT = initial_RTT;
    double deviation_RTT = 0;
    clock_t start_time_RTT = 0;
    clock_t end_time_RTT = 0;
    double measured_RTT = 0;
    double timeout_interval = 0;
    int time_sent = 0;

    // Sending File
    for (int i = 0; i < total_frag; ++i) {
        // Timer
        start_time_RTT = clock();

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
            --i;
        // Acknowledgement Received
        } else if (strcmp(buffer, "ACK") != 0) {
            --i;
        // Setting RTT Time
        } else {
            end_time_RTT = clock();
            measured_RTT = end_time_RTT - start_time_RTT;
            estimated_RTT = 0.875 * estimated_RTT + 0.125 * measured_RTT;
            deviation_RTT = 0.75 * deviation_RTT + 0.25 * (measured_RTT - estimated_RTT);
            timeout_interval = 20 * estimated_RTT + 4 * deviation_RTT;
            timeout_interval = timeout_interval / CLOCKS_PER_SEC;
            timeout.tv_sec = timeout_interval / 1;
            timeout.tv_usec = (timeout_interval - timeout.tv_sec) * 1000000;
            if (setsockopt(socketFD, SOL_SOCKET, SO_RCVTIMEO, (void *) &timeout, sizeof(timeout)) < 0) {
                fprintf(stderr, "Set Socket Option Error\n");
                exit(1);
            }
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