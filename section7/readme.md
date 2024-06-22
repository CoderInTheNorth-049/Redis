# Section-7 Basic Server: get, set, del
- In last section we added the event loop, now we are adding commands to our server.
- The “command” in our design is a list of strings, like `set key val`. We’ll encode the “command” with the following scheme:

    ```
    +------+-----+------+-----+------+-----+-----+------+
    | nstr | len | str1 | len | str2 | ... | len | strn |
    +------+-----+------+-----+------+-----+-----+------+
    ```
    `nstr` is number of strings, `len` is length of following string.

    The response is a 32-bit status code followed by the response string.
    ```
    +-----+--------+
    | res | data...|
    +-----+--------+
    ```
- The `do_request` function handles the request. Only 3 commands (get, set, del) are recognized now.


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
EOF
EOF
EOF
EOF
EOF
EOF
EOF

$ ./client get k
server says: [2]
$ ./client set k v
server says: [0]
$ ./client get k
server says: [0] v
$ ./client del k
server says: [0]
$ ./client get k
server says: [2]
$ ./client arif mulani
server says: [1] Unknown cmd
$ ./client set arif mulani
server says: [0] 
```
`0` means successful, `1` means invalid command, `2` means key not found.
## References
- [07. Basic Server: get, set, del](https://build-your-own.org/redis/07_basic_server)