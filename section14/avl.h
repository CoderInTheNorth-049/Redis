#pragma once

#include <stddef.h>
#include <stdint.h>

// Definition of the AVLNode structure
struct AVLNode {
    uint32_t depth = 0; // Depth of the node in the tree
    uint32_t cnt = 0; // Number of nodes in the subtree rooted at this node
    AVLNode *left = NULL; // Pointer to the left child
    AVLNode *right = NULL; // Pointer to the right child
    AVLNode *parent = NULL; // Pointer to the parent node
};

// Initialize an AVL node
void avl_init(AVLNode *node) {
    node->depth = 1; // Initial depth is 1 (the node itself)
    node->cnt = 1; // Initial count is 1 (the node itself)
    node->left = node->right = node->parent = NULL; // No children or parent
}

// Fix the AVL tree properties after insertion or deletion
AVLNode *avl_fix(AVLNode *node);

// Delete a node from the AVL tree
AVLNode *avl_del(AVLNode *node);

// Get the node at a specific offset in the in-order traversal
AVLNode *avl_offset(AVLNode *node, int64_t offset);
