#include <bits/types/FILE.h>

void logger(const char * storage_name, const char * server_addr, const char * fmt, ...);
void loggerf(const char * fmt, ...);
void logger_set_file(FILE * file_in);
void logger_unset_file();