// Include Important Modules
#include "common.h"

// Function Prototype
void messageToPacket(char * message, packet * packet);

// Function To Convert Message To Packet
void messageToPacket(char* buffer, struct packet* packet) {
    //printf("%s\n", buffer);
    int colon_one, colon_two, colon_three, colon_four;

    // find total_frag
    for (int i = 0; i < BUFFER_SIZE; i++){
        if (buffer[i] == ':'){
            colon_one = i;
            break;
        }
    }

    char total_frag_str[colon_one + 1];
    for (int i = 0; i < colon_one; i++){
        total_frag_str[i] = buffer[i];
    }
    total_frag_str[colon_one] = '\0';
    packet->total_frag = atoi(total_frag_str);

    // find frag_no
    for (int i = colon_one + 1; i < BUFFER_SIZE; i++){
        if (buffer[i] == ':'){
            colon_two = i;
            break;
        }
    }

    char frag_no_str[colon_two - colon_one];
    for (int i = 0; i < colon_two - colon_one - 1; i++){
        frag_no_str[i] = buffer[colon_one + 1 + i];
    }
    frag_no_str[colon_two - colon_one - 1] = '\0';
    packet->frag_no = atoi(frag_no_str);

    // find file size
    for (int i = colon_two + 1; i < BUFFER_SIZE; i++){
        if (buffer[i] == ':'){
            colon_three = i;
            break;
        }
    }

    char size_str[colon_three - colon_two];
    for (int i = 0; i < colon_three - colon_two - 1; i++){
        size_str[i] = buffer[colon_two + 1 + i];
    }
    size_str[colon_three - colon_two - 1] = '\0';
    packet->size = atoi(size_str);

    // find filename
    for (int i = colon_three + 1; i < BUFFER_SIZE; i++){
        if (buffer[i] == ':'){
            colon_four = i;
            break;
        }
    }

    packet->filename = (char *)malloc(sizeof(colon_four - colon_three));
    for (int i = 0; i < colon_four - colon_three - 1; i++){
        packet->filename[i] = buffer[colon_three + 1 + i];
    }
    packet->filename[colon_four - colon_three - 1] = '\0';

    // find filedata
    for (int i = 0; i < packet->size; i++){
        packet->filedata[i] = buffer[colon_four + 1 + i];
    }
    
}

// void messageToPacket(char * message, packet * packet) {
//     char * token = strtok(message, ":");
//     packet -> total_frag = atoi(token);
//     token = strtok(message, ":");
//     packet -> frag_no = atoi(token);
//     token = strtok(message, ":");
//     packet -> size = atoi(token);
//     token = strtok(message, ":");
//     packet -> filename = (char *) malloc(sizeof(char) * sizeof(token));
//     strcpy(packet -> filename, token);
//     token = strtok(message, ":");
//     memcpy(token, packet -> filedata, sizeof(char) * sizeof(token));

//     fprintf(stdout, "Message: %s\n", message);
//     fprintf(stdout, "Total Frag: %d\n", packet -> total_frag);
//     fprintf(stdout, "Frag No: %d\n", packet -> frag_no);
//     fprintf(stdout, "Size: %d\n", packet -> size);
//     fprintf(stdout, "Filename: %s\n", packet -> filename);
//     fprintf(stdout, "Filedata: %s\n", packet -> filedata);
// }

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
    int maximum_message_length = 10 + 10 + 4 + BUFFER_SIZE + MAX_PACKET_SIZE + 4;
    char * file_buffer = (char *) malloc(sizeof(char) * maximum_message_length);
    while (complete_file_transfer == 0) {
        memset(file_buffer, 0, maximum_message_length);
        if (recvfrom(socketFD, file_buffer, maximum_message_length, 0, (struct sockaddr *) &client_addr, &client_addr_length) == -1) {
            fprintf(stderr, "Recvfrom Error\n");
            exit(1);
        }

        // Receiving Packet
        packet receiving_packet;
        messageToPacket(file_buffer, &receiving_packet);

        // Creating File Descriptor
        if (save_fd == -1) {
            char receive_file_path[BUFFER_SIZE] = "Receive/";
            strcat(receive_file_path, receiving_packet.filename);
            printf("File Name : %s\n", receive_file_path);
            if (((save_fd = creat(receive_file_path, 0777)) < 0)) {
                fprintf(stderr, "Error Creating File Descriptor\n");
                exit(1);
            }
        }

        // Write To File
        fprintf(stdout, "Writing %s\n", receiving_packet.filedata);
        if (write(save_fd, receiving_packet.filedata, receiving_packet.size) < 0) {
            fprintf(stderr, "Error Saving File To save_fd %d\n", save_fd);
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
    }

    // Close Socket
    close(socketFD);
    
    // Close File Descriptor
    if (close(save_fd) < 0) {
        fprintf(stderr, "Error Closing save_fd\n");
    }

    // Free Allocated Memory
    free(buffer);

    // Successfully Executed
    return 0;
}