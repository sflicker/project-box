/* server portion for networked tic-tac-toe game */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#define PORT "3490"

#define BACKLOG 10

static int active_games = 0;

void sigchld_handler(int sig) {
    (void)sig;  //quite unused variable warning

    // waitpid() might overwrite errno, so we save and restore it.

    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}

// get sockaddr, IPv4 or IPv6

void * get_in_addr(struct sockaddr * sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void run_game(int first_fd, int second_fd) {


    exit(0);
}

int setup_listener_socket(int port) {
    struct addrinfo hints, *servinfo, *p;
    int sockfd;
    struct sigaction sa;

    int yes = 1;
    char s[INET6_ADDRSTRLEN];
    int rv;

    memset(&hints, 0, sizeof hints);

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }

    // loop through all results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("bind");
            continue;
        }

        break;
    }

    // all done with this structure
    freeaddrinfo(servinfo);

    if (p == NULL) {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    sa.sa_handler = sigchld_handler;   // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags |= SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    return sockfd;

}

int main(int argc, char * argv[]) {
    // listen on sock_fd, new connection on new_fd
    int sockfd, first_fd, second_fd;

    struct sockaddr_storage first_addr;  // connector's address info
    struct sockaddr_storage second_addr;  // connector's address info
    socklen_t firstsin_size;
    socklen_t secondsin_size;

    struct sigaction sa;

    sockfd = setup_listener_socket(PORT);


    printf("server: waiting for incoming connections...\n");

    while(1) {
        sin_size = sizeof their_addr;

        first_fd = accept(sockfd, (struct sockaddr *)&first_addr, &sin_size);
        if (first_fd == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&first_addr),s, sizeof s);
        printf("server: got first player connection from %s\n", s);

        second_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (second_fd == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr),s, sizeof s);
        printf("server: got second player connection from %s\n", s);


        if (!fork()) {  // this is a child process
            close(sockfd);    // child doesn't need listneer
            if (send(new_fd, "Hello, world!", 13, 0) == -1) {
                perror("send");
            }
            close(new_fd);
            exit(0);
        }
        close(new_fd);  // parent doesn't need this
    }

    return 0;

}