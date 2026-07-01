# Simple C Networking Tutorial for the Tic-Tac-Toe Project

## Purpose

This document is a practical reference for implementing the networking parts of
the Networked Tic-Tac-Toe exercise in C. It assumes you already understand the
general ideas of clients, servers, TCP connections, ports, and protocols, but
you may not have used the POSIX socket API before.

The examples target Unix-like systems such as Linux and macOS.

## Mental Model

A TCP server:

1. Creates a socket.
2. Assigns the socket to a local address and port.
3. Starts listening for connections.
4. Accepts clients.
5. Reads and writes bytes on each accepted client socket.
6. Closes sockets when done.

A TCP client:

1. Creates a socket.
2. Resolves a hostname and port to a network address.
3. Connects to the server.
4. Reads and writes bytes on the connected socket.
5. Closes the socket when done.

In C, sockets are represented as integer file descriptors. Once connected, a TCP
socket behaves similarly to a file: use `read`, `write`, and `close`.

## Required Headers

Common headers for this project:

```c
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
```

Use `pthread.h` only if you use threads for simultaneous games.

## TCP Is a Byte Stream

The most important rule: TCP does not preserve message boundaries.

If one process writes three bytes and then four bytes, the other process might
read all seven bytes at once, or one byte at a time, or any grouping in between.
For small local examples, `read(fd, buf, 3)` often returns three bytes, but
robust code must not rely on that.

Use helper functions that keep reading or writing until the expected number of
bytes has been transferred.

```c
int read_exact(int fd, void *buffer, size_t size)
{
    char *p = buffer;
    size_t done = 0;

    while (done < size) {
        ssize_t n = read(fd, p + done, size - done);
        if (n == 0) {
            return 0; /* peer disconnected */
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
    const char *p = buffer;
    size_t done = 0;

    while (done < size) {
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
```

Return value convention in this example:

- `1`: success
- `0`: clean disconnect while reading
- `-1`: error

## Network Byte Order

Machines can store integers in different byte orders. Network protocols normally
send multi-byte integers in network byte order, which is big-endian.

Use:

- `htons`: host-to-network short, usually for 16-bit ports
- `htonl`: host-to-network long, usually for 32-bit integers
- `ntohs`: network-to-host short
- `ntohl`: network-to-host long

For this project, prefer sending 32-bit integers with `int32_t` and
`htonl`/`ntohl`.

```c
#include <stdint.h>

int send_i32(int fd, int32_t value)
{
    int32_t net_value = htonl(value);
    return write_exact(fd, &net_value, sizeof(net_value));
}

int recv_i32(int fd, int32_t *value)
{
    int32_t net_value = 0;
    int result = read_exact(fd, &net_value, sizeof(net_value));
    if (result != 1) {
        return result;
    }

    *value = ntohl(net_value);
    return 1;
}
```

## A Small Message Protocol

The Tic-Tac-Toe exercise can use three-byte command codes from server to client:

```text
SRT
TRN
UPD
WIN
LSE
DRW
```

Because every command is exactly three bytes, the receiver can call
`read_exact(fd, code, 3)`.

```c
int send_code(int fd, const char code[3])
{
    return write_exact(fd, code, 3);
}

int recv_code(int fd, char code[4])
{
    int result = read_exact(fd, code, 3);
    if (result != 1) {
        return result;
    }

    code[3] = '\0';
    return 1;
}
```

Some commands need payloads. For example:

```text
UPD <player_id:int32> <move:int32>
CNT <active_players:int32>
```

The sender and receiver must agree on the exact order.

Server sends:

```c
send_code(client_fd, "UPD");
send_i32(client_fd, player_id);
send_i32(client_fd, move);
```

Client receives:

```c
char code[4];
recv_code(server_fd, code);

if (strcmp(code, "UPD") == 0) {
    int32_t player_id;
    int32_t move;
    recv_i32(server_fd, &player_id);
    recv_i32(server_fd, &move);
}
```

## Creating a Listening Server Socket

For a simple IPv4 server:

