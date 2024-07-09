# Section-14  Thread Pool & Asynchronous Tasks
- If the size of sorted set is huge the deletion of keys will take long time and server will get stalled at the time of destruction of keys.
-  Multi-threading is used to move the destructor away from the main thread.

## How to run?
```
$ g++ server.cpp -o server
$ g++ client.cpp -o client
$ ./server
```

The output:
```
$ ./client zadd movies 5 percy_jackson_and_the_lightning_thief
(int) 1
$ ./client keys
(arr) len=1
(str) movies
(arr) end
$ ./client zadd movies 10 lords_of_the_rings_return_of_the_king
(int) 1
$ ./client zadd series 10 game_of_thrones
(int) 1
$ ./client zadd series 8 money_heist
(int) 1
$ ./client keys
(arr) len=2
(str) series
(str) movies
(arr) end
$ ./client zquery movies 6 "" 0 4
(arr) len=2
(str) lords_of_the_rings_return_of_the_king
(dbl) 10
(arr) end
$ ./client zquery movies 5 "" 0 4
(arr) len=4
(str) percy_jackson_and_the_lightning_thief
(dbl) 5
(str) lords_of_the_rings_return_of_the_king
(dbl) 10
(arr) end
$ ./client zrem series game_of_thrones
(int) 1
$ ./client zquery series 0 "" 0 4
(arr) len=2
(str) money_heist
(dbl) 8
(arr) end
$ ./client zrem movies percy_jackson_and_the_lightning_thief
(int) 1
$ ./client zquery movies 5 "" 0 4
(arr) len=2
(str) lords_of_the_rings_return_of_the_king
(dbl) 10
(arr) end
$ 
```


## References
- [14. Thread Pool & Asynchronous Tasks](https://build-your-own.org/redis/14_thread)