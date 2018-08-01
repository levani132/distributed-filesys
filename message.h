#ifndef MESSAGE_H
#define MESSAGE_H

#include <sys/types.h>

struct message {
    int function_id;
    long status;
    int wait_for_message;
    size_t size;
    off_t offset;
    char small_data[256];
};

enum function_id {
    fnc_nothing,
    fnc_opendir,
    fnc_readdir,
    fnc_getattr,
    fnc_open,
    fnc_read,
    fnc_write,
    fnc_utime,
    fnc_truncate,
    fnc_release,
    fnc_releasedir
};

extern char* function_name[];

struct message* create_message(int function_id, long status, int wait_for_message, const char * small_data);
struct message* create_ext_message(int function_id, long status, int wait_for_message, size_t size, off_t offset, const char * small_data);

#endif