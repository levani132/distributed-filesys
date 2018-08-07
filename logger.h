#pragma once
#include <bits/types/FILE.h>

void logger(const char * storage_name, const char * server_addr, const char * fmt, ...);
void loggerf(const char * fmt, ...);
void logger_error(const char* filename, int line, const char * fmt, ...);
void logger_set_file(FILE * file_in);
void logger_unset_file();

#define LOGGER_ERROR(fmt, ...) logger_error(__FILE__, __LINE__, fmt, __VA_ARGS__)
#define LOGGER(fmt, ...) logger(STORAGE.diskname, STORAGE.servers[i], fmt, __VA_ARGS__)