# Section-11  Sorted Set From AVL Tree
- New commands has been implemented:
  1. `zadd key score name`: Insert a new element (pair of score and name) into a sorted set identified by `key`.
  2. `zrem key name`: Remove an element identified by `name` from a sorted set identified by `key`.
  3. `zquery key score name offset limit`: Perform a range query on the sorted set identified by `key`, starting from a pair `(score, name)`, and return a subset of elements. `offset` means number of elements to skip from the starting point and `limit` means maximum number of elements to return.
  4. `zscore key name`: Retrieve the `score` of an element identified by `name` from a sorted set identified by `key`.


## How to run?
compile `test_offset.cpp` and run it.
```
$ g++ test_offset.cpp -o test_offset
$ ./test_offset
```
If no error has been thrown by code then we have successfully implemented the sorted set (There will be no output in case of successful execution).

As we have added new commands we have to test it too.

**Note**: I have included slightly different headers in `server.cpp` as the definition of some functions was overlapping in Author's code.

```
g++ client.cpp -o client
g++ server.cpp -o server
./server
```
The output:
```
$ ./client zscore asdf n1
(nil)
$ ./client zquery xxx 1 asdf 1 10
(arr) len=0
(arr) end
$ ./client zadd zset 1 n1
(int) 1
$ ./client zadd zset 2 n2
(int) 1
$ ./client zadd zset 1.1 n1
(int) 0
$ ./client zscore zset n1
(dbl) 1.1
$ ./client zquery zset 1 "" 0 10
(arr) len=4
(str) n1
(dbl) 1.1
(str) n2
(dbl) 2
(arr) end
$ ./client zquery zset 1.1 "" 1 10
(arr) len=2
(str) n2
(dbl) 2
(arr) end
$ ./client zquery zset 1.1 "" 2 10
(arr) len=0
(arr) end
$ ./client zrem zset adsf
(int) 0
$ ./client zrem zset n1
(int) 1
$ ./client zquery zset 1 "" 0 10
(arr) len=2
(str) n2
(dbl) 2
(arr) end
$
```
- Returning `1` on `zadd` command means element successfully added to the set and returning `0` means element was already present and it's `score` has successfully updated.
- `zquery` return array of `_name` and `_score` which having `_score`>=`score` and excluding first `offset` number of elements from them and returning max of `limit` or total elements.


## References
- [11. Sorted Set From AVL Tree](https://build-your-own.org/redis/11_sortedset)