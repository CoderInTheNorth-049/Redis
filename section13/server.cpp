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
#include <cmath>
#include "hashtable.cpp"
#include "zset.cpp"
#include "avl.cpp"
#include "common.h"
#include "list.h"
#include "heap.cpp"

using namespace std;

static void msg(const char* msg){
    fprintf(stderr, "%s\n", msg);
}

static void die(const char* msg) {
    int err = errno;  // Store the current value of errno
    fprintf(stderr, "[%d] %s\n", err, msg);  // Print the error code and message to stderr
    abort();  // Terminate the program abnormally
}

static uint64_t get_monotonic_usec() {
    timespec tv = {0, 0};                          // Declare a timespec structure and initialize it to {0, 0}
    clock_gettime(CLOCK_MONOTONIC, &tv);           // Get the current monotonic time and store it in tv
    return uint64_t(tv.tv_sec) * 1000000 + tv.tv_nsec / 1000; // Convert seconds to microseconds and add nanoseconds converted to microseconds
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

struct Conn;

static struct{
    HMap db;
    vector<Conn*> fd2conn;
    DList idle_list;
    vector<HeapItem> heap;
} g_data;

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
    uint64_t idle_start = 0; // Timestamp of when the connection became idle
    DList idle_list; // Doubly linked list node for managing idle connections
};

static void conn_put(vector<Conn*>& fd2conn, struct Conn* conn) {
    if (fd2conn.size() <= (size_t)conn->fd) { // If the vector is smaller than or equal to the file descriptor index
        fd2conn.resize(conn->fd + 1); // Resize the vector to be large enough to include the file descriptor index
    }
    fd2conn[conn->fd] = conn; // Assign the connection pointer to the vector at the index of the file descriptor
}

