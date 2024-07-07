#pragma once

#include <stddef.h>
#include <stdint.h>

struct HeapItem {
    uint64_t val = 0; // Value of the item
    size_t *ref = NULL; // Reference to metadata
};

void heap_update(HeapItem *a, size_t pos, size_t len); // Updates heap item
