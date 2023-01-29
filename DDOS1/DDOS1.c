// Include Important Modules
#include "common.h"

// Main Function
int main(int argc, char* argv[]) {
    // Machine Number
    int machine_start = 201;
    int machine_end = 249;
    int port_start = 1025;
    int port_end = 65535;

    for (int machine_number = machine_start; machine_number <= machine_end; ++machine_number) {
        fprintf(stdout, "Running UG Machine %d\n", machine_number);
        for (int port_number = port_start; port_number <= port_end; ++port_number) {
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
            server_addr.sin_port = htons(port_number);
            // fprintf(stdout, "Port Number: %d\n", port_number);

            char ip_address[1024] = "128.100.13.";
            char machine_number_string[4];
            sprintf(machine_number_string, "%d", machine_number);
            strcat(ip_address, machine_number_string);
            // fprintf(stdout, "IP Address: %s\n", ip_address);

            if (inet_pton(AF_INET, ip_address, &server_addr.sin_addr) == 0) {
                fprintf(stderr, "inet_pton Error\n");
                exit(1);
            }

            // Sending Message
            int number_bytes;
            if ((number_bytes = sendto(socketFD, "ftp", strlen("ftp"), 0, (struct sockaddr *) &server_addr, sizeof(server_addr))) == -1) {
                fprintf(stderr, "Sendto Error\n");
                exit(1);
            }

            // Close Socket
            close(socketFD);
        }
    }

    // Successfully Executed
    return 0;
}