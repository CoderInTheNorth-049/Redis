#pragma once

#include <stdint.h>
#include <stddef.h>
#include <pthread.h>
#include <vector>
#include <deque>

using namespace std;
// Represents a unit of work to be processed by the thread pool
struct Work {
    void (*f)(void *) = NULL;  // Function pointer for the work
    void *arg = NULL;          // Argument to the function
};

// Represents a thread pool
struct TheadPool {
    vector<pthread_t> threads;    // Vector to hold thread identifiers
    deque<Work> queue;            // Queue to hold the work items
    pthread_mutex_t mu;           // Mutex to protect the queue
    pthread_cond_t not_empty;     // Condition variable to signal that the queue is not empty
};

// Initializes the thread pool with the specified number of threads
void thread_pool_init(TheadPool *tp, size_t num_threads);

// Queues a new work item to be processed by the thread pool
void thread_pool_queue(TheadPool *tp, void (*f)(void *), void *arg);