static int32_t accept_new_conn(int fd) {
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
    conn->idle_start = get_monotonic_usec(); // Set the idle_start timestamp to the current monotonic time in microseconds
    dlist_insert_before(&g_data.idle_list, &conn->idle_list);
    conn_put(g_data.fd2conn, conn); // Add the connection to the vector
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

enum{
    T_STR = 0,
    T_ZSET = 1,
};

// the structure for the key
struct Entry {
    struct HNode node;
    string key;
    string val;
    uint32_t type = 0;
    ZSet *zset = NULL;
    size_t heap_idx = -1;
};

// Define a function to compare two HNode entries based on their 'key' field
static bool entry_eq(HNode *lhs, HNode *rhs) {
    // Cast the 'lhs' and 'rhs' nodes back to their containing struct Entry using container_of macro
    struct Entry *le = container_of(lhs, struct Entry, node);
    struct Entry *re = container_of(rhs, struct Entry, node);

    // Compare the 'key' field of both entries
    return le->key == re->key;
}

enum {
    ERR_UNKNOWN = 1,  // Represents an unknown error
    ERR_2BIG = 2,     // Represents an error indicating a size that's too big
    ERR_TYPE = 3,     // Represents an error related to an incorrect type
    ERR_ARG = 4,      // Represents an error related to an incorrect argument
};


static void out_nil(string& out){
    out.push_back(SER_NIL); // Append the SER_NIL byte to indicate a nil value
}

// Function to serialize a string and append it to the output buffer.
static void out_str(string &out, const char *s, size_t size) {
    out.push_back(SER_STR);           // Append the string type identifier.
    uint32_t len = (uint32_t)size;    // Get the length of the string.
    out.append((char *)&len, 4);      // Append the length of the string as 4 bytes.
    out.append(s, len);               // Append the actual string data.
}

// Overloaded function to serialize a string and append it to the output buffer.
static void out_str(string &out, const string &val) {
    return out_str(out, val.data(), val.size()); // Call the previous function with string data.
}


static void out_int(string &out, int64_t val) {
    out.push_back(SER_INT); // Append the SER_INT byte to indicate an integer
    out.append((char *)&val, 8); // Append the integer value (8 bytes)
}

// Function to serialize a double and append it to the output buffer.
static void out_dbl(string &out, double val) {
    out.push_back(SER_DBL);         // Append the double type identifier.
    out.append((char *)&val, 8);    // Append the 8-byte representation of the double value.
}

static void out_err(string &out, int32_t code, const string &msg) {
    out.push_back(SER_ERR); // Append the SER_ERR byte to indicate an error
    out.append((char *)&code, 4); // Append the error code (4 bytes)
    uint32_t len = (uint32_t)msg.size(); // Get the length of the error message
    out.append((char *)&len, 4); // Append the length of the error message (4 bytes)
    out.append(msg); // Append the actual error message
}

static void out_arr(string &out, uint32_t n) {
    out.push_back(SER_ARR); // Append the SER_ARR byte to indicate an array
    out.append((char *)&n, 4); // Append the number of elements in the array (4 bytes)
}

// Function to begin an array serialization
static void *begin_arr(string &out) {
    out.push_back(SER_ARR);            // Append the array type identifier
    out.append("\0\0\0\0", 4);         // Reserve space for the array length (4 bytes)
    return (void *)(out.size() - 4);   // Return the position (ctx) where the array length will be written later
}

// Function to end an array serialization
static void end_arr(string &out, void *ctx, uint32_t n) {
    size_t pos = (size_t)ctx;          // Retrieve the position (ctx) where the array length is to be written
    assert(out[pos - 1] == SER_ARR);   // Ensure that the position corresponds to an array type identifier
    memcpy(&out[pos], &n, 4);          // Write the array length at the reserved position
}

// Function to handle a "get" command
static void do_get(vector<string> &cmd, string &out) {
    Entry key;
    key.key.swap(cmd[1]);   // Move the key from the command into the 'key' variable
    key.node.hcode = str_hash((uint8_t *)key.key.data(), key.key.size()); // Compute the hash code for the key

    // Look up the key in the hash map
    HNode *node = hm_lookup(&g_data.db, &key.node, &entry_eq);
    if (!node) {    // If the key is not found
        return out_nil(out);    // Output a "nil" value
    }

    // Retrieve the Entry corresponding to the found node
    Entry *ent = container_of(node, Entry, node);
    if (ent->type != T_STR) {   // If the entry type is not a string
        return out_err(out, ERR_TYPE, "expect string type");    // Output an error indicating the type mismatch
    }
    return out_str(out, ent->val);    // Output the string value of the entry
}


static void do_set(vector<string> &cmd, string &out) {  // Handles setting values in a database
    Entry key;  // Create an Entry object for the key-value pair
    key.key.swap(cmd[1]);  // Extract and take ownership of the key from cmd[1]
    key.node.hcode = str_hash((uint8_t *)key.key.data(), key.key.size());  // Calculate hash code for the key

    HNode *node = hm_lookup(&g_data.db, &key.node, &entry_eq);  // Look up the key in the global database
    if (node) {  // If key exists
        Entry *ent = container_of(node, Entry, node);  // Retrieve the Entry associated with the key
        if (ent->type != T_STR) {  // Check if the Entry type is not string
            return out_err(out, ERR_TYPE, "expect string type");  // Return error for incorrect type
        }
        ent->val.swap(cmd[2]);  // Replace current value with new value from cmd[2]
    } else {  // If key does not exist
        Entry *ent = new Entry();  // Create a new Entry object
        ent->key.swap(key.key);  // Take ownership of the key
        ent->node.hcode = key.node.hcode;  // Set hash code for the Entry
        ent->val.swap(cmd[2]);  // Take ownership of the value from cmd[2]
        hm_insert(&g_data.db, &ent->node);  // Insert Entry into the database
    }
    return out_nil(out);  // Output nil value
}

// set or remove the TTL
static void entry_set_ttl(Entry *ent, int64_t ttl_ms) {
    if (ttl_ms < 0 && ent->heap_idx != (size_t)-1) {
        // erase an item from the heap
        // by replacing it with the last item in the array.
        size_t pos = ent->heap_idx;
        g_data.heap[pos] = g_data.heap.back();
        g_data.heap.pop_back();
        if (pos < g_data.heap.size()) {
            heap_update(g_data.heap.data(), pos, g_data.heap.size());
        }
        ent->heap_idx = -1; // Mark as removed from heap
    } else if (ttl_ms >= 0) {
        size_t pos = ent->heap_idx;
        if (pos == (size_t)-1) {
            // add a new item to the heap
            HeapItem item;
            item.ref = &ent->heap_idx;
            g_data.heap.push_back(item);
            pos = g_data.heap.size() - 1;
        }
        g_data.heap[pos].val = get_monotonic_usec() + (uint64_t)ttl_ms * 1000; // Set expiration time
        heap_update(g_data.heap.data(), pos, g_data.heap.size()); // Update heap
    }
}

static bool str2int(const string &s, int64_t &out) {
    char *endp = NULL;
    out = strtoll(s.c_str(), &endp, 10);
    return endp == s.c_str() + s.size(); // Return true if the entire string was converted
}

static void do_expire(vector<string> &cmd, string &out) {
    int64_t ttl_ms = 0;
    if (!str2int(cmd[2], ttl_ms)) {
        return out_err(out, ERR_ARG, "expect int64"); // Error if the argument is not an int
    }

    Entry key;
    key.key.swap(cmd[1]);
    key.node.hcode = str_hash((uint8_t *)key.key.data(), key.key.size());

    HNode *node = hm_lookup(&g_data.db, &key.node, &entry_eq);
    if (node) {
        Entry *ent = container_of(node, Entry, node);
        entry_set_ttl(ent, ttl_ms); // Set TTL
    }
    return out_int(out, node ? 1 : 0); // Return 1 if node found and TTL set, else 0
}

static void do_ttl(vector<string> &cmd, string &out) {
    Entry key;
    key.key.swap(cmd[1]);
    key.node.hcode = str_hash((uint8_t *)key.key.data(), key.key.size());

    HNode *node = hm_lookup(&g_data.db, &key.node, &entry_eq);
    if (!node) {
        return out_int(out, -2); // Return -2 if key not found
    }

    Entry *ent = container_of(node, Entry, node);
    if (ent->heap_idx == (size_t)-1) {
        return out_int(out, -1); // Return -1 if no TTL set
    }

    uint64_t expire_at = g_data.heap[ent->heap_idx].val;
    uint64_t now_us = get_monotonic_usec();
    return out_int(out, expire_at > now_us ? (expire_at - now_us) / 1000 : 0); // Return remaining TTL in ms
}


static void entry_del(Entry *ent) {  // Deletes an Entry from the database
    switch (ent->type) {  // Check the type of the Entry
    case T_ZSET:  // If Entry type is a sorted set
        zset_dispose(ent->zset);  // Dispose of the associated sorted set
        delete ent->zset;  // Deallocate memory for the sorted set object
        break;
    }
    entry_set_ttl(ent, -1);
    delete ent;  // Deallocate memory for the Entry object
}

static void do_del(vector<string> &cmd, string &out) {  // Handles deleting an entry from the database
    Entry key;  // Create an Entry object for the key to be deleted
    key.key.swap(cmd[1]);  // Extract and take ownership of the key from cmd[1]
    key.node.hcode = str_hash((uint8_t *)key.key.data(), key.key.size());  // Calculate hash code for the key

    HNode *node = hm_pop(&g_data.db, &key.node, &entry_eq);  // Pop the Entry associated with the key from the database
    if (node) {  // If Entry was found and removed
        entry_del(container_of(node, Entry, node));  // Delete the Entry
    }
    return out_int(out, node ? 1 : 0);  // Output 1 if Entry was deleted, otherwise 0
}

static void h_scan(HTab *tab, void (*f)(HNode *, void *), void *arg) {  // Scans through a hash table and applies a function to each node
    if (tab->size == 0) {  // If hash table is empty, return immediately
        return;
    }
    for (size_t i = 0; i < tab->mask + 1; ++i) {  // Iterate through each slot in the hash table
        HNode *node = tab->tab[i];  // Get the first node in the current slot
        while (node) {  // Iterate through linked list of nodes in the slot
            f(node, arg);  // Apply the function f to the current node with argument arg
            node = node->next;  // Move to the next node in the linked list
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

// Function to convert string to double
static bool str2dbl(const string &s, double &out) {
    char *endp = NULL;
    out = strtod(s.c_str(), &endp);  // Convert string to double
    return endp == s.c_str() + s.size() && !isnan(out);  // Check if conversion successful and result is not NaN
}

// Function to handle ZADD command
static void do_zadd(vector<string> &cmd, string &out) {
    double score = 0;
    if (!str2dbl(cmd[2], score)) {
        return out_err(out, ERR_ARG, "expect fp number");  // Return error if score conversion fails
    }

    // Look up or create the zset entry
    Entry key;
    key.key.swap(cmd[1]);
    key.node.hcode = str_hash((uint8_t *)key.key.data(), key.key.size());
    HNode *hnode = hm_lookup(&g_data.db, &key.node, &entry_eq);

    Entry *ent = NULL;
    if (!hnode) {
        ent = new Entry();
        ent->key.swap(key.key);
        ent->node.hcode = key.node.hcode;
        ent->type = T_ZSET;
        ent->zset = new ZSet();
        hm_insert(&g_data.db, &ent->node);  // Insert new zset entry into database
    } else {
        ent = container_of(hnode, Entry, node);
        if (ent->type != T_ZSET) {
            return out_err(out, ERR_TYPE, "expect zset");  // Return error if existing entry is not a zset
        }
    }

    // Add or update the tuple in the zset
    const string &name = cmd[3];
    bool added = zset_add(ent->zset, name.data(), name.size(), score);  // Add or update tuple in zset
    return out_int(out, (int64_t)added);  // Return result as integer (1 if added, 0 if updated)
}

// Function to expect a zset entry and return false if not found or not a zset
static bool expect_zset(string &out, string &s, Entry **ent) {
    Entry key;
    key.key.swap(s);
    key.node.hcode = str_hash((uint8_t *)key.key.data(), key.key.size());
    HNode *hnode = hm_lookup(&g_data.db, &key.node, &entry_eq);
    if (!hnode) {
        out_nil(out);  // Output nil if entry not found
        return false;
    }

    *ent = container_of(hnode, Entry, node);
    if ((*ent)->type != T_ZSET) {
        out_err(out, ERR_TYPE, "expect zset");  // Output error if entry is not a zset
        return false;
    }
    return true;
}

// Function to handle ZREM command
static void do_zrem(vector<string> &cmd, string &out) {
    Entry *ent = NULL;
    if (!expect_zset(out, cmd[1], &ent)) {
        return;  // Return if entry is not a zset or not found
    }

    const string &name = cmd[2];
    ZNode *znode = zset_pop(ent->zset, name.data(), name.size());  // Remove tuple from zset
    if (znode) {
        znode_del(znode);  // Delete znode if found
    }
    return out_int(out, znode ? 1 : 0);  // Return result as integer (1 if deleted, 0 if not found)
}

// Function to handle ZSCORE command
static void do_zscore(vector<string> &cmd, string &out) {
    Entry *ent = NULL;
    if (!expect_zset(out, cmd[1], &ent)) {
        return;  // Return if entry is not a zset or not found
    }

    const string &name = cmd[2];
    ZNode *znode = zset_lookup(ent->zset, name.data(), name.size());  // Lookup tuple in zset
    return znode ? out_dbl(out, znode->score) : out_nil(out);  // Output score or nil if not found
}

// Function to handle ZQUERY command
static void do_zquery(vector<string> &cmd, string &out) {
    // Parse command arguments
    double score = 0;
    if (!str2dbl(cmd[2], score)) {
        return out_err(out, ERR_ARG, "expect fp number");  // Return error if score conversion fails
    }
    const string &name = cmd[3];
    int64_t offset = 0;
    int64_t limit = 0;
    if (!str2int(cmd[4], offset)) {
        return out_err(out, ERR_ARG, "expect int");  // Return error if offset conversion fails
    }
    if (!str2int(cmd[5], limit)) {
        return out_err(out, ERR_ARG, "expect int");  // Return error if limit conversion fails
    }

    // Retrieve zset entry from database
    Entry *ent = NULL;
    if (!expect_zset(out, cmd[1], &ent)) {
        if (out[0] == SER_NIL) {
            out.clear();  // Clear output if entry is nil
            out_arr(out, 0);  // Output empty array
        }
        return;
    }

    // Perform zset query
    if (limit <= 0) {
        return out_arr(out, 0);  // Output empty array if limit is zero or negative
    }
    ZNode *znode = zset_query(ent->zset, score, name.data(), name.size());  // Query zset for tuples
    znode = znode_offset(znode, offset);  // Offset znode by specified offset

    // Output results
    void *arr = begin_arr(out);  // Begin serialization of array in output
    uint32_t n = 0;
    while (znode && (int64_t)n < limit) {
        out_str(out, znode->name, znode->len);  // Output tuple name
        out_dbl(out, znode->score);  // Output tuple score
        znode = znode_offset(znode, +1);  // Move to next znode
        n += 2;  // Increment count of serialized elements
    }
    end_arr(out, arr, n);  // End serialization of array in output
}

static bool cmd_is(const string& word, const char* cmd) {
    return strcasecmp(word.c_str(), cmd) == 0; // Compare word with cmd ignoring case differences
}

// Function to handle various commands based on input cmd vector
static void do_request(std::vector<std::string> &cmd, std::string &out) {
    if (cmd.size() == 1 && cmd_is(cmd[0], "keys")) {            // Check if the command is "keys" with no additional arguments
        do_keys(cmd, out);                                      // Call the function to handle "keys" command
    } else if (cmd.size() == 2 && cmd_is(cmd[0], "get")) {      // Check if the command is "get" with one argument
        do_get(cmd, out);                                       // Call the function to handle "get" command
    } else if (cmd.size() == 3 && cmd_is(cmd[0], "set")) {      // Check if the command is "set" with two arguments
        do_set(cmd, out);                                       // Call the function to handle "set" command
    } else if (cmd.size() == 2 && cmd_is(cmd[0], "del")) {      // Check if the command is "del" with one argument
        do_del(cmd, out);                                       // Call the function to handle "del" command
    } else if (cmd.size() == 3 && cmd_is(cmd[0], "pexpire")) {  // Check if the command is "pexpire" with two arguments
        do_expire(cmd, out);                                    // Call the function to handle "pexpire" command
    } else if (cmd.size() == 2 && cmd_is(cmd[0], "pttl")) {     // Check if the command is "pttl" with one argument
        do_ttl(cmd, out);                                       // Call the function to handle "pttl" command
    } else if (cmd.size() == 4 && cmd_is(cmd[0], "zadd")) {     // Check if the command is "zadd" with three arguments
        do_zadd(cmd, out);                                      // Call the function to handle "zadd" command
    } else if (cmd.size() == 3 && cmd_is(cmd[0], "zrem")) {     // Check if the command is "zrem" with two arguments
        do_zrem(cmd, out);                                      // Call the function to handle "zrem" command
    } else if (cmd.size() == 3 && cmd_is(cmd[0], "zscore")) {   // Check if the command is "zscore" with two arguments
        do_zscore(cmd, out);                                    // Call the function to handle "zscore" command
    } else if (cmd.size() == 6 && cmd_is(cmd[0], "zquery")) {   // Check if the command is "zquery" with five arguments
        do_zquery(cmd, out);                                    // Call the function to handle "zquery" command
    } else {
        // cmd is not recognized
        out_err(out, ERR_UNKNOWN, "Unknown cmd");               // Handle unknown command
    }
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

static void connection_io(Conn *conn) {
    // waked up by poll, update the idle timer
    // by moving conn to the end of the list.
    conn->idle_start = get_monotonic_usec(); // Set the idle_start timestamp to the current monotonic time in microseconds
    dlist_detach(&conn->idle_list); // Remove conn from its current position in the idle list
    dlist_insert_before(&g_data.idle_list, &conn->idle_list); // Insert conn at the end of the idle list

    // do the work
    if (conn->state == STATE_REQ) {
        state_req(conn); // Process the request state
    } else if (conn->state == STATE_RES) {
        state_res(conn); // Process the response state
    } else {
        assert(0);  // not expected, indicates an unexpected state
    }
}

const uint64_t k_idle_timeout_ms = 5 * 1000; // Idle timeout in milliseconds (5 seconds)

static uint32_t next_timer_ms() {
    uint64_t now_us = get_monotonic_usec();                     // Get the current monotonic time in microseconds
    uint64_t next_us = (uint64_t)-1;                            // Initialize next_us to the maximum possible value

    // idle timers
    if (!dlist_empty(&g_data.idle_list)) {                      // Check if the idle list is not empty
        Conn *next = container_of(g_data.idle_list.next, Conn, idle_list); // Get the next connection from the idle list
        next_us = next->idle_start + k_idle_timeout_ms * 1000;  // Calculate the next idle timeout in microseconds
    }

    // ttl timers
    if (!g_data.heap.empty() && g_data.heap[0].val < next_us) { // Check if the TTL heap is not empty and the top value is less than next_us
        next_us = g_data.heap[0].val;                           // Update next_us to the top value of the heap
    }

    if (next_us == (uint64_t)-1) {                              // If no timers are set
        return 10000;                                           // Return a default value of 10000 ms
    }

    if (next_us <= now_us) {                                    // If the next timer has already expired
        // missed?
        return 0;                                               // Return 0 to indicate an immediate timeout
    }
    return (uint32_t)((next_us - now_us) / 1000);               // Calculate and return the time until the next timer in milliseconds
}

static void conn_done(Conn *conn) {
    g_data.fd2conn[conn->fd] = NULL; // Remove the connection from the fd2conn map
    (void)close(conn->fd); // Close the file descriptor
    dlist_detach(&conn->idle_list); // Detach the connection from the idle list
    free(conn); // Free the connection object
}

static bool hnode_same(HNode *lhs, HNode *rhs){
    return lhs==rhs;
}

static void process_timers() {
    // the extra 1000us is for the ms resolution of poll()
    uint64_t now_us = get_monotonic_usec() + 1000;  // Get the current time plus 1000 microseconds for poll resolution

    // idle timers
    while (!dlist_empty(&g_data.idle_list)) {  // Check if the idle list is not empty
        Conn *next = container_of(g_data.idle_list.next, Conn, idle_list);  // Get the next connection from the idle list
        uint64_t next_us = next->idle_start + k_idle_timeout_ms * 1000;  // Calculate the next idle timeout in microseconds
        if (next_us >= now_us) {  // If the next idle timeout is not ready yet
            // not ready
            break;
        }

        printf("removing idle connection: %d\n", next->fd);  // Print the file descriptor of the idle connection being removed
        conn_done(next);  // Remove the idle connection
    }

    // TTL timers
    const size_t k_max_works = 2000;  // Set the maximum number of works to prevent server stalling
    size_t nworks = 0;  // Initialize the number of works done to 0
    while (!g_data.heap.empty() && g_data.heap[0].val < now_us) {  // Check if the TTL heap is not empty and the top value is less than the current time
        Entry *ent = container_of(g_data.heap[0].ref, Entry, heap_idx);  // Get the entry associated with the top value of the heap
        HNode *node = hm_pop(&g_data.db, &ent->node, &hnode_same);  // Remove the node from the hash map
        assert(node == &ent->node);  // Ensure the node is the same as the entry's node
        entry_del(ent);  // Delete the entry
        if (nworks++ >= k_max_works) {  // If the number of works exceeds the maximum allowed
            // don't stall the server if too many keys are expiring at once
            break;  // Exit the loop to prevent server stalling
        }
    }
}


int main(){
    dlist_init(&g_data.idle_list);

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

    fd_set_nb(fd); // Set the server socket to non-blocking mode

    vector<struct pollfd> poll_args; // Vector to store pollfd structures for polling
    while (true) { // Infinite loop to continuously handle connections
        poll_args.clear(); // Clear the poll_args vector for each iteration
        struct pollfd pfd = {fd, POLLIN, 0}; // Initialize pollfd structure for the server socket
        poll_args.push_back(pfd); // Add the server socket pollfd to the vector

        for (Conn* conn : g_data.fd2conn) { // Iterate over all connections
            if (!conn) continue; // Skip if the connection is NULL
            struct pollfd pfd = {}; // Initialize pollfd structure for the connection
            pfd.fd = conn->fd; // Set the file descriptor
            pfd.events = (conn->state == STATE_REQ) ? POLLIN : POLLOUT; // Set events based on connection state
            pfd.events = pfd.events | POLLERR; // Always monitor for errors
            poll_args.push_back(pfd); // Add the connection pollfd to the vector
        }
        int timeout_ms = (int)next_timer_ms();
        int rv = poll(poll_args.data(), (nfds_t)poll_args.size(), timeout_ms); // Call poll() with the pollfd array
        if (rv < 0) { // If poll() returns an error
            die("poll"); // Log and exit on poll error
        }

        for (size_t i = 1; i < poll_args.size(); i++) { // Iterate over all connections in poll_args
            if (poll_args[i].revents) { // If there are events on the connection
                Conn* conn = g_data.fd2conn[poll_args[i].fd]; // Get the connection object
                connection_io(conn); // Handle the connection IO
                if (conn->state == STATE_END) { // If the connection is in the end state
                    conn_done(conn);
                }
            }
        }

        process_timers();

        if (poll_args[0].revents) { // If there are events on the server socket
            (void)accept_new_conn(fd); // Accept new connection
        }
    }

    return 0;
}