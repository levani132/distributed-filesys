#pragma once
#include <linux/limits.h>
#include "../protocol.h"

#define SERVER_DOWN 0
#define SERVER_UP 1
#define SERVER_STARTING 2

struct fd_wrapper {
    long fd;
    long server_fd;
};

struct server {
    char name[20];
    int state;
    struct fd_wrapper* fds;
    int n_fds;
};

struct storage {
    char * diskname;
    char * mountpoint;
    int raid;
    int n_servers;
    struct server * servers;
    char * hotswap;
};

struct config {
    char * errorlog;
    int cache_size;
    char * cache_replacement;
    int timeout;
    int n_storages;
    struct storage * storages;
};

void config_init(struct config * config, char * filename, Request req);
void config_dest(struct config * config);

struct fd_wrapper* fd_wrapper_create(long fd, long server_fd);
int insert_fd_wrapper(struct server* server, struct fd_wrapper* fd_wrapper);
long get_server_fd(struct server* server, long fd);
long connector_reconnect(struct server * server, struct storage* storage, int timeout);