# Section-6 The Event Loop Implementation
- `query` function from `client.cpp` splited into `send_req` and `read_res`.
- `server.cpp` now use non-blocking I/O & the `poll` system call for efficient event-driven communication. Recent updates:
    1. Non-blocking socket operations
    2. Efficient polling mechanism
    3. Buffer management for reading and writing data
    4. Improved state handling for client connections

## Overview of some Functions

1. ### `fd_set_nb(fd)` 
    - Sets the server socket to non-blocking mode to ensure the server does not block while waiting for I/O operations to complete.

2. ### `try_fill_buffer` (Reading Data)
    - Fills the read buffer by reading data from the client socket.
    - Handles errors and EOF appropriately, updating the connection state as needed.

3. ### `try_flush_buffer` (Writing Data)
    - Flushes the write buffer by writing data to the client socket.
    - Handles errors appropriately, updating the connection state as needed.

## How to run?
Compile both `client.cpp` and `server.cpp` with following commands:
```
g++ client.cpp -o client
g++ server.cpp -o server
```
Run `./server` in one window and `./client` in another window:

The Output:
```
$ ./server
client says: hello1
client says: hello2
client says: hello3
EOF

$ ./client
server says: world
server says: world
server says: world
```
## References
- [06. The Event Loop Implementation](https://build-your-own.org/redis/06_event_loop_impl)