#ifndef _COMMON_H_
#define _COMMON_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <math.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <time.h>

#define MAX_PACKET_SIZE 1000
#define BUFFER_SIZE 1024

// Packet Structure
typedef struct packet {
    unsigned int total_frag;
    unsigned int frag_no;
    unsigned int size;
    char * filename;
    char filedata[1000];
} packet;

#endif