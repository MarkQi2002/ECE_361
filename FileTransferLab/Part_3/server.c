// Include Important Modules
#include "common.h"

// Function Prototype
void messageToPacket(char * message, packet * packet);
double uniform_rand();

// Function To Convert Message To Packet
void messageToPacket(char * message, packet * packet) {
    // Variable Declaration
    int colon_one, colon_two, colon_three, colon_four;

    // Find Total Frag
    for (int i = 0; i < MAX_MESSAGE_LENGTH; i++){
        if (message[i] == ':'){
            colon_one = i;
            break;
        }
    }

    // Copy Total Frag
    char total_frag_str[colon_one + 1];
    for (int i = 0; i < colon_one; i++){
        total_frag_str[i] = message[i];
    }
    total_frag_str[colon_one] = '\0';
    packet -> total_frag = atoi(total_frag_str);

    // Find Frag Number
    for (int i = colon_one + 1; i < MAX_MESSAGE_LENGTH; i++){
        if (message[i] == ':'){
            colon_two = i;
            break;
        }
    }

    // Copy Frag Number
    char frag_no_str[colon_two - colon_one];
    for (int i = 0; i < colon_two - colon_one - 1; i++){
        frag_no_str[i] = message[colon_one + 1 + i];
    }
    frag_no_str[colon_two - colon_one - 1] = '\0';
    packet -> frag_no = atoi(frag_no_str);

    // Find File Size
    for (int i = colon_two + 1; i < MAX_MESSAGE_LENGTH; i++){
        if (message[i] == ':'){
            colon_three = i;
            break;
        }
    }

    // Copy File Size
    char size_str[colon_three - colon_two];
    for (int i = 0; i < colon_three - colon_two - 1; i++){
        size_str[i] = message[colon_two + 1 + i];
    }
    size_str[colon_three - colon_two - 1] = '\0';
    packet -> size = atoi(size_str);

    // Find File Name
    for (int i = colon_three + 1; i < MAX_MESSAGE_LENGTH; i++){
        if (message[i] == ':'){
            colon_four = i;
            break;
        }
    }

    // Copy File Name
    packet -> filename = (char *) malloc(sizeof(colon_four - colon_three));
    for (int i = 0; i < colon_four - colon_three - 1; i++){
        packet -> filename[i] = message[colon_three + 1 + i];
    }
    packet -> filename[colon_four - colon_three - 1] = '\0';

    // Copy File Data
    for (int i = 0; i < packet -> size; i++){
        packet -> filedata[i] = message[colon_four + 1 + i];
    }
}

// Uniform Random Number Generator Between Zero And One
double uniform_rand() {
    return (double) rand() / (double) RAND_MAX;
}

// Main Function
int main(int argc, char * argv[]) {
    // Randomize Generator
    srand(time(NULL));

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
        fprintf(stderr, "Socket File Descriptor Error\n");
        exit(1);
    }
    
    // Bind Associates Socket With An Address
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Binding To Socket
    if (bind(socketFD, (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1) {
        fprintf(stderr, "Binding Error\n");
        exit(1);
    }

    // Recvfrom
    char * buffer = (char *) malloc(sizeof(char) * BUFFER_SIZE);

    struct sockaddr_in client_addr;
    socklen_t client_addr_length = sizeof(client_addr);
    if (recvfrom(socketFD, buffer, BUFFER_SIZE, 0, (struct sockaddr *) &client_addr, &client_addr_length) == -1) {
        fprintf(stderr, "Recvfrom Error\n");
        exit(1);
    }

    // Send Message Back To Client Accordingly
    if (strcmp(buffer, "ftp") == 0) {
        if ((sendto(socketFD, "yes", strlen("yes"), 0, (struct sockaddr *) &client_addr, client_addr_length)) == -1) {
            fprintf(stderr, "Yes Sendto Error\n");
            exit(1);
        } else {
            fprintf(stdout, "Reply Message 'yes' Sent Successfully\n");
        }
    } else {
        if (sendto(socketFD, "no", strlen("no"), 0, (struct sockaddr *) &client_addr, client_addr_length) == -1) {
            fprintf(stderr, "No Sendto Error\n");
            exit(1);
        } else {
            fprintf(stdout, "Reply Message 'no' Sent Successfully\n");
        }
    }

    // Receiving File
    int complete_file_transfer = 0;
    int save_fd = -1;
    char * file_buffer = (char *) malloc(sizeof(char) * MAX_MESSAGE_LENGTH);
    char receive_file_path[BUFFER_SIZE] = "Receive/";
    FILE * fp = NULL;
    while (complete_file_transfer == 0) {
        memset(file_buffer, 0, MAX_MESSAGE_LENGTH);
        if (recvfrom(socketFD, file_buffer, MAX_MESSAGE_LENGTH, 0, (struct sockaddr *) &client_addr, &client_addr_length) == -1) {
            fprintf(stderr, "Recvfrom Error\n");
            exit(1);
        }

        // Randomly Dropping Packets
        if (uniform_rand() < 0.8) {
            continue;
        }
        // Receiving Packet
        packet receiving_packet;
        messageToPacket(file_buffer, &receiving_packet);

        // Write To File
        if (fp == NULL) {
            // Direct To Receive Directory
            strcat(receive_file_path, receiving_packet.filename);

            // Open File To Write To
            fp = fopen(receive_file_path, "w+");
            if (fp == NULL) {
                perror("Server: cannot create the target file");
                exit(1);
            }
        }
        
        // Write To File
        int byte_wrote = fwrite(receiving_packet.filedata, sizeof(char), receiving_packet.size, fp);
        if (byte_wrote != receiving_packet.size) {
            printf("fwrite error.\n");
            exit(1);
        }

        // Acknowledge Received File
        if (sendto(socketFD, "ACK", strlen("ACK"), 0, (struct sockaddr *) &client_addr, client_addr_length) == -1) {
            fprintf(stderr, "ACK Sendto Error\n");
            exit(1);
        }

        // Received All Files
        if (receiving_packet.frag_no == receiving_packet.total_frag) {
            complete_file_transfer = 1;
        }

        // Free Packet
        free(receiving_packet.filename);
    }

    // Indicate File Received Successfully
    fprintf(stdout, "File %s Received Successfully\nTerminating Program...\n", receive_file_path);

    // Close Socket
    close(socketFD);

    // Close File
    fclose(fp);

    // Free Allocated Memory
    free(buffer);
    free(file_buffer);

    // Successfully Executed
    return 0;
}