```c
int create_listener(int port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }

    int yes = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
        perror("setsockopt");
        close(fd);
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)port);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(fd);
        return -1;
    }

    if (listen(fd, 16) < 0) {
        perror("listen");
        close(fd);
        return -1;
    }

    return fd;
}
```

Notes:

- `AF_INET` means IPv4.
- `SOCK_STREAM` means TCP.
- `INADDR_ANY` means listen on all local interfaces.
- `htons(port)` converts the port to network byte order.
- `SO_REUSEADDR` makes development easier by allowing quick server restarts.
- The `listen` backlog is the number of pending connections the kernel may
  queue before `accept` handles them.

## Accepting Clients

`accept` blocks until a client connects, unless the socket is configured as
non-blocking.

```c
int accept_client(int listener_fd)
{
    struct sockaddr_storage peer_addr;
    socklen_t peer_len = sizeof(peer_addr);

    int client_fd = accept(
        listener_fd,
        (struct sockaddr *)&peer_addr,
        &peer_len
    );

    if (client_fd < 0) {
        perror("accept");
        return -1;
    }

    return client_fd;
}
```

For the Tic-Tac-Toe project, the simplest server loop accepts two clients, puts
their file descriptors into a small game object, then starts a worker thread for
that game.

```c
struct game {
    int players[2];
};

while (1) {
    struct game *game = malloc(sizeof(*game));
    if (game == NULL) {
        perror("malloc");
        break;
    }

    game->players[0] = accept_client(listener_fd);
    game->players[1] = accept_client(listener_fd);

    pthread_t thread;
    pthread_create(&thread, NULL, run_game, game);
    pthread_detach(thread);
}
```

Production-quality code should handle partial failure. For example, if the first
client connects and the second `accept` fails, close the first client and free
the game object.

## Connecting a Client

Prefer `getaddrinfo` over older APIs such as `gethostbyname`.

```c
int connect_to_server(const char *host, const char *port)
{
    struct addrinfo hints;
    struct addrinfo *results = NULL;
    struct addrinfo *rp = NULL;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int rc = getaddrinfo(host, port, &hints, &results);
    if (rc != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rc));
        return -1;
    }

    int fd = -1;

    for (rp = results; rp != NULL; rp = rp->ai_next) {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd < 0) {
            continue;
        }

        if (connect(fd, rp->ai_addr, rp->ai_addrlen) == 0) {
            break;
        }

        close(fd);
        fd = -1;
    }

    freeaddrinfo(results);
    return fd;
}
```

This supports both IPv4 and IPv6 if the server and operating system support
them.

## Threading Model for Multiple Games

The project can use one thread per game:

- Main server thread owns the listening socket.
- Main server thread accepts clients and pairs them.
- Each game thread owns exactly two connected client sockets.
- Each game thread runs the turn loop for one board.
- When a game ends, the game thread closes both sockets and frees its game data.

This model is simple and works well for a practice exercise.

Thread function shape:

```c
void *run_game(void *arg)
{
    struct game *game = arg;

    int p0 = game->players[0];
    int p1 = game->players[1];

    /* Send initial ids, run turn loop, send updates/results. */

    close(p0);
    close(p1);
    free(game);
    return NULL;
}
```

Detach threads if the main thread will not call `pthread_join`:

```c
pthread_detach(thread);
```

## Shared State and Mutexes

If multiple game threads update shared data, protect that data with a mutex.

For example, an active player count:

```c
pthread_mutex_t count_mutex = PTHREAD_MUTEX_INITIALIZER;
int active_players = 0;

void add_player_count(int delta)
{
    pthread_mutex_lock(&count_mutex);
    active_players += delta;
    pthread_mutex_unlock(&count_mutex);
}

int get_player_count(void)
{
    pthread_mutex_lock(&count_mutex);
    int value = active_players;
    pthread_mutex_unlock(&count_mutex);
    return value;
}
```

Do not read shared mutable variables without the same lock used for writes.

## Blocking I/O Is Acceptable Here

Blocking `read`, `write`, and `accept` are fine for this exercise because:

