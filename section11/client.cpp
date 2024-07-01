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
#include "common.h"

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

static int32_t on_response(const uint8_t *data, size_t size) {
    if (size < 1) { // Check if the size is less than 1
        msg("bad response"); // Log a bad response message
        return -1; // Return error code -1
    }
    switch (data[0]) {
    case SER_NIL: // Handle SER_NIL case
        printf("(nil)\n"); // Print nil
        return 1; // Return 1 as the number of bytes processed
    case SER_ERR: // Handle SER_ERR case
        if (size < 1 + 8) { // Check if size is less than 9 bytes
            msg("bad response"); // Log a bad response message
            return -1; // Return error code -1
        }
        {
            int32_t code = 0;
            uint32_t len = 0;
            memcpy(&code, &data[1], 4); // Copy 4 bytes into code
            memcpy(&len, &data[1 + 4], 4); // Copy next 4 bytes into len
            if (size < 1 + 8 + len) { // Check if size is less than required
                msg("bad response"); // Log a bad response message
                return -1; // Return error code -1
            }
            printf("(err) %d %.*s\n", code, len, &data[1 + 8]); // Print error code and message
            return 1 + 8 + len; // Return total bytes processed
        }
    case SER_STR: // Handle SER_STR case
        if (size < 1 + 4) { // Check if size is less than 5 bytes
            msg("bad response"); // Log a bad response message
            return -1; // Return error code -1
        }
        {
            uint32_t len = 0;
            memcpy(&len, &data[1], 4); // Copy 4 bytes into len
            if (size < 1 + 4 + len) { // Check if size is less than required
                msg("bad response"); // Log a bad response message
                return -1; // Return error code -1
            }
            printf("(str) %.*s\n", len, &data[1 + 4]); // Print string
            return 1 + 4 + len; // Return total bytes processed
        }
    case SER_INT: // Handle SER_INT case
        if (size < 1 + 8) { // Check if size is less than 9 bytes
            msg("bad response"); // Log a bad response message
            return -1; // Return error code -1
        }
        {
            int64_t val = 0;
            memcpy(&val, &data[1], 8); // Copy 8 bytes into val
            printf("(int) %ld\n", val); // Print integer value
            return 1 + 8; // Return total bytes processed
        }
    case SER_DBL:
        if (size < 1 + 8) {
            // If the size of the data is less than the expected size for a double,
            // print an error message and return -1 to indicate failure.
            msg("bad response");
            return -1;
        }
        {
            // Extract the double value from the serialized data.
            double val = 0;
            memcpy(&val, &data[1], 8);   // Copy 8 bytes starting from data[1] into val
            printf("(dbl) %g\n", val);   // Print the extracted double value
            return 1 + 8;                // Return the total size consumed (1 byte for SER_DBL + 8 bytes for double)
        }
    case SER_ARR: // Handle SER_ARR case
        if (size < 1 + 4) { // Check if size is less than 5 bytes
            msg("bad response"); // Log a bad response message
            return -1; // Return error code -1
        }
        {
            uint32_t len = 0;
            memcpy(&len, &data[1], 4); // Copy 4 bytes into len
            printf("(arr) len=%u\n", len); // Print array length
            size_t arr_bytes = 1 + 4; // Initialize arr_bytes
            for (uint32_t i = 0; i < len; ++i) { // Loop through array elements
                int32_t rv = on_response(&data[arr_bytes], size - arr_bytes); // Process each element in array which can be string,int or another array.
                if (rv < 0) { // Check if return value is negative
                    return rv; // Return error code
                }
                arr_bytes += (size_t)rv; // Add bytes processed to arr_bytes
            }
            printf("(arr) end\n"); // Print end of array
            return (int32_t)arr_bytes; // Return total bytes processed
        }
    default: // Handle default case
        msg("bad response"); // Log a bad response message
        return -1; // Return error code -1
    }
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
    
    int32_t rv = on_response((uint8_t *)&rbuf[4], len);
    if (rv > 0 && (uint32_t)rv != len) {
        msg("bad response");
        rv = -1;
    }
    return rv;
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