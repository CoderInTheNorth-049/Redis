#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>

static void msg(const char* msg){
    fprintf(stderr, "%s\n", msg);
}

static void die(const char* msg) {
    int err = errno;  // Store the current value of errno
    fprintf(stderr, "[%d] %s\n", err, msg);  // Print the error code and message to stderr
    abort();  // Terminate the program abnormally
}

const size_t k_max_msg = 4096;

// Function to read exactly 'n' bytes from a file descriptor 'fd' into the buffer 'buf'
static int32_t read_full(int fd, char* buf, size_t n) {
    // Loop until all 'n' bytes have been read
    while (n > 0) { 
        ssize_t rv = read(fd, buf, n); // Attempt to read up to 'n' bytes from 'fd' into 'buf'
        // size_t -> unsigned int for size and count of elements, ssize_t -> signed int for sized or error codes
        if (rv <= 0) { // Check for read errors or end of file
            return -1;  // Return -1 if read() fails or end of file is reached
        } 
        assert((size_t)rv <= n); //rv -> no. of bytes read
        n -= (size_t)rv;
        buf += rv;
    }
    return 0;
}

// Function to write exactly 'n' bytes from the buffer 'buf' to the file descriptor 'fd'
static int32_t write_all(int fd, const char* buf, size_t n){
    while(n > 0){
        ssize_t rv = write(fd, buf, n);
        if(rv <= 0){
            return -1;
        }
        assert((size_t)rv <= n); //rv -> no. of bytes written
        n -= (size_t)rv; //reduce no. of bytes remained
        buf += rv; //move buffer pointer forward
    }
    return 0;
}

static int32_t one_request(int connfd){
    char rbuf[4 + k_max_msg + 1]; // Buffer to hold the response
    errno = 0;
    int32_t err = read_full(connfd, rbuf, 4); // Read the first 4 bytes (length) from the file descriptor
    if (err) {
        if (errno == 0) msg("EOF"); // Check for end-of-file condition
        else msg("read() error"); // Handle read error
        return err; // Return the error if read_full fails
    }

    uint32_t len = 0;
    memcpy(&len, rbuf, 4); // Copy the first 4 bytes from rbuf to len
    if (len > k_max_msg) { // Check if the response length exceeds the maximum allowed message size
        msg("too long");
        return -1;
    }

    err = read_full(connfd, &rbuf[4], len); // Read the response text from the file descriptor
    if (err) {
        msg("read() error"); // Handle read error
        return err; // Return the error if read_full fails
    }
    
    rbuf[4 + len] = '\0'; // Null-terminate the response text
    printf("client says: %s\n", &rbuf[4]);

    const char reply[] = "world";
    char wbuf[4 + sizeof(reply)];
    len = (uint32_t)strlen(reply);
    memcpy(wbuf, &len, 4);
    memcpy(&wbuf[4], reply, len);
    return write_all(connfd, wbuf, 4 + len);
}

int main(){
    int fd = socket(AF_INET, SOCK_STREAM, 0); // AF_INET for IPv4 and SOCK_STREAM for TCP
    if (fd < 0){
        die("socket()"); // if socket call fails
    }

    int val = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    struct sockaddr_in addr = {}; // Declare and initialize sockaddr_in structure
    addr.sin_family = AF_INET; // Set address family to AF_INET (IPv4)
    addr.sin_port = ntohs(1234); // Set port number to 1234, converting to network byte order
    addr.sin_addr.s_addr = ntohl(0); // Set IP address to 0.0.0.0, converting to network byte order

    int rv = bind(fd, (const sockaddr *)&addr, sizeof(addr));
    if(rv){
        die("bind()");
    }

    rv = listen(fd, SOMAXCONN); // Mark the socket as a passive socket to accept incoming connections
    if(rv){
        die("listen"); // If listen fails, print error and abort
    }

    while (true) {
        struct sockaddr_in client_addr = {};  // Structure to hold client address information
        socklen_t socklen = sizeof(client_addr);  // Size of the client address structure
        int connfd = accept(fd, (struct sockaddr *)&client_addr, &socklen);  // Accept an incoming connection

        if (connfd < 0) {
            continue;  // If accept fails, continue to the next iteration (handle error silently)
        }

        while(true){
            int32_t err = one_request(connfd);
            if(err){
                break;
            }
        }
        close(connfd);  // Close the client connection
    }

    return 0;
}