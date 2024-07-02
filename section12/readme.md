# Section-12  The Event Loop and Timers
- A couple of things were modified:
 1. The timeout argument of `poll` is calculated by the `next_timer_ms` function.
 2. The code for destroying a connection was moved to the `conn_done` function.
 3. Added the `process_timers` function for firing timers.
 4. Timers are updated in `connection_io` and initialized in `accept_new_conn`.


## How to run?
```
$ g++ server.cpp -o server
$ ./server
```
Open another terminal.
``` 
$ sudo apt get socat
$ socat tcp:127.0.0.1:1234 -
```
After 5s of running above socat command on server side the following output will occur.

The output:
```
$ ./server
removing idle connection: 4
```


## References
- [12. The Event Loop and Timers](https://build-your-own.org/redis/12_timer)