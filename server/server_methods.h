#include <stdlib.h>
#include <stdint.h>
#include <utime.h>
#include <linux/limits.h>

typedef struct FileManager {
    char root_path[PATH_MAX];
    intptr_t (*opendir) (const char * path);
    struct message* (*open) (const char * path, int flags);
    char* (*readdir) (intptr_t dp, char* path);
    struct getattr_ans* (*getattr)(const char * path);
    char* (*read)(int fd, size_t size, off_t offset);
    int (*truncate)(char* path, off_t size);
    int (*utime)(const char *__file, struct utimbuf *__file_times);
    int (*mknod)(const char *path, mode_t mode, dev_t dev);
    int (*mkdir)(const char *path, mode_t mode);
    int (*rename)(const char *path, char *newpath);
    int (*unlink)(const char *path);
    int (*rmdir)(const char *path);
    int (*write)(const char* path, int fd, void* data, size_t size, off_t offset);
    int (*restore)(const char* path, const char* server);
    char* (*readall)(const char* path);
    int (*restoreall)(const char* path, const char* server, int first);
    void (*log)(const char* msg);
} *FileManager;

FileManager new_server(char* root_path, void* (*req_msg_data)(struct message* message_to_send, const char* server));