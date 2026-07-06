#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "net_helpers.h"

/* usage: server <port> */
int main(int argc, char * argv[]) {
    if (argc < 1) {
        printf("Usage: %s port", argv[0]);
    }
    int port = atoi(argv[1]);
}