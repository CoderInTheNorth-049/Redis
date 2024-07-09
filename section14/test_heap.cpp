#include <assert.h>
#include <vector>
#include <map>
#include "heap.cpp"
using namespace std;

struct Data {
    size_t heap_idx = -1; // Index in the heap
};

struct Container {
    vector<HeapItem> heap; // Min-heap of HeapItems
    multimap<uint64_t, Data *> map; // Multimap for value to Data pointer mapping
};

static void dispose(Container &c) {
    for (auto p : c.map) {
        delete p.second; // Free allocated memory for Data pointers
    }
}

static void add(Container &c, uint64_t val) {
    Data *d = new Data(); // Allocate new Data
    c.map.insert(make_pair(val, d)); // Insert into multimap
    HeapItem item;
    item.ref = &d->heap_idx; // Set reference to heap_idx
    item.val = val; // Set value
    c.heap.push_back(item); // Add to heap
    heap_update(c.heap.data(), c.heap.size() - 1, c.heap.size()); // Update heap
}

static void del(Container &c, uint64_t val) {
    auto it = c.map.find(val); // Find value in multimap
    assert(it != c.map.end());
    Data *d = it->second;
    assert(c.heap.at(d->heap_idx).val == val);
    assert(c.heap.at(d->heap_idx).ref == &d->heap_idx);
    c.heap[d->heap_idx] = c.heap.back(); // Replace with last item
    c.heap.pop_back(); // Remove last item
    if (d->heap_idx < c.heap.size()) {
        heap_update(c.heap.data(), d->heap_idx, c.heap.size()); // Update heap
    }
    delete d; // Free allocated memory for Data
    c.map.erase(it); // Erase from multimap
}

static void verify(Container &c) {
    assert(c.heap.size() == c.map.size()); // Verify heap and map sizes are equal
    for (size_t i = 0; i < c.heap.size(); ++i) {
        size_t l = heap_left(i);
        size_t r = heap_right(i);
        assert(l >= c.heap.size() || c.heap[l].val >= c.heap[i].val); // Verify heap property
        assert(r >= c.heap.size() || c.heap[r].val >= c.heap[i].val); // Verify heap property
        assert(*c.heap[i].ref == i); // Verify heap index
    }
}

static void test_case(size_t sz) {
    for (uint32_t j = 0; j < 2 + sz * 2; ++j) {
        Container c;
        for (uint32_t i = 0; i < sz; ++i) {
            add(c, 1 + i * 2); // Add elements to container
        }
        verify(c);

        add(c, j); // Add another element
        verify(c);

        dispose(c); // Dispose container
    }

    for (uint32_t j = 0; j < sz; ++j) {
        Container c;
        for (uint32_t i = 0; i < sz; ++i) {
            add(c, i); // Add elements to container
        }
        verify(c);

        del(c, j); // Delete element
        verify(c);

        dispose(c); // Dispose container
    }
}

int main() {
    for (uint32_t i = 0; i < 200; ++i) {
        test_case(i); // Run test cases
    }
    return 0;
}
