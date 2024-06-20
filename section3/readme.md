# Section-3 TCP Server and Client
This contains a simple client-server program with TCP Socket and IPv4
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
client says: hello

$ ./client
server says: world
```
## References
- [03. TCP Server and Client](https://build-your-own.org/redis/03_hello_cs)