# Programming Problem: Networked Tic-Tac-Toe

## Overview

Build a command-line, networked Tic-Tac-Toe game for two human players. The
solution should include a server process that coordinates games and a client
process that players use to connect, view the board, and submit moves.

This exercise is intended for an experienced developer. It focuses on socket
programming, simple protocol design, turn-based game state management,
concurrency, input validation, and clean project organization in C.

## Goals

Implement:

- A TCP server that listens on a configurable port.
- A TCP client that connects to the server by hostname and port.
- Two-player Tic-Tac-Toe games on a 3 by 3 board.
- Server-side move validation and win/draw detection.
- A simple client/server protocol for game messages and integer payloads.
- Support for multiple simultaneous games by pairing clients into groups of two.
- A CMake build that produces separate `server` and `client` executables.

## User Experience

The server is started first:

```sh
./server 8080
```

Each player then starts a client:

```sh
./client localhost 8080
```

The first connected player should be told that they are waiting for another
player. Once a second player connects, both clients should be told the game has
started, shown an empty board, and assigned a mark.

Players take turns entering positions `0` through `8`, mapped left-to-right and
top-to-bottom:

```text
 0 | 1 | 2
-----------
 3 | 4 | 5
-----------
 6 | 7 | 8
```

The client should also allow a player to enter `9` to request the number of
currently connected active players from the server. This query must not consume
the player's turn.

## Game Rules

- The game is played on a 3 by 3 board.
- Player 1 uses one mark and player 2 uses the other mark, for example `O` and
  `X`.
- Players alternate turns.
- A move is valid only if it is in the range `0` through `8` and the target
  square is empty.
- If a player chooses an occupied square, the server should reject the move and
  ask that same player to try again.
- The first player to place three marks in a row, column, or diagonal wins.
- If all nine squares are filled and neither player has won, the game is a draw.
- When the game ends, both clients should receive the result and exit cleanly.

## Required Protocol

Use a compact protocol over TCP. Messages from the server to the client should be
three-byte ASCII command codes. Integer payloads may be sent as fixed-size
binary integers, as long as both client and server use the same representation.

At minimum, support these server-to-client messages:

| Code | Meaning |
| ---- | ------- |
| `HLD` | The client is connected and waiting for a second player. |
| `SRT` | The game has started. |
| `TRN` | It is this client's turn; the client should prompt for input. |
| `WAT` | The client should wait for the other player. |
| `INV` | The previous move was invalid; prompt the same player again. |
| `UPD` | A board update follows. |
| `CNT` | The active player count follows. |
| `WIN` | This client won the game. |
| `LSE` | This client lost the game. |
| `DRW` | The game ended in a draw. |

Recommended payloads:

- On connection, send each client its player id as an integer, either `0` or `1`.
- After `UPD`, send the player id and move index as integers.
- After `CNT`, send the current active player count as an integer.
- When the client sends a move, send the selected index as an integer.

You may extend the protocol if needed, but document any additions.

## Server Requirements

The server must:

- Accept a port number as its only required command-line argument.
- Bind a TCP listening socket to that port.
- Accept incoming client connections.
- Pair clients into games of exactly two players.
- Start each game independently so that a new pair of clients can play while
  other games are already running.
- Maintain a shared count of active connected players.
- Protect shared state with appropriate synchronization.
- Keep authoritative board state on the server.
- Validate every move on the server, even if the client performs local checks.
- Broadcast valid board updates to both clients.
- Notify clients of wins, losses, draws, invalid moves, and wait states.
- Handle client disconnects without crashing the entire server.
- Close client sockets and release per-game resources when a game ends.

## Client Requirements

The client must:

- Accept a hostname and port as command-line arguments.
- Connect to the server over TCP.
- Receive its assigned player id.
- Maintain a local board display based on server updates.
- Print a readable command-line board after each update.
- Prompt for input only when the server indicates it is the client's turn.
- Accept moves `0` through `8`.
- Accept `9` as a request for the active player count.
- Reject obviously invalid local input before sending it to the server.
- Display clear messages for waiting, invalid moves, player count, win, loss,
  draw, and disconnection.
- Exit cleanly when the game ends or the server disconnects.

## Build Requirements

Use CMake to build the project. The repository should have a structure similar
to:

```text
networked-tic-tac-toe/
├── CMakeLists.txt
├── include/
│   ├── client.h
│   └── server.h
└── src/
    ├── client.c
    └── server.c
```

The build should produce:

- `server`
- `client`

Example build commands:

```sh
mkdir -p build
cd build
cmake ..
make
```

## Edge Cases

Your implementation should account for:

- Missing or invalid command-line arguments.
- Failure to create, bind, listen on, accept from, connect to, read from, or
  write to a socket.
- A player disconnecting before a game starts.
- A player disconnecting during a game.
- A client sending an invalid move.
- A client requesting the active player count during their turn.
- Multiple games running at the same time.
- Cleanup of sockets, dynamically allocated memory, and synchronization
  primitives.

## Suggested Implementation Plan

1. Implement board state, move validation, win checking, and draw checking as
   isolated functions.
2. Implement a single-game local loop to confirm the game logic.
3. Add TCP server setup and client connection code.
4. Define and implement the message protocol.
5. Implement one networked game between exactly two clients.
6. Add threading or another concurrency model to support multiple simultaneous
   games.
7. Add robust disconnect handling and resource cleanup.
8. Polish user-facing prompts and board display.

## Acceptance Criteria

A submission is complete when:

- The project builds with CMake without manual compiler commands.
- `./server <port>` starts a server that waits for players.
- Two clients can connect and complete a full game.
- The server enforces turn order and rejects occupied squares.
- Both clients see board updates after every valid move.
- Win, loss, and draw outcomes are correctly detected and reported.
- Entering `9` during a turn prints the active player count and does not advance
  the turn.
- At least two independent games can run concurrently with four connected
  clients.
- Disconnects do not crash the server process.

## Optional Extensions

- Add names for players.
- Add text chat between players.
- Support replaying without reconnecting.
- Add a spectator mode.
- Replace binary integer payloads with a portable text-based protocol.
- Add automated tests for board logic and protocol parsing.
- Add IPv6 support.
- Add a graceful server shutdown command.
