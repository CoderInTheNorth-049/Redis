#pragma once

#include "avl.h"
#include "hashtable.h"

struct ZSet {
    AVLNode *tree = NULL;  // AVL tree to keep nodes ordered by score
    HMap hmap;             // Hash map to allow quick lookup by name
};

struct ZNode {
    AVLNode tree;        // AVL tree node for ordering by score
    HNode hmap;          // Hash map node for quick lookup by name
    double score = 0;    // Score associated with the node
    size_t len = 0;      // Length of the name
    char name[0];        // Name (variable length array)
};

// Adds a node to the sorted set, or updates the score if it already exists
bool zset_add(ZSet *zset, const char *name, size_t len, double score);

// Looks up a node by name in the sorted set
ZNode *zset_lookup(ZSet *zset, const char *name, size_t len);

// Removes and returns a node by name from the sorted set
ZNode *zset_pop(ZSet *zset, const char *name, size_t len);

// Queries a node by score and name in the sorted set
ZNode *zset_query(ZSet *zset, double score, const char *name, size_t len);

// Disposes of all nodes in the sorted set
void zset_dispose(ZSet *zset);

// Finds a node by an offset from the given node
ZNode *znode_offset(ZNode *node, int64_t offset);

// Deletes a node from the sorted set
void znode_del(ZNode *node);
