#ifndef PACKET_H_  
#define PACKEt_H_

#include <stdio.h>
#include <string.h>
#define BUFFER_SIZE 4096

typedef struct Packet {
    unsigned int total_frag;
    unsigned int frag_no;
    unsigned int size;
    char *filename;
    char filedata[1000];
} packet;

void message_to_packet(packet *p, char *recevied_buffer) {
    //extract info that is seperated by ":"
    int coloum_index[4];
    int str_length = strlen(recevied_buffer);
    int counter = 0;

    //find the index for each colon
    for(int i = 0; i < str_length; i++){
        if(recevied_buffer[i] = ':'){
            coloum_index[counter++] = i;
        }

        if(counter == 4) {
            break;
        }
    }

    //build a buffer to store info from text
    char* buffer[BUFFER_SIZE];

    //get total number of fragments
    memmove(buffer, recevied_buffer, coloum_index[0]);
    buffer[coloum_index[0]] = 0;
    p->total_frag = strtol(buffer, NULL, 10);

    //get the packet fragment number
    memmove(buffer, recevied_buffer + coloum_index[0] + 1, coloum_index[1] - coloum_index[0] - 1);
    buffer[coloum_index[1] - coloum_index[0] - 1] = 0;
    p->frag_no = strtol(buffer, NULL, 10);

    //get the size of the packet
    memmove(buffer, recevied_buffer + coloum_index[1] + 1, coloum_index[2] - coloum_index[1] - 1);
    buffer[coloum_index[2] - coloum_index[1] - 1] = 0;
    p->size = strtol(buffer, NULL, 10);

    //get the file name
    memmove(buffer, recevied_buffer + coloum_index[2] + 1, coloum_index[3] - coloum_index[2] - 1);
    buffer[coloum_index[3] - coloum_index[2] - 1] = 0;
    strcpy(p->filename, buffer);
    
    //get the file data
    memmove(buffer, recevied_buffer + coloum_index[3] + 1, str_length - coloum_index[3] - 1);
    buffer[str_length - coloum_index[3] - 1] = 0;
    strcpy(p->filedata, buffer);
}

#endif
