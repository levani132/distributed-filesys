#pragma once
#include <stdio.h>

void memswap(void * left, void * right, size_t size);

struct console {
    void (*log)(const char*, ...);
    void (*logger)(const char * storage_name, const char * server_addr, const char * fmt, ...);
    void (*logger_error)(const char* filename, int line, const char * fmt, ...);
    void (*set_file)(FILE * file_in);
    void (*unset_file)();
};

extern struct console console;

#define LOGGER_ERROR(...) console.logger_error(__FILE__, __LINE__, __VA_ARGS__)
#define LOGGER(...) console.logger(STORAGE.diskname, STORAGE.servers[i].name, __VA_ARGS__)