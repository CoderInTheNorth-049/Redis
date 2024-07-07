#include <assert.h>
#include "avl.cpp"

#define container_of(ptr, type, member) ({                  \
    const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
    (type *)( (char *)__mptr - offsetof(type, member) );}) // macro to get the container structure from a member pointer

struct Data {
    AVLNode node;
    uint32_t val = 0; // value to store in the node
};

struct Container {
    AVLNode *root = NULL; // root of the AVL tree
};

static void add(Container &c, uint32_t val) {
    Data *data = new Data(); // allocate the data
    avl_init(&data->node); // initialize the AVL node
    data->val = val; // set the value

    if (!c.root) { // if the tree is empty
        c.root = &data->node; // set the root to the new node
        return;
    }

    AVLNode *cur = c.root; // current node
    while (true) {
        AVLNode **from =
            (val < container_of(cur, Data, node)->val) // decide which branch to follow
            ? &cur->left : &cur->right;
        if (!*from) {
            *from = &data->node; // attach the new node
            data->node.parent = cur; // set the parent
            c.root = avl_fix(&data->node); // fix the AVL tree
            break;
        }
        cur = *from; // move to the next node
    }
}

static void dispose(AVLNode *node) {
    if (node) {
        dispose(node->left); // dispose the left subtree
        dispose(node->right); // dispose the right subtree
        delete container_of(node, Data, node); // delete the node
    }
}

static void test_case(uint32_t sz) {
    Container c;
    for (uint32_t i = 0; i < sz; ++i) {
        add(c, i); // add elements to the tree
    }

    AVLNode *min = c.root;
    while (min->left) {
        min = min->left; // find the minimum node
    }
    for (uint32_t i = 0; i < sz; ++i) {
        AVLNode *node = avl_offset(min, (int64_t)i); // get the node at offset i
        assert(container_of(node, Data, node)->val == i); // check the value

        for (uint32_t j = 0; j < sz; ++j) {
            int64_t offset = (int64_t)j - (int64_t)i;
            AVLNode *n2 = avl_offset(node, offset); // get the node at offset j
            assert(container_of(n2, Data, node)->val == j); // check the value
        }
        assert(!avl_offset(node, -(int64_t)i - 1)); // check invalid offset
        assert(!avl_offset(node, sz - i)); // check invalid offset
    }

    dispose(c.root); // dispose the tree
}

int main() {
    for (uint32_t i = 1; i < 500; ++i) {
        test_case(i); // run test cases
    }
    return 0;
}
