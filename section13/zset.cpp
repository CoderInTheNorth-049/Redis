#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "zset.h"
#include "common.h"

static ZNode *znode_new(const char *name, size_t len, double score) {
    ZNode *node = (ZNode *)malloc(sizeof(ZNode) + len); // allocate memory for the node
    assert(node);
    avl_init(&node->tree);
    node->hmap.next = NULL;
    node->hmap.hcode = str_hash((uint8_t *)name, len); // hash code for the name
    node->score = score;
    node->len = len;
    memcpy(&node->name[0], name, len); // copy the name into the node
    return node;
}

static uint32_t min(size_t lhs, size_t rhs) {
    return lhs < rhs ? lhs : rhs; // return the minimum of two values
}

// compare by the (score, name) tuple
static bool zless(AVLNode *lhs, double score, const char *name, size_t len) {
    ZNode *zl = container_of(lhs, ZNode, tree);
    if (zl->score != score) {
        return zl->score < score; // compare scores first
    }
    int rv = memcmp(zl->name, name, min(zl->len, len)); // compare names if scores are equal
    if (rv != 0) {
        return rv < 0;
    }
    return zl->len < len; // compare lengths if names are equal
}

static bool zless(AVLNode *lhs, AVLNode *rhs) {
    ZNode *zr = container_of(rhs, ZNode, tree);
    return zless(lhs, zr->score, zr->name, zr->len); // compare using the (score, name) tuple
}

// insert into the AVL tree
static void tree_add(ZSet *zset, ZNode *node) {
    AVLNode *cur = NULL; // current node
    AVLNode **from = &zset->tree; // the incoming pointer to the next node
    while (*from) { // tree search
        cur = *from;
        from = zless(&node->tree, cur) ? &cur->left : &cur->right;
    }
    *from = &node->tree; // attach the new node
    node->tree.parent = cur;
    zset->tree = avl_fix(&node->tree); // fix the tree balance
}

// update the score of an existing node (AVL tree reinsertion)
static void zset_update(ZSet *zset, ZNode *node, double score) {
    if (node->score == score) {
        return;
    }
    zset->tree = avl_del(&node->tree); // remove the node from the tree
    node->score = score;
    avl_init(&node->tree);
    tree_add(zset, node); // reinsert the node with the new score
}

// add a new (score, name) tuple, or update the score of the existing tuple
bool zset_add(ZSet *zset, const char *name, size_t len, double score) {
    ZNode *node = zset_lookup(zset, name, len);
    if (node) {
        zset_update(zset, node, score); // update the existing node
        return false;
    } else {
        node = znode_new(name, len, score); // create a new node
        hm_insert(&zset->hmap, &node->hmap); // insert into the hash map
        tree_add(zset, node); // insert into the AVL tree
        return true;
    }
}

// a helper structure for the hashtable lookup
struct HKey {
    HNode node;
    const char *name = NULL;
    size_t len = 0;
};

static bool hcmp(HNode *node, HNode *key) {
    ZNode *znode = container_of(node, ZNode, hmap);
    HKey *hkey = container_of(key, HKey, node);
    if (znode->len != hkey->len) {
        return false;
    }
    return 0 == memcmp(znode->name, hkey->name, znode->len); // compare names
}

// lookup by name
ZNode *zset_lookup(ZSet *zset, const char *name, size_t len) {
    if (!zset->tree) {
        return NULL;
    }

    HKey key;
    key.node.hcode = str_hash((uint8_t *)name, len); // hash code for the name
    key.name = name;
    key.len = len;
    HNode *found = hm_lookup(&zset->hmap, &key.node, &hcmp); // lookup in the hash map
    return found ? container_of(found, ZNode, hmap) : NULL;
}

// deletion by name
ZNode *zset_pop(ZSet *zset, const char *name, size_t len) {
    if (!zset->tree) {
        return NULL;
    }

    HKey key;
    key.node.hcode = str_hash((uint8_t *)name, len); // hash code for the name
    key.name = name;
    key.len = len;
    HNode *found = hm_pop(&zset->hmap, &key.node, &hcmp); // remove from the hash map
    if (!found) {
        return NULL;
    }

    ZNode *node = container_of(found, ZNode, hmap);
    zset->tree = avl_del(&node->tree); // remove from the AVL tree
    return node;
}

// find the (score, name) tuple that is greater or equal to the argument
ZNode *zset_query(ZSet *zset, double score, const char *name, size_t len) {
    AVLNode *found = NULL;
    AVLNode *cur = zset->tree;
    while (cur) {
        if (zless(cur, score, name, len)) {
            cur = cur->right; // move right if current node is less
        } else {
            found = cur; // candidate
            cur = cur->left; // move left otherwise
        }
    }
    return found ? container_of(found, ZNode, tree) : NULL;
}

// offset into the succeeding or preceding node
ZNode *znode_offset(ZNode *node, int64_t offset) {
    AVLNode *tnode = node ? avl_offset(&node->tree, offset) : NULL;
    return tnode ? container_of(tnode, ZNode, tree) : NULL;
}

void znode_del(ZNode *node) {
    free(node); // free the memory allocated for the node
}

static void tree_dispose(AVLNode *node) {
    if (!node) {
        return;
    }
    tree_dispose(node->left); // dispose the left subtree
    tree_dispose(node->right); // dispose the right subtree
    znode_del(container_of(node, ZNode, tree)); // delete the node
}

// destroy the zset
void zset_dispose(ZSet *zset) {
    tree_dispose(zset->tree); // dispose the AVL tree
    hm_destroy(&zset->hmap); // destroy the hash map
}
