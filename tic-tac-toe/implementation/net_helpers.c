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

int read_exact(int fd, void *buffer, size_t size) 
{
    char *p = buffer;
    size_t done = 0;

    while(done < size) {
        ssize_t n = read(fd, p + done, size - done);
        if (n == 0) {
            return 0;  /* peer disconnected */
        }
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        done += (size_t)n;
    }
    return 1;
}

int write_exact(int fd, const void *buffer, size_t size)
{
    const char*p = buffer;
    size_t done = 0;

    while(done < size) {
        ssize_t n = write(fd, p + done, size - done);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (n == 0) {
            return -1;
        }
        done += (size_t)n;
    }

    return 1;
}

int send_i32(int fd, int32_t value) {
    int32_t net_value = htonl(value);   // convert to network byte order
    return write_exact(fd, &net_value, sizeof(net_value));
}

int recv_i32(int fd, int32_t *value) {
    int32_t net_value = 0;
    int result = read_exact(fd, &net_value, sizeof(net_value));
    if (result != 1) {
        return result;
    }

    *value = ntohl(net_value);     // convert from network byte order
    return 1;
}

