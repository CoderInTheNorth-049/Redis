#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>

// Custom error handling function
static void die(const char* msg) {
    int err = errno;  // Store the current value of errno
    fprintf(stderr, "[%d] %s\n", err, msg);  // Print the error code and message to stderr
    abort();  // Terminate the program abnormally
}

int main(){
    int fd = socket(AF_INET, SOCK_STREAM, 0); // AF_INET for IPv4 and SOCK_STREAM for TCP
    if (fd < 0){
        die("socket()"); // if socket call fails
    }

    struct sockaddr_in addr = {};                  // Declare and initialize sockaddr_in structure
    addr.sin_family = AF_INET;                     // Set address family to AF_INET (IPv4)
    addr.sin_port = ntohs(1234);                   // Set port number to 1234, converting to network byte order
    addr.sin_addr.s_addr = ntohl(INADDR_LOOPBACK); // Set IP address to 127.0.0.1, converting to network byte order

    // Attempt to connect the socket to the specified address and port
    int rv = connect(fd, (const struct sockaddr *)&addr, sizeof(addr));
    if (rv) {
        die("connect");  // If connection fails, print error and abort
    }
}