# Section-4 Protocol Parsing
Updated server to process multiple requests.

Implemented a sort of "protocol" to split requests apart from the TCP byte stream.

Scheme:
```
+-----+------+-----+------+------
| len | msg1 | len | msg2 | more.....
+-----+------+-----+------+------
```

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
- [04. Protocol Parsing](https://build-your-own.org/redis/04_proto)