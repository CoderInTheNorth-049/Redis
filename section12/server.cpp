#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <string>
#include <vector>
#include "hashtable.h"

using namespace std;

#define container_of(ptr, type, member) ({                  \
    const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
    (type *)( (char *)__mptr - offsetof(type, member) );})


static void msg(const char* msg){
    fprintf(stderr, "%s\n", msg);
}

static void die(const char* msg) {
    int err = errno;  // Store the current value of errno
    fprintf(stderr, "[%d] %s\n", err, msg);  // Print the error code and message to stderr
    abort();  // Terminate the program abnormally
}

static void fd_set_nb(int fd) {
    errno = 0; // Clear errno before the operation
    int flags = fcntl(fd, F_GETFL, 0); // Get the current file status flags
    if (errno) {
        die("fcntl error"); // Handle error if fcntl fails
        return;
    }
    flags |= O_NONBLOCK; // Add the O_NONBLOCK flag to enable non-blocking mode

    errno = 0; // Clear errno before the next operation
    (void)fcntl(fd, F_SETFL, flags); // Set the new flags for the file descriptor
    if (errno) {
        die("fcntl error"); // Handle error if fcntl fails
    }
}

const size_t k_max_msg = 4096;

enum {
    STATE_REQ = 0, // Waiting for or processing a request from the client
    STATE_RES = 1, // Preparing or sending a response to the client
    STATE_END = 2, // Connection is ending or has ended
};

struct Conn {
    int fd = -1; // File descriptor for the connection, initialized to -1
    uint32_t state = 0; // State of the connection, either STATE_REQ or STATE_RES
    // buffer for reading
    size_t rbuf_size = 0; // Size of data currently in the read buffer
    uint8_t rbuf[4 + k_max_msg]; // Read buffer to hold incoming messages
    // buffer for writing
    size_t wbuf_size = 0; // Size of data currently in the write buffer
    size_t wbuf_sent = 0; // Amount of data sent from the write buffer
    uint8_t wbuf[4 + k_max_msg]; // Write buffer to hold outgoing messages
};

static void conn_put(vector<Conn*>& fd2conn, struct Conn* conn) {
    if (fd2conn.size() <= (size_t)conn->fd) { // If the vector is smaller than or equal to the file descriptor index
        fd2conn.resize(conn->fd + 1); // Resize the vector to be large enough to include the file descriptor index
    }
    fd2conn[conn->fd] = conn; // Assign the connection pointer to the vector at the index of the file descriptor
}

static int32_t accept_new_conn(vector<Conn*>& fd2conn, int fd) {
    struct sockaddr_in client_addr = {}; // Structure to hold client address
    socklen_t socklen = sizeof(client_addr); // Size of the client address structure
    int connfd = accept(fd, (struct sockaddr*)&client_addr, &socklen); // Accept a new connection
    if (connfd < 0) {
        msg("accept() error"); // Log an error message if accept fails
        return -1;
    }
    // Set the new connection fd to non-blocking mode
    fd_set_nb(connfd);

    struct Conn* conn = (struct Conn*)malloc(sizeof(struct Conn)); // Allocate memory for a new connection
    if (!conn) {
        close(connfd); // Close the connection if memory allocation fails
        return -1;
    }
    conn->fd = connfd; // Set the file descriptor for the connection
    conn->state = STATE_REQ; // Initialize the state to STATE_REQ
    conn->rbuf_size = 0; // Initialize the read buffer size
    conn->wbuf_size = 0; // Initialize the write buffer size
    conn->wbuf_sent = 0; // Initialize the amount of data sent
    conn_put(fd2conn, conn); // Add the connection to the vector
    return 0;
}

static void state_req(Conn *conn);
static void state_res(Conn *conn);

const size_t k_max_args = 1024;

static int32_t parse_req(const uint8_t* data, size_t len, vector<string>& out) {
    if (len < 4) return -1; // Ensure there's at least 4 bytes to read the number of arguments

    uint32_t n = 0;
    memcpy(&n, &data[0], 4); // Read the number of arguments
    if (n > k_max_args) return -1; // Return an error if the number of arguments exceeds the maximum allowed

    size_t pos = 4; // Position to start reading arguments from
    while (n--) {
        if (pos + 4 > len) { // Check if there's enough data left to read the size of the next argument
            return -1;
        }
        uint32_t sz = 0;
        memcpy(&sz, &data[pos], 4); // Read the size of the next argument
        if (pos + 4 + sz > len) { // Check if there's enough data left to read the argument itself
            return -1;
        }
        out.push_back(string((char*)&data[pos + 4], sz)); // Extract the argument and add it to the output vector
        pos += 4 + sz; // Move the position forward
    }

    if (pos != len) return -1; // Ensure there's no trailing garbage data
    return 0;
}

