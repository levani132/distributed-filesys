#include <stdlib.h>
#include <string.h>
#include "message.h"
#include "logger.h"

char* function_name[] = {
    "",
    "ping",
    "opendir",
    "readdir",
    "getattr",
    "open",
    "read",
    "write",
    "utime",
    "truncate",
    "release",
    "releasedir",
    "mknod",
    "mkdir",
    "rename",
    "unlink",
    "rmdir",
    "restore",
    "readall",
};

struct message* create_message(int function_id, long status, int wait_for_message, const char * small_data){
    struct message* message = malloc(sizeof(struct message));
    if(message == NULL){
        loggerf("malloc failed");
    }
    memset(message, 0, sizeof(struct message));
    message->function_id = function_id;
    message->status = status;
    message->wait_for_message = wait_for_message;
    strcpy(message->small_data, small_data);
    return message;
}

struct message* create_ext_message(int function_id, long status, int wait_for_message, size_t size, off_t offset, const char * small_data){
    struct message* message = create_message(function_id, status, wait_for_message, small_data);
    message->size = size;
    message->offset = offset;
    return message;
}

struct message* create_mk_message(int function_id, mode_t mode, dev_t dev, const char * small_data){
    struct message* message = create_ext_message(function_id, 0, 0, 0, 0, small_data);
    message->mode = mode;
    message->dev = dev;
    return message;
}