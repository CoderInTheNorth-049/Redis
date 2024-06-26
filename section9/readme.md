# Section-9 Data Serialization
- In this section we added a new `keys` command which returns all keys.
- Our serialization protocol starts with a byte of data type, followed by various types of payload data. Arrays come first with their size, then their possibly nested elements.
- The serialization scheme can be summarized as `type-length-value` (TLV): `Type` indicates the type of the value; `Length` is for variable length data such as strings or arrays; `Value` is the encoded at last.
- As we used serialization in `server` we are doing deserialization in `client` inshort decoding the return values.
- **Note:** Return types of `do_get`, `do_set` and `do_del` in `server.cpp` is changed to `void`.

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
EOF
EOF
EOF
EOF
EOF

$ ./client asdf
(err) 1 unknown cmd
$ ./client get arif
(nil)
$ ./client set arif mulani
(nil)
$ ./client get arif
(str) mulani
$ ./client set abhijeet fasate
(nil)
$ ./client get abhijeet
(str) fasate
$ ./client keys
(arr) len=2
(str) abhijeet
(str) arif
(arr) end
$ ./client del arif
(int) 1
$ ./client del abhijeet
(int) 1
$ ./client keys
(arr) len=0
(arr) end
```
`err` shows error, `nil` shows nothing is returned, `str` shows string is returned, the new thing in this section is `./client keys` and it returns all keys in an array so returned values are `arr` an array of length `len` and consisting of strings `str` and `end` shows end of the array. `int` returns integer values. int `del` if key is successfully deleted it returns `1` else `0`.
## References
- [09. Data Serialization](https://build-your-own.org/redis/09_serialization)