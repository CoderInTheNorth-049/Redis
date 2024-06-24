# Section-8 Data Structure: Hashtables
- In last section we used inbuild map data structure for hashing. In this section we implemented our own Data Structure for Hashing.
- A chaining hashtable is used. It’s also used by the real Redis.
- We use powers of 2 for growth, so a bit mask is used instead of the slow modulo operator, since modulo by the power of 2 is the same as getting the lower bits.
- **Note:** Last time in functions `do_get`, `do_set` and `do_del` we took `const vector<string>& cmd` as parameter but now we are taking `vector<string>& cmd` as parameter. The reason is we are swapping values from vector with `key` in `Entry` struct.

## How to run?
Compile both `client.cpp` and `server.cpp` with following commands:
```
g++ client.cpp -o client
g++ server.cpp -o server hashtable.cpp
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

$ ./client set arif mulani
server says: [0] 
$ ./client get arif
server says: [0] mulani
$ ./client del arif
server says: [0] 
$ ./client get arif
server says: [2] 
$ ./client arif mulani
server says: [1] unknown cmd
```
`0` means successful, `1` means invalid command, `2` means key not found.
## References
- [08. Data Structure: Hashtables](https://build-your-own.org/redis/08_hashtables)