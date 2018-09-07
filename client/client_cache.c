#include <stddef.h>
#include <fuse.h>
#include <malloc.h>
#include <fcntl.h>
#include <string.h>
#include <linux/limits.h>

#include "client_cache.h"
#include "client_config.h"
#include "../logger.h"
#include "../protocol.h"

struct entry {
    char* data;
    size_t size;
    char path[PATH_MAX];
    struct entry* older;
    struct entry* newer;
};

struct entry oldest;
struct entry newest;
size_t max_size;
size_t cur_size;

struct entry* find(const char* path){
    struct entry* cur = &oldest;
    while(cur != NULL && strcmp((cur)->path, path)){
        cur = cur->newer;
    }
    return cur;
}

int insert_new(struct entry* entry){
    entry->newer = &newest;
    newest.older->newer = entry;
    entry->older = newest.older;
    newest.older = entry;
    return 0;
}

void remove_entry(const char* path, struct entry *new_entry){
    struct entry* cur = find(path);
    if(cur){
        if(new_entry){
            new_entry->data = cur->data;
            new_entry->size = cur->size;
        }
        cur->older->newer = cur->newer;
        cur->newer->older = cur->older;
        free(cur);
    }
}

int get_space(int size){
    while(size > 0){
        if(oldest.newer == &newest){
            return -1;
        }
        cur_size -= oldest.newer->size;
        struct entry* tmp = oldest.newer;
        oldest.newer = oldest.newer->newer;
        oldest.newer->older = &oldest;
        free(tmp->data);
        free(tmp);
    }
    return 0;
}

int cache_write(const char* path, const char* buf, size_t size, off_t offset){
    console.logger(STORAGE_NAME, NULL, "CACHE starting [client_write]");
    struct entry* new_entry = malloc(sizeof*new_entry);
    strcpy(new_entry->path, path);
    new_entry->data = NULL;
    new_entry->size = 0;
    if(oldest.newer == NULL || oldest.newer == &newest){
        oldest.newer = newest.older = new_entry;
        new_entry->newer = &newest;
        new_entry->older = &oldest;
        new_entry->size = offset + size;
        new_entry->data = malloc(new_entry->size);
        memcpy(new_entry->data + offset, buf, size);
        goto success;
    }
    remove_entry(path, new_entry);
    size_t new_size = offset + size - new_entry->size;
    if(new_size > 0){
        if(cur_size + new_size > max_size && get_space(cur_size + new_size - max_size) < 0){
            if(new_entry->data)
                free(new_entry->data);
            cur_size -= new_entry->size;
            free(new_entry);
            console.logger(STORAGE_NAME, NULL, "CACHE not enough space for %s", path);
            return -1;
        }
        new_entry->data = realloc(new_entry->data, offset + size);
    }
    new_entry->size = offset + size;
    memcpy(new_entry->data + offset, buf, size);
    insert_new(new_entry);
    success:
    console.logger(STORAGE_NAME, NULL, "CACHE ended    [client_write]");
    return 0;
}

char* cache_read(const char* path, size_t size, off_t offset){
    console.logger(STORAGE_NAME, NULL, "CACHE starting [client_read]");
    struct entry* cur = find(path);
    if(cur == NULL){
        goto error;
    }
    cur->older->newer = cur->newer;
    cur->newer->older = cur->older;
    insert_new(cur);
    if(cur->size < offset + size){
        goto error;
    }
    char* res = malloc(size);
    memcpy(res, cur->data + offset, size);
    console.logger(STORAGE_NAME, NULL, "CACHE ended    [client_read]");
    return res;
    error:
    console.logger(STORAGE_NAME, NULL, "CACHE  NO ENTRY");
    return NULL;
}

int cache_rename (const char* path, const char* new_path){
    struct entry* entry = find(path);
    if(!entry){
        return -1;
    }
    strcpy(entry->path, new_path);
    return 1;
}

Cache new_cache (size_t size, size_t dim) {
    max_size = size * dim;
    Cache c = malloc(sizeof(struct cache));
    struct cache tmp = {
        .write = cache_write,
        .read = cache_read,
        .rename = cache_rename
    };
    memcpy(c, &tmp, sizeof(struct cache));
    return c;
}