- The server has a main thread for accepting clients.
- Each game has its own thread.
- Each game is turn-based, so waiting for the current player is expected.

You do not need `select`, `poll`, `epoll`, non-blocking sockets, or async I/O for
the basic implementation.

## Handling Disconnects

A disconnect usually appears as:

- `read` returns `0`: the peer closed the connection.
- `read` or `write` returns `-1`: an error occurred.

In a two-player game, if one client disconnects, the simplest behavior is:

1. End that game.
2. Notify the other client if possible.
3. Close both sockets.
4. Decrement active player counts.
5. Free the game object.

You may define an explicit protocol message such as `DSC` for disconnect, but
the programming problem can also accept exiting with a clear local message when a
socket read fails.

## Server-Side Game Flow

A typical game thread can follow this structure:

```text
send player ids
send SRT to both clients
current_player = 0
turn_count = 0

while game is not over:
    send WAT to waiting player
    send TRN to current player
    read move from current player

    if read failed:
        end game as disconnect

    if move == 9:
        send CNT and active player count to current player
        continue

    if move is invalid:
        send INV to current player
        continue

    update board
    send UPD to both clients

    if current player won:
        send WIN to current player
        send LSE to other player
        break

    if board is full:
        send DRW to both clients
        break

    switch current player
```

The server should be authoritative. The client can reject bad keyboard input for
user experience, but only the server decides whether a move is legal.

## Client-Side Flow

A client normally runs a receive loop:

```text
connect
receive assigned player id
wait for SRT

while game is not over:
    receive command code

    if TRN:
        prompt user and send move
    if WAT:
        print waiting message
    if INV:
        print invalid move message
    if CNT:
        receive count and display it
    if UPD:
        receive player id and move
        update local board
        print board
    if WIN/LSE/DRW:
        print result and exit loop
```

The client should not decide whose turn it is. It should only prompt when the
server sends `TRN`.

## Useful Error Handling Pattern

Many socket calls return `-1` on error and set `errno`.

```c
if (some_socket_call(...) < 0) {
    perror("some_socket_call");
    return -1;
}
```

For `getaddrinfo`, use `gai_strerror` instead of `perror`:

```c
int rc = getaddrinfo(host, port, &hints, &results);
if (rc != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rc));
    return -1;
}
```

## Common Mistakes

- Assuming one `read` equals one message.
- Forgetting `htons` for port numbers.
- Sending raw `int` values without considering byte order or size.
- Not closing accepted client sockets after a game ends.
- Not freeing heap-allocated per-game data.
- Reading shared counters without a mutex.
- Prompting from the client based on local assumptions instead of server
  commands.
- Letting a player-count query consume a Tic-Tac-Toe turn.
- Failing to handle the case where one player disconnects.
- Using `strcmp` on a three-byte command buffer that is not null-terminated.

## Build Notes

With CMake, a threaded server usually needs to link pthreads:

```cmake
add_executable(server src/server.c)
target_link_libraries(server pthread)

add_executable(client src/client.c)
```

For stricter builds, enable warnings:

```cmake
target_compile_options(server PRIVATE -Wall -Wextra -pedantic)
target_compile_options(client PRIVATE -Wall -Wextra -pedantic)
```

Warnings are useful for networking code because type mismatches, unchecked return
values, and uninitialized fields can become runtime failures.

## Manual Testing Checklist

Start the server:

```sh
./server 8080
```

Connect two clients in separate terminals:

```sh
./client localhost 8080
./client localhost 8080
```

Test:

- First client waits until the second client connects.
- Both clients receive different player ids.
- Only one client is prompted at a time.
- A valid move updates both boards.
- An occupied square is rejected.
- Entering `9` reports the active player count and preserves the turn.
- A row win, column win, diagonal win, and draw are all detected.
- Closing one client does not crash the server.
- Four clients can play two independent games.

## Further Reading

Useful manual pages:

```sh
man socket
man bind
man listen
man accept
man connect
man read
man write
man close
man getaddrinfo
man pthread_create
man pthread_mutex_lock
```

Beej's Guide to Network Programming is also a widely used introduction to
socket programming in C.
