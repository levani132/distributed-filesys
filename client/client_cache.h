#pragma once

#include <fcntl.h>

typedef struct cache {
    char* (*read)(const char* path, size_t size, off_t offset);
    int (*write)(const char* path, const char* buf, size_t size, off_t offset);
    int (*rename)(const char* path, const char* new_path);
} *Cache;

Cache new_cache(size_t size, size_t dim);

#define KB 1024
#define MB KB*KB
#define GB KB*MB
#define TB KB*GB