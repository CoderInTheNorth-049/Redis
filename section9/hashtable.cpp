#include<assert.h>
#include<stdlib.h>
#include "hashtable.h"
using namespace std;

static void h_init(HTab* htab, size_t n) {
    assert(n > 0 && ((n - 1) & n) == 0); // Ensure n is a positive power of 2
    htab->tab = (HNode**)calloc(sizeof(HNode*), n); // Allocate memory for the hash table
    htab->mask = n - 1; // Set mask for efficient indexing
    htab->size = 0; // Initialize size to 0
}

static void h_insert(HTab *htab, HNode *node) {
    size_t pos = node->hcode & htab->mask; // Calculate the position using the hash code and mask
    HNode *next = htab->tab[pos]; // Get the current head of the list at this position
    node->next = next; // Insert the new node at the beginning of the list
    htab->tab[pos] = node; // Update the head of the list to the new node
    htab->size++; // Increment the size of the hash table
}

static HNode **h_lookup(HTab *htab, HNode *key, bool (*eq)(HNode *, HNode *)) {
    if (!htab->tab) { // If the table is not initialized
        return NULL; // Return NULL
    }
    size_t pos = key->hcode & htab->mask; // Calculate the position using the hash code and mask
    HNode **from = &htab->tab[pos]; // Pointer to the pointer of the first node at this position
    for (HNode *cur; (cur = *from) != NULL; from = &cur->next) { // Traverse the linked list at this position
        if (cur->hcode == key->hcode && eq(cur, key)) { // If a node with matching hash code and key is found
            return from; // Return the pointer to the pointer of this node
        }
    }
    return NULL; // If no matching node is found, return NULL
}

static HNode *h_detach(HTab *htab, HNode **from) {
    HNode *node = *from; // Get the node to detach
    *from = node->next; // Update the pointer to bypass the detached node
    htab->size--; // Decrement the size of the hash table
    return node; // Return the detached node
}

const size_t k_resizing_work = 128;

static void hm_help_resizing(HMap *hmap) {
    size_t nwork = 0;
    while (nwork < k_resizing_work && hmap->ht2.size > 0) {
        // Scan for nodes from ht2 and move them to ht1
        HNode **from = &hmap->ht2.tab[hmap->resizing_pos];
        if (!*from) { // If no node at current position, move to the next position
            hmap->resizing_pos++;
            continue;
        }

        h_insert(&hmap->ht1, h_detach(&hmap->ht2, from)); // Move node to ht1
        nwork++; // Increment work done
    }

    if (hmap->ht2.size == 0 && hmap->ht2.tab) { // If ht2 is empty and tab is not null
        // Done with resizing, free the old table
        free(hmap->ht2.tab);
        hmap->ht2 = HTab{}; // Reset ht2
    }
}

static void hm_start_resizing(HMap *hmap) {
    assert(hmap->ht2.tab == NULL); // Ensure ht2 is not already in use
    // Create a bigger hashtable and swap them
    hmap->ht2 = hmap->ht1; // Move ht1 to ht2
    h_init(&hmap->ht1, (hmap->ht1.mask + 1) * 2); // Initialize ht1 with double the size
    hmap->resizing_pos = 0; // Reset the resizing position
}

HNode *hm_lookup(HMap *hmap, HNode *key, bool (*eq)(HNode *, HNode *)) {
    hm_help_resizing(hmap); // Assist with resizing if necessary
    HNode **from = h_lookup(&hmap->ht1, key, eq); // Attempt to find the key in ht1
    from = from ? from : h_lookup(&hmap->ht2, key, eq); // If not found, try ht2
    return from ? *from : NULL; // Return the found node or NULL if not found
}

const size_t k_max_load_factor = 8;

void hm_insert(HMap *hmap, HNode *node) {
    if (!hmap->ht1.tab) { // If ht1 is not initialized
        h_init(&hmap->ht1, 4); // Initialize ht1 with a size of 4
    }
    h_insert(&hmap->ht1, node); // Insert the node into ht1

    if (!hmap->ht2.tab) { // If ht2 is not being used (not resizing)
        // Check whether we need to resize
        size_t load_factor = hmap->ht1.size / (hmap->ht1.mask + 1); // Calculate the load factor
        if (load_factor >= k_max_load_factor) { // If load factor exceeds the max allowed
            hm_start_resizing(hmap); // Start resizing
        }
    }
    hm_help_resizing(hmap); // Help with resizing if necessary
}

HNode *hm_pop(HMap *hmap, HNode *key, bool (*eq)(HNode *, HNode *)) {
    hm_help_resizing(hmap); // Help with resizing if necessary
    if (HNode **from = h_lookup(&hmap->ht1, key, eq)) { // Look up the key in ht1
        return h_detach(&hmap->ht1, from); // If found, detach and return the node
    }
    if (HNode **from = h_lookup(&hmap->ht2, key, eq)) { // Look up the key in ht2
        return h_detach(&hmap->ht2, from); // If found, detach and return the node
    }
    return NULL; // Return NULL if the key is not found in either table
}

size_t hm_size(HMap *hmap) {
    return hmap->ht1.size + hmap->ht2.size; // Return the total size of both hash tables
}

void hm_destroy(HMap *hmap) {
    free(hmap->ht1.tab); // Free the memory allocated for the first hash table
    free(hmap->ht2.tab); // Free the memory allocated for the second hash table
    *hmap = HMap{}; // Reset the HMap structure to its initial state
}
