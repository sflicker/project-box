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


#ifndef __NET_HELPERS_
#define __NET_HELPERS_

int read_exact(int fd, void *buffer, size_t size);
int write_exact(int fd, const void *buffer, size_t size);
int send_i32(int fd, int32_t value);
int recv_i32(int fd, int32_t *value);

#endif