// DS for the key space
static struct {
    HMap db;
} g_data;

// the structure for the key
struct Entry {
    struct HNode node;
    string key;
    string val;
};

// Define a function to compare two HNode entries based on their 'key' field
static bool entry_eq(HNode *lhs, HNode *rhs) {
    // Cast the 'lhs' and 'rhs' nodes back to their containing struct Entry using container_of macro
    struct Entry *le = container_of(lhs, struct Entry, node);
    struct Entry *re = container_of(rhs, struct Entry, node);

    // Compare the 'key' field of both entries
    return le->key == re->key;
}

// Define a function to compute the hash value of a given data buffer with a specified length
static uint64_t str_hash(const uint8_t *data, size_t len) {
    uint32_t h = 0x811C9DC5;  // Initial hash value
    for (size_t i = 0; i < len; i++) {
        h = (h + data[i]) * 0x01000193;  // Update the hash value using the FNV-1a algorithm
    }
    return h;  // Return the computed hash value
}

enum {
    ERR_UNKNOWN = 1,  // Represents an unknown error
    ERR_2BIG = 2,     // Represents an error indicating a size that's too big
};

enum {
    SER_NIL = 0,  // Represents a nil or null value
    SER_ERR = 1,  // Represents an error
    SER_STR = 2,  // Represents a string
    SER_INT = 3,  // Represents an integer
    SER_ARR = 4,  // Represents an array
};

static void out_nil(string& out){
    out.push_back(SER_NIL); // Append the SER_NIL byte to indicate a nil value
}

static void out_str(string& out,const string& val){
    out.push_back(SER_STR); // Append the SER_STR byte to indicate a string
    uint32_t len = (uint32_t)val.size(); // Get the length of the string
    out.append((char*)&len, 4); // Append the length of the string (4 bytes)
    out.append(val); // Append the actual string data
}

static void out_int(std::string &out, int64_t val) {
    out.push_back(SER_INT); // Append the SER_INT byte to indicate an integer
    out.append((char *)&val, 8); // Append the integer value (8 bytes)
}

static void out_err(std::string &out, int32_t code, const std::string &msg) {
    out.push_back(SER_ERR); // Append the SER_ERR byte to indicate an error
    out.append((char *)&code, 4); // Append the error code (4 bytes)
    uint32_t len = (uint32_t)msg.size(); // Get the length of the error message
    out.append((char *)&len, 4); // Append the length of the error message (4 bytes)
    out.append(msg); // Append the actual error message
}

static void out_arr(std::string &out, uint32_t n) {
    out.push_back(SER_ARR); // Append the SER_ARR byte to indicate an array
    out.append((char *)&n, 4); // Append the number of elements in the array (4 bytes)
}


static void do_get(vector<string>& cmd, string& out) {
    Entry key;
    key.key.swap(cmd[1]);

    // Compute the hash code for key.key and assign it to key.node.hcode
    key.node.hcode = str_hash((uint8_t *)key.key.data(), key.key.size());

    // Lookup key.node in the hash map g_data.db using the entry_eq function for equality comparison
    HNode *node = hm_lookup(&g_data.db, &key.node, &entry_eq);

    if (!node) { // If node is NULL, the key was not found in the hash map
        return out_nil(out);
    }

    const string& val = container_of(node, Entry, node)->val; // Get the value associated with the key
    out_str(out, val);
}

static void do_set(vector<string>& cmd, string& out) {
    Entry key;
    key.key.swap(cmd[1]);
    key.node.hcode = str_hash((uint8_t *)key.key.data(), key.key.size());

    HNode *node = hm_lookup(&g_data.db, &key.node, &entry_eq);
    if (node) { // If node is not NULL, update its value
        container_of(node, Entry, node)->val.swap(cmd[2]); // Swap cmd[2] with the value in the corresponding Entry structure
    } else { // If node is NULL, insert a new Entry structure into the hash map
        Entry *ent = new Entry(); // Allocate memory for a new Entry structure
        ent->key.swap(key.key); // Swap key.key with ent->key
        ent->node.hcode = key.node.hcode; // Assign key.node.hcode to ent->node.hcode
        ent->val.swap(cmd[2]); // Swap cmd[2] with ent->val
        hm_insert(&g_data.db, &ent->node); // Insert ent->node into the hash map
    }
    return out_nil(out);
}

static void do_del(vector<string>& cmd, string& out) {
    Entry key;
    key.key.swap(cmd[1]);
    key.node.hcode = str_hash((uint8_t *)key.key.data(), key.key.size());

    HNode *node = hm_pop(&g_data.db, &key.node, &entry_eq);
    if (node) { // If node is not NULL, delete the corresponding Entry structure
        delete container_of(node, Entry, node);
    }
    return out_int(out, node ? 1:0);
}

