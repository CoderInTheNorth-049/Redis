#pragma once

#include <stddef.h>

// Doubly linked list node structure
struct DList {
    DList *prev = NULL;  // Pointer to the previous node
    DList *next = NULL;  // Pointer to the next node
};

// Initialize the node to point to itself
inline void dlist_init(DList *node) {
    node->prev = node->next = node;  // Both prev and next point to the node itself
}

// Check if the node is the only element in the list
inline bool dlist_empty(DList *node) {
    return node->next == node;  // If next points to itself, the list is empty
}

// Detach the node from the list
inline void dlist_detach(DList *node) {
    DList *prev = node->prev;  // Save the previous node
    DList *next = node->next;  // Save the next node
    prev->next = next;  // Link previous node to next node
    next->prev = prev;  // Link next node to previous node
}

// Insert a rookie node before the target node
inline void dlist_insert_before(DList *target, DList *rookie) {
    DList *prev = target->prev;  // Save the previous node of the target
    prev->next = rookie;  // Link previous node to the rookie
    rookie->prev = prev;  // Link rookie's previous to the previous node
    rookie->next = target;  // Link rookie's next to the target
    target->prev = rookie;  // Link target's previous to the rookie
}
