# Section-13  The Heap Data Structure & TTL
- Heap is used for TTLs (Time to Live), to manage the cache size.
- Two new commands has been added:
 1. `pexpire key time` in which `key` gets deleted after `time` milliseconds.
 2. `pttl key` which returns remained milliseconds for expiration of the `key`.

## How to run?
```
$ g++ server.cpp -o server
$ g++ client.cpp -o client
$ ./server
```

The output:
```
$ ./client keys
(arr) len=0
(arr) end
$ ./client set arif mulani
(nil)
$ ./client keys
(arr) len=1
(str) arif
(arr) end
$ ./client pexpire arif 5000
(int) 1
$ ./client keys
(arr) len=1
(str) arif
(arr) end
$ ./client keys
(arr) len=0
(arr) end
$ ./client pttl arif
(int) -2
$ ./client set arif mulani
(nil)
$ ./client pttl arif
(int) -1
$ 
```
First of all there were no keys in cache. Then I added key `arif` with value `mulani` and now set expiration time of key `arif` to 5000 mSec. Before 5 sec I again ran keys command so I still had `arif` in my cache but after 5 sec when I ran keys command agai it was gone. `pttl` return `-2` indicates that there is no such key exists in cache while `-1` indicates that the key is present but expiration is not set yet.

## References
- [13. The Heap Data Structure & TTL](https://build-your-own.org/redis/13_heap)