static void h_scan(HTab *tab, void (*f)(HNode *, void *), void *arg) {
    if (tab->size == 0) {
        return; // If the table is empty, return immediately
    }
    for (size_t i = 0; i < tab->mask + 1; ++i) { // Iterate over each bucket in the hash table
        HNode *node = tab->tab[i]; // Get the head node of the current bucket
        while (node) { // Iterate through the linked list of nodes in the current bucket
            f(node, arg); // Apply the function f to the current node, passing arg as an additional argument
            node = node->next; // Move to the next node in the linked list
        }
    }
}

static void cb_scan(HNode *node, void *arg) {
    string& out = *(string *)arg;
    out_str(out, container_of(node, Entry, node)->key);
}

static void do_keys(vector<string> &cmd, string &out) {
    (void)cmd;
    out_arr(out, (uint32_t)hm_size(&g_data.db));
    h_scan(&g_data.db.ht1, &cb_scan, &out);
    h_scan(&g_data.db.ht2, &cb_scan, &out);
}

static bool cmd_is(const string& word, const char* cmd) {
    return strcasecmp(word.c_str(), cmd) == 0; // Compare word with cmd ignoring case differences
}

static int32_t do_request(vector<string>& cmd, string& out) {
    if(cmd.size() == 1 && cmd_is(cmd[0], "keys")){
        do_keys(cmd, out);
    } else if (cmd.size() == 2 && cmd_is(cmd[0], "get")) { // Check if the command is a 'get' with the correct number of arguments
        do_get(cmd, out);
    } else if (cmd.size() == 3 && cmd_is(cmd[0], "set")) { // Check if the command is a 'set' with the correct number of arguments
        do_set(cmd, out);
    } else if (cmd.size() == 2 && cmd_is(cmd[0], "del")) { // Check if the command is a 'del' with the correct number of arguments
        do_del(cmd, out);
    } else {
        out_err(out, ERR_UNKNOWN, "unknown cmd");
    }
    return 0;
}

static bool try_one_request(Conn* conn) {
    if (conn->rbuf_size < 4) {
        return false; // Not enough data in the buffer. Will retry in the next iteration
    }
    uint32_t len = 0;
    memcpy(&len, &conn->rbuf[0], 4); // Copy the length of the message from the buffer
    if (len > k_max_msg) {
        msg("too long"); // Log a message if the length is too long
        conn->state = STATE_END; // Set the state to end
        return false;
    }
    if (4 + len > conn->rbuf_size) {
        return false; // Not enough data in the buffer. Will retry in the next iteration
    }

    // parse the request
    vector<string> cmd;
    if(parse_req(&conn->rbuf[4], len, cmd) != 0){
        msg("bad req");
        conn->state = STATE_END;
        return false;
    }
    
    string out;
    do_request(cmd, out);

    if(4+out.size() > k_max_msg){
        out.clear();
        out_err(out, ERR_2BIG, "response is too big");
    }

    uint32_t wlen = (uint32_t)out.size();
    memcpy(&conn->wbuf[0], &wlen, 4); // Copy the length of the response to the beginning of the write buffer
    memcpy(&conn->wbuf[4], out.data(), out.size()); // Copy the result code to the write buffer
    conn->wbuf_size = 4 + wlen; // Update the write buffer size to include the length and the response

    size_t remain = conn->rbuf_size - 4 - len; // Calculate the remaining data in the read buffer
    if (remain) {
        memmove(conn->rbuf, &conn->rbuf[4 + len], remain); // Move the remaining data to the start of the buffer
    }
    conn->rbuf_size = remain; // Update the size of the read buffer

    conn->state = STATE_RES; // Set the state to response
    state_res(conn); // Call the state_res function to handle the response state
    return (conn->state == STATE_REQ); // Return true if the state is still request, otherwise false
}

