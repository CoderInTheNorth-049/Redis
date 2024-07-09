#pragma once

#include <stdint.h>
#include <stddef.h>

#define container_of(ptr, type, member) ({                  \
    const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
    (type *)( (char *)__mptr - offsetof(type, member) );})

inline uint64_t str_hash(const uint8_t *data, size_t len) {
    uint32_t h = 0x811C9DC5; // initial hash value
    for (size_t i = 0; i < len; i++) {
        h = (h + data[i]) * 0x01000193; // FNV-1a hash algorithm
    }
    return h;
}

enum {
    SER_NIL = 0, // null value
    SER_ERR = 1, // error value
    SER_STR = 2, // string value
    SER_INT = 3, // integer value
    SER_DBL = 4, // double value
    SER_ARR = 5, // array value
};
