#pragma once

#include <sys/types.h>
#include "fuse.h"

#define ERRCONNECTION   -132
#define ERRHASH         -133

struct message {
    int function_id;
    long status;
    int wait_for_message;
    size_t size;
    off_t offset;
    mode_t mode;
    dev_t dev;
    char small_data[256];
};

struct getattr_ans {
    int retval;
    struct stat stat;
};

struct server_open_ans {
    int retval;

};

enum function_id {
    fnc_nothing,
    fnc_ping,
    fnc_opendir,
    fnc_readdir,
    fnc_getattr,
    fnc_open,
    fnc_read,
    fnc_write,
    fnc_utime,
    fnc_truncate,
    fnc_release,
    fnc_releasedir,
    fnc_mknod,
    fnc_mkdir,
    fnc_rename,
    fnc_unlink,
    fnc_rmdir,
    fnc_restore,
    fnc_readall,
    fnc_restoreall
};

extern char* function_name[];

struct message* create_message(int function_id, long status, int wait_for_message, const char * small_data);
struct message* create_ext_message(int function_id, long status, int wait_for_message, size_t size, off_t offset, const char * small_data);
struct message* create_mk_message(int function_id, mode_t mode, dev_t dev, const char * small_data);