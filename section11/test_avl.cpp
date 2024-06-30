#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <set>
#include "avl.cpp"  // lazy inclusion of AVL tree implementation
using namespace std;

#define container_of(ptr, type, member) ({                  \
    const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
    (type *)( (char *)__mptr - offsetof(type, member) );})  // macro to get container struct from member pointer

struct Data {
    AVLNode node;    // AVL tree node
    uint32_t val = 0;  // value associated with this node
};

struct Container {
    AVLNode *root = NULL;  // root of the AVL tree
};

static void add(Container &c, uint32_t val) {
    Data *data = new Data();    // allocate the data
    avl_init(&data->node);      // initialize AVL node
    data->val = val;            // set value

    AVLNode *cur = NULL;        // current node
    AVLNode **from = &c.root;   // the incoming pointer to the next node
    while (*from) {             // tree search
        cur = *from;
        uint32_t node_val = container_of(cur, Data, node)->val;  // get value of current node
        from = (val < node_val) ? &cur->left : &cur->right;  // decide direction of tree traversal
    }
    *from = &data->node;        // attach the new node
    data->node.parent = cur;    // set parent
    c.root = avl_fix(&data->node);  // fix AVL tree and update root
}

static bool del(Container &c, uint32_t val) {
    AVLNode *cur = c.root;
    while (cur) {
        uint32_t node_val = container_of(cur, Data, node)->val;  // get value of current node
        if (val == node_val) {
            break;  // found the node to delete
        }
        cur = val < node_val ? cur->left : cur->right;  // decide direction of tree traversal
    }
    if (!cur) {
        return false;  // value not found
    }

    c.root = avl_del(cur);  // delete the node and update root
    delete container_of(cur, Data, node);  // deallocate data
    return true;  // deletion successful
}

static void avl_verify(AVLNode *parent, AVLNode *node) {
    if (!node) {
        return;
    }

    assert(node->parent == parent);  // verify parent pointer
    avl_verify(node, node->left);    // verify left subtree
    avl_verify(node, node->right);   // verify right subtree

    assert(node->cnt == 1 + avl_cnt(node->left) + avl_cnt(node->right));  // verify node count

    uint32_t l = avl_depth(node->left);  // left subtree depth
    uint32_t r = avl_depth(node->right); // right subtree depth
    assert(l == r || l + 1 == r || l == r + 1);  // verify AVL property
    assert(node->depth == 1 + max(l, r));        // verify depth

    uint32_t val = container_of(node, Data, node)->val;  // get node value
    if (node->left) {
        assert(node->left->parent == node);  // verify left child parent pointer
        assert(container_of(node->left, Data, node)->val <= val);  // verify left child value
    }
    if (node->right) {
        assert(node->right->parent == node);  // verify right child parent pointer
        assert(container_of(node->right, Data, node)->val >= val);  // verify right child value
    }
}

static void extract(AVLNode *node, multiset<uint32_t> &extracted) {
    if (!node) {
        return;
    }
    extract(node->left, extracted);  // extract left subtree
    extracted.insert(container_of(node, Data, node)->val);  // insert current node value
    extract(node->right, extracted);  // extract right subtree
}

static void container_verify(Container &c, const multiset<uint32_t> &ref) {
    avl_verify(NULL, c.root);  // verify AVL tree
    assert(avl_cnt(c.root) == ref.size());  // verify tree size
    multiset<uint32_t> extracted;
    extract(c.root, extracted);  // extract values from tree
    assert(extracted == ref);  // verify extracted values
}

static void dispose(Container &c) {
    while (c.root) {
        AVLNode *node = c.root;
        c.root = avl_del(c.root);  // delete root
        delete container_of(node, Data, node);  // deallocate data
    }
}

static void test_insert(uint32_t sz) {
    for (uint32_t val = 0; val < sz; ++val) {
        Container c;
        multiset<uint32_t> ref;
        for (uint32_t i = 0; i < sz; ++i) {
            if (i == val) {
                continue;
            }
            add(c, i);  // add value to tree
            ref.insert(i);  // add value to reference set
        }
        container_verify(c, ref);  // verify container

        add(c, val);  // add missing value
        ref.insert(val);  // add missing value to reference set
        container_verify(c, ref);  // verify container
        dispose(c);  // dispose container
    }
}

static void test_insert_dup(uint32_t sz) {
    for (uint32_t val = 0; val < sz; ++val) {
        Container c;
        multiset<uint32_t> ref;
        for (uint32_t i = 0; i < sz; ++i) {
            add(c, i);  // add value to tree
            ref.insert(i);  // add value to reference set
        }
        container_verify(c, ref);  // verify container

        add(c, val);  // add duplicate value
        ref.insert(val);  // add duplicate value to reference set
        container_verify(c, ref);  // verify container
        dispose(c);  // dispose container
    }
}

static void test_remove(uint32_t sz) {
    for (uint32_t val = 0; val < sz; ++val) {
        Container c;
        multiset<uint32_t> ref;
        for (uint32_t i = 0; i < sz; ++i) {
            add(c, i);  // add value to tree
            ref.insert(i);  // add value to reference set
        }
        container_verify(c, ref);  // verify container

        assert(del(c, val));  // delete value from tree
        ref.erase(val);  // remove value from reference set
        container_verify(c, ref);  // verify container
        dispose(c);  // dispose container
    }
}

int main() {
    Container c;

    // some quick tests
    container_verify(c, {});  // verify empty container
    add(c, 123);  // add value
    container_verify(c, {123});  // verify container with single value
    assert(!del(c, 124));  // try deleting non-existent value
    assert(del(c, 123));  // delete existing value
    container_verify(c, {});  // verify empty container

    // sequential insertion
    multiset<uint32_t> ref;
    for (uint32_t i = 0; i < 1000; i += 3) {
        add(c, i);  // add value
        ref.insert(i);  // add value to reference set
        container_verify(c, ref);  // verify container
    }

    // random insertion
    for (uint32_t i = 0; i < 100; i++) {
        uint32_t val = (uint32_t)rand() % 1000;
        add(c, val);  // add random value
        ref.insert(val);  // add value to reference set
        container_verify(c, ref);  // verify container
    }

    // random deletion
    for (uint32_t i = 0; i < 200; i++) {
        uint32_t val = (uint32_t)rand() % 1000;
        auto it = ref.find(val);
        if (it == ref.end()) {
            assert(!del(c, val));  // try deleting non-existent value
        } else {
            assert(del(c, val));  // delete existing value
            ref.erase(it);  // remove value from reference set
        }
        container_verify(c, ref);  // verify container
    }

    // insertion/deletion at various positions
    for (uint32_t i = 0; i < 200; ++i) {
        test_insert(i);  // test insertion
        test_insert_dup(i);  // test duplicate insertion
        test_remove(i);  // test removal
    }

    dispose(c);  // dispose container
    return 0;
}
