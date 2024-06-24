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
#include <string>
#include <vector>

using namespace std;

static void msg(const char* msg){
    fprintf(stderr, "%s\n", msg);
}

// Custom error handling function
static void die(const char* msg) {
    int err = errno;  // Store the current value of errno
    fprintf(stderr, "[%d] %s\n", err, msg);  // Print the error code and message to stderr
    abort();  // Terminate the program abnormally
}

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

const size_t k_max_msg = 4096;

static int32_t send_req(int fd, const vector<string>& cmd) {
    uint32_t len = 4; // Start with 4 bytes to store the number of commands
    for (const string &s : cmd) {
        len += 4 + s.size(); // Add 4 bytes for the length of each command string and the size of the command string itself
    }
    if (len > k_max_msg) {
        return -1; // Return an error if the message is too long
    }

    char wbuf[4 + k_max_msg]; // Buffer to hold the message length and text
    memcpy(&wbuf[0], &len, 4); // Copy the total length into the first 4 bytes of wbuf [0,3]

    uint32_t n = cmd.size(); // Number of commands
    memcpy(&wbuf[4], &n, 4); // Copy the number of commands into the next 4 bytes of wbuf [4,7]

    size_t cur = 8; // Start copying commands from the 9th byte [8,...]
    for (const string &s : cmd) {
        uint32_t p = (uint32_t)s.size(); // Size of the command string
        memcpy(&wbuf[cur], &p, 4); // Copy the size of the command string
        memcpy(&wbuf[cur + 4], s.data(), s.size()); // Copy the command string itself
        cur += 4 + s.size(); // Move the cursor forward
    }
    return write_all(fd, wbuf, 4 + len); // Write the complete buffer to the socket
}

static int32_t read_res(int fd) {
    char rbuf[4 + k_max_msg + 1]; // Buffer to hold the response
    errno = 0;
    int32_t err = read_full(fd, rbuf, 4); // Read the first 4 bytes (length) from the file descriptor
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

    err = read_full(fd, &rbuf[4], len); // Read the response text from the file descriptor
    if (err) {
        msg("read() error"); // Handle read error
        return err; // Return the error if read_full fails
    }
    
    uint32_t rescode = 0;
    if(len<4){
        msg("bad response");
        return -1;
    }
    memcpy(&rescode, &rbuf[4], 4);
    printf("server says: [%u] %.*s\n", rescode, len-4, &rbuf[8]);
    return 0;
}

int main(int argc, char** argv){
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

    vector<string> cmd;
    for(int i=1;i<argc;i++){
        cmd.push_back(argv[i]);
    }
    int32_t err = send_req(fd, cmd);
    if(err) goto L_DONE;
    err = read_res(fd);
    if(err) goto L_DONE;

   L_DONE:
    close(fd);
    return 0;
}