static bool try_fill_buffer(Conn* conn) {
    assert(conn->rbuf_size < sizeof(conn->rbuf)); // Ensure that rbuf_size is within bounds
    ssize_t rv = 0; // Variable to store return value of read()

    do {
        size_t cap = sizeof(conn->rbuf) - conn->rbuf_size; // Calculate remaining capacity in rbuf
        rv = read(conn->fd, &conn->rbuf[conn->rbuf_size], cap); // Read data into rbuf
    } while (rv < 0 && errno == EINTR); // Retry if interrupted by signal

    if (rv < 0 && errno == EAGAIN) { // If rv < 0 and errno is EAGAIN (non-blocking read)
        return false; // Return false to try again later
    }
    if (rv < 0) { // If rv < 0 and errno is not EAGAIN
        msg("read() error"); // Log read error
        conn->state = STATE_END; // Set state to end
        return false; // Return false to indicate failure
    }
    if (rv == 0) { // If rv is 0 (EOF reached)
        if (conn->rbuf_size > 0) {
            msg("unexpected EOF"); // Log unexpected EOF
        } else {
            msg("EOF"); // Log normal EOF
        }
        conn->state = STATE_END; // Set state to end
        return false; // Return false to indicate failure
    }

    conn->rbuf_size += (size_t)rv; // Update rbuf_size with the number of bytes read
    assert(conn->rbuf_size <= sizeof(conn->rbuf)); // Ensure rbuf_size is within bounds

    while (try_one_request(conn)) {} // Process as many complete requests as possible

    return (conn->state == STATE_REQ); // Return true if still in request state
}

static void state_req(Conn* conn){
    while(try_fill_buffer(conn)){}
}

static bool try_flush_buffer(Conn* conn) {
    ssize_t rv = 0; // Variable to store return value of write()
    do {
        size_t remain = conn->wbuf_size - conn->wbuf_sent; // Calculate remaining data to be sent
        rv = write(conn->fd, &conn->wbuf[conn->wbuf_sent], remain); // Write remaining data to socket
    } while (rv < 0 && errno == EINTR); // Retry if interrupted by signal

    if (rv < 0) { // If write() returns an error
        msg("write() error"); // Log write error
        conn->state = STATE_END; // Set state to end
        return false; // Return false to indicate failure
    }

    conn->wbuf_sent += (size_t)rv; // Update wbuf_sent with the number of bytes written
    assert(conn->wbuf_sent <= conn->wbuf_size); // Ensure wbuf_sent is within bounds

    if (conn->wbuf_sent == conn->wbuf_size) { // If all data has been sent
        conn->state = STATE_REQ; // Set state to request
        conn->wbuf_sent = 0; // Reset wbuf_sent
        conn->wbuf_size = 0; // Reset wbuf_size
        return false; // Return false to indicate completion
    }
    return true; // Return true to indicate more data to be sent
}

static void state_res(Conn* conn){
    while(try_flush_buffer(conn)){}
}

static void connection_io(Conn* conn) { 
    if (conn->state == STATE_REQ) { // If the connection is in the request state
        state_req(conn); // Process the request state
    } else if (conn->state == STATE_RES) { // If the connection is in the response state
        state_res(conn); // Process the response state
    } else {
        assert(0); // This should never happen, assert failure if an unknown state is encountered
    }
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

    vector<Conn*> fd2conn; // Vector to store connection objects indexed by file descriptor
    fd_set_nb(fd); // Set the server socket to non-blocking mode

    vector<struct pollfd> poll_args; // Vector to store pollfd structures for polling
    while (true) { // Infinite loop to continuously handle connections
        poll_args.clear(); // Clear the poll_args vector for each iteration
        struct pollfd pfd = {fd, POLLIN, 0}; // Initialize pollfd structure for the server socket
        poll_args.push_back(pfd); // Add the server socket pollfd to the vector

        for (Conn* conn : fd2conn) { // Iterate over all connections
            if (!conn) continue; // Skip if the connection is NULL
            struct pollfd pfd = {}; // Initialize pollfd structure for the connection
            pfd.fd = conn->fd; // Set the file descriptor
            pfd.events = (conn->state == STATE_REQ) ? POLLIN : POLLOUT; // Set events based on connection state
            pfd.events = pfd.events | POLLERR; // Always monitor for errors
            poll_args.push_back(pfd); // Add the connection pollfd to the vector
        }

        int rv = poll(poll_args.data(), (nfds_t)poll_args.size(), 1000); // Call poll() with the pollfd array
        if (rv < 0) { // If poll() returns an error
            die("poll"); // Log and exit on poll error
        }

        for (size_t i = 1; i < poll_args.size(); i++) { // Iterate over all connections in poll_args
            if (poll_args[i].revents) { // If there are events on the connection
                Conn* conn = fd2conn[poll_args[i].fd]; // Get the connection object
                connection_io(conn); // Handle the connection IO
                if (conn->state == STATE_END) { // If the connection is in the end state
                    fd2conn[conn->fd] = NULL; // Remove the connection from fd2conn
                    (void)close(conn->fd); // Close the connection file descriptor
                    free(conn); // Free the connection object
                }
            }
        }

        if (poll_args[0].revents) { // If there are events on the server socket
            (void)accept_new_conn(fd2conn, fd); // Accept new connection
        }
    }

    return 0;
}