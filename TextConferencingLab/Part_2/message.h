// Conditional Compilation
#ifndef _MESSAGE_H_
#define _MESSAGE_H_

// stdio.h -> Standard Input And Output Library
// stdlib.h -> General Purpose Standard Library
// string.h -> String Library
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Defining Constants
#define MAX_NAME 1024
#define MAX_DATA 1024
#define BUFFER_SIZE 4096

// Define Message Structure
typedef struct message {
    unsigned int type;
    unsigned int size;
    unsigned char source[MAX_NAME];
    unsigned char data[MAX_DATA];
} message;

// Serialization Function (Converting Packet To Message)
void serialization(struct message * packet, char * message) {
    // Initialize Message Memory
    memset(message, 0, BUFFER_SIZE * sizeof(char));

    // Convert Type
    int position = 0;
    sprintf(message, "%d", packet -> type);
    position = strlen(message);
    sprintf(message + position, "%c", ':');
    position += 1;

    // Conver File Size
    sprintf(message + position, "%d", packet -> size);
    position = strlen(message);
    sprintf(message + position, "%c", ':');
    position += 1;

    // Convert Source
    sprintf(message + position, "%s", packet -> source);
    position = strlen(message);
    sprintf(message + position, "%c", ':');
    position += 1;

    // Convert Filedata
    sprintf(message + position, "%s", packet -> data);
} 

// Deserialization Function (Converting Message To Packet)
void deserialization(char * buffer, struct message * packet) {
    // Variable Declaration
    int colon_one, colon_two, colon_three;

    // Find Type
    for (int i = 0; i < BUFFER_SIZE; i++) {
        if (buffer[i] == ':') {
            colon_one = i;
            break;
        }
    }

    char type_str[colon_one + 1];
    for (int i = 0; i < colon_one; i++) {
        type_str[i] = buffer[i];
    }
    type_str[colon_one] = '\0';
    packet -> type = atoi(type_str);

    // Find File Size
    for (int i = colon_one + 1; i < BUFFER_SIZE; i++) {
        if (buffer[i] == ':') {
            colon_two = i;
            break;
        }
    }

    char size_str[colon_two - colon_one];
    for (int i = 0; i < colon_two - colon_one - 1; i++) {
        size_str[i] = buffer[colon_one + 1 + i];
    }
    size_str[colon_two - colon_one - 1] = '\0';
    packet -> size = atoi(size_str);

    // Find Source
    for (int i = colon_two + 1; i < BUFFER_SIZE; i++) {
        if (buffer[i] == ':') {
            colon_three = i;
            break;
        }
    }

    for (int i = 0; i < colon_three - colon_two - 1; i++) {
        packet -> source[i] = buffer[colon_two + 1 + i];
    }
    packet -> source[colon_three - colon_two - 1] = '\0';

    // Find Filedata
    for (int i = 0; i < packet -> size; i++) {
        packet -> data[i] = buffer[colon_three + 1 + i];
    }
    // packet -> data[packet -> size] = '\0';
}

// Deserialization Function (Converting Stacked Message To Multiple Packets)
int deserialization_multiple(char * buffer, struct message * packet, int start_index) {
    // See If There Are More Messages
    bool more_message = false;
    for (int i = start_index; i < BUFFER_SIZE; i++) {
        if (buffer[i] == ':') {
            more_message = true;
            break;
        }
    }

    // If No More Message Return -1
    if (more_message == false) return -1;

    // Variable Declaration
    int colon_one, colon_two, colon_three;

    // Find Type
    for (int i = start_index; i < BUFFER_SIZE; i++) {
        if (buffer[i] == ':') {
            colon_one = i;
            break;
        }
    }

    char type_str[colon_one + 1];
    for (int i = 0; i < colon_one; i++) {
        type_str[i] = buffer[i];
    }
    type_str[colon_one] = '\0';
    packet -> type = atoi(type_str);

    // Find File Size
    for (int i = colon_one + 1; i < BUFFER_SIZE; i++) {
        if (buffer[i] == ':') {
            colon_two = i;
            break;
        }
    }

    char size_str[colon_two - colon_one];
    for (int i = 0; i < colon_two - colon_one - 1; i++) {
        size_str[i] = buffer[colon_one + 1 + i];
    }
    size_str[colon_two - colon_one - 1] = '\0';
    packet -> size = atoi(size_str);

    // Find Source
    for (int i = colon_two + 1; i < BUFFER_SIZE; i++) {
        if (buffer[i] == ':') {
            colon_three = i;
            break;
        }
    }

    for (int i = 0; i < colon_three - colon_two - 1; i++) {
        packet -> source[i] = buffer[colon_two + 1 + i];
    }
    packet -> source[colon_three - colon_two - 1] = '\0';

    // Find Filedata
    for (int i = 0; i < packet -> size; i++) {
        packet -> data[i] = buffer[colon_three + 1 + i];
    }
    
    // Return Ending Index
    return colon_three + 1 + packet -> size;
}

// Conditional Compilation
#endif