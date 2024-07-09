#include <assert.h>
#include "thread_pool.h"

// Worker thread function to process tasks from the queue
static void *worker(void *arg) {
    TheadPool *tp = (TheadPool *)arg;
    while (true) {
        pthread_mutex_lock(&tp->mu);
        // wait for the condition: a non-empty queue
        while (tp->queue.empty()) {
            pthread_cond_wait(&tp->not_empty, &tp->mu);
        }

        // got the job
        Work w = tp->queue.front();
        tp->queue.pop_front();
        pthread_mutex_unlock(&tp->mu);

        // do the work
        w.f(w.arg);
    }
    return NULL;
}

// Initializes the thread pool with the specified number of threads
void thread_pool_init(TheadPool *tp, size_t num_threads) {
    assert(num_threads > 0);

    int rv = pthread_mutex_init(&tp->mu, NULL);  // Initialize mutex
    assert(rv == 0);
    rv = pthread_cond_init(&tp->not_empty, NULL);  // Initialize condition variable
    assert(rv == 0);

    tp->threads.resize(num_threads);  // Resize the thread vector to hold num_threads threads
    for (size_t i = 0; i < num_threads; ++i) {
        int rv = pthread_create(&tp->threads[i], NULL, &worker, tp);  // Create worker threads
        assert(rv == 0);
    }
}

// Queues a new work item to be processed by the thread pool
void thread_pool_queue(TheadPool *tp, void (*f)(void *), void *arg) {
    Work w;
    w.f = f;  // Set function pointer for the work
    w.arg = arg;  // Set argument to the function

    pthread_mutex_lock(&tp->mu);  // Lock mutex
    tp->queue.push_back(w);  // Add work to the queue
    pthread_cond_signal(&tp->not_empty);  // Signal that the queue is not empty
    pthread_mutex_unlock(&tp->mu);  // Unlock mutex
}
