#include "avl.h"

static uint32_t avl_depth(AVLNode *node) {
    return node ? node->depth : 0;  // Return node depth, 0 if NULL
}

static uint32_t avl_cnt(AVLNode *node) {
    return node ? node->cnt : 0;  // Return node count, 0 if NULL
}

static uint32_t max(uint32_t lhs, uint32_t rhs) {
    return lhs < rhs ? rhs : lhs;  // Return maximum of lhs and rhs
}

// Maintaining the depth and count field
static void avl_update(AVLNode *node) {
    node->depth = 1 + max(avl_depth(node->left), avl_depth(node->right));  // Update node depth
    node->cnt = 1 + avl_cnt(node->left) + avl_cnt(node->right);  // Update node count
}

static AVLNode *rot_left(AVLNode *node) {
    AVLNode *new_node = node->right;  // Right child becomes new root
    if (new_node->left) {
        new_node->left->parent = node;  // Update parent of left child
    }
    node->right = new_node->left;  // Move left child
    new_node->left = node;  // Perform rotation
    new_node->parent = node->parent;  // Update parent
    node->parent = new_node;  // Update parent
    avl_update(node);  // Update node
    avl_update(new_node);  // Update new root
    return new_node;  // Return new root
}

static AVLNode *rot_right(AVLNode *node) {
    AVLNode *new_node = node->left;  // Left child becomes new root
    if (new_node->right) {
        new_node->right->parent = node;  // Update parent of right child
    }
    node->left = new_node->right;  // Move right child
    new_node->right = node;  // Perform rotation
    new_node->parent = node->parent;  // Update parent
    node->parent = new_node;  // Update parent
    avl_update(node);  // Update node
    avl_update(new_node);  // Update new root
    return new_node;  // Return new root
}

// The left subtree is too deep
static AVLNode *avl_fix_left(AVLNode *root) {
    if (avl_depth(root->left->left) < avl_depth(root->left->right)) {
        root->left = rot_left(root->left);  // Perform left rotation
    }
    return rot_right(root);  // Perform right rotation
}

// The right subtree is too deep
static AVLNode *avl_fix_right(AVLNode *root) {
    if (avl_depth(root->right->right) < avl_depth(root->right->left)) {
        root->right = rot_right(root->right);  // Perform right rotation
    }
    return rot_left(root);  // Perform left rotation
}

// Fix imbalanced nodes and maintain invariants until the root is reached
AVLNode *avl_fix(AVLNode *node) {
    while (true) {
        avl_update(node);  // Update node
        uint32_t l = avl_depth(node->left);  // Get left depth
        uint32_t r = avl_depth(node->right);  // Get right depth
        AVLNode **from = NULL;
        if (node->parent) {
            from = (node->parent->left == node) ? &node->parent->left : &node->parent->right;  // Get parent reference
        }
        if (l == r + 2) {
            node = avl_fix_left(node);  // Fix left imbalance
        } else if (l + 2 == r) {
            node = avl_fix_right(node);  // Fix right imbalance
        }
        if (!from) {
            return node;  // Return if root
        }
        *from = node;  // Update parent reference
        node = node->parent;  // Move to parent
    }
}

// Detach a node and returns the new root of the tree
AVLNode *avl_del(AVLNode *node) {
    if (node->right == NULL) {
        // No right subtree, replace the node with the left subtree
        // Link the left subtree to the parent
        AVLNode *parent = node->parent;
        if (node->left) {
            node->left->parent = parent;  // Update parent
        }
        if (parent) {
            // Attach the left subtree to the parent
            (parent->left == node ? parent->left : parent->right) = node->left;
            return avl_fix(parent);  // Fix parent
        } else {
            // Removing root?
            return node->left;  // Return left child
        }
    } else {
        // Swap the node with its next sibling
        AVLNode *victim = node->right;
        while (victim->left) {
            victim = victim->left;  // Find next sibling
        }
        AVLNode *root = avl_del(victim);  // Delete sibling

        *victim = *node;  // Copy node data to victim
        if (victim->left) {
            victim->left->parent = victim;  // Update parent
        }
        if (victim->right) {
            victim->right->parent = victim;  // Update parent
        }
        AVLNode *parent = node->parent;
        if (parent) {
            (parent->left == node ? parent->left : parent->right) = victim;  // Update parent reference
            return root;  // Return new root
        } else {
            // Removing root?
            return victim;  // Return victim
        }
    }
}

// offset into the succeeding or preceding node.
// note: the worst-case is O(log(n)) regardless of how long the offset is.
AVLNode *avl_offset(AVLNode *node, int64_t offset) {
    int64_t pos = 0;    // relative to the starting node
    while (offset != pos) {  // Continue until the desired offset is reached
        if (pos < offset && pos + avl_cnt(node->right) >= offset) { 
            // If the target is in the right subtree
            node = node->right;  // Move to the right child
            pos += avl_cnt(node->left) + 1;  // Update position to include left subtree and current node
        } else if (pos > offset && pos - avl_cnt(node->left) <= offset) {
            // If the target is in the left subtree
            node = node->left;  // Move to the left child
            pos -= avl_cnt(node->right) + 1;  // Update position to include right subtree and current node
        } else {
            // If the target is in the parent node
            AVLNode *parent = node->parent;  // Move to the parent node
            if (!parent) {  // If there's no parent, the target is out of bounds
                return NULL;
            }
            if (parent->right == node) {  // If the current node is a right child
                pos -= avl_cnt(node->left) + 1;  // Adjust position to move up from the right subtree
            } else {  // If the current node is a left child
                pos += avl_cnt(node->right) + 1;  // Adjust position to move up from the left subtree
            }
            node = parent;  // Move to the parent node
        }
    }
    return node;  // Return the node at the desired offset
}
