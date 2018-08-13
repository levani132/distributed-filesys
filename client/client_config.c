#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <error.h>
#include <assert.h>

#include "client_config.h"
#include "client_connector.h"
#include "../logger.h"

int n_storages(FILE * file){
    int count = 0;
    fseek(file, 0, SEEK_END);
    long fsize = ftell(file);
    rewind(file);
    char *file_content = malloc(fsize + 1);
    fread(file_content, fsize, 1, file);
    rewind(file);
    const char *tmp = file_content;
    while((tmp = strstr(tmp, "diskname =")))
    {
        count++;
        tmp++;
    }
    return count;
}

void parse_config(struct config * config, FILE * file){
    char errorlog_tmp[256];
    char cache_replacement_tmp[256];
    assert(0 != fscanf(file, " errorlog = %s cache_size = %dM cache_replacment = %s timeout = %d ",
            errorlog_tmp, &config->cache_size, cache_replacement_tmp, &config->timeout));
    config->errorlog = strdup(errorlog_tmp);
    config->cache_replacement = strdup(cache_replacement_tmp);
    loggerf("parse_config: errorlog - %s", config->errorlog);
    loggerf("parse_config: cache_size - %d", config->cache_size);
    loggerf("parse_config: cache_replacement - %s", config->cache_replacement);
    loggerf("parse_config: timeout - %d", config->timeout);
}

int parse_storage(struct storage * storage, FILE * file){
    char diskname_tmp[256], mountpoint_tmp[256], servers_tmp[256], hotswap_tmp[256];
    if(fscanf(file, " diskname = %s mountpoint = %s raid = %d servers = %[^\n] hotswap = %s",
            diskname_tmp, mountpoint_tmp, &storage->raid, servers_tmp, hotswap_tmp) != EOF){
        storage->mountpoint = strdup(mountpoint_tmp);
        storage->diskname = strdup(diskname_tmp);
        storage->hotswap = strdup(hotswap_tmp);
        servers_tmp[strlen(servers_tmp) - 1] = '\0';
        loggerf("parse_storage: diskname - %s", storage->diskname);
        loggerf("parse_storage: mountpoint - %s", storage->mountpoint);
        loggerf("parse_storage: raid - %d", storage->raid);
        loggerf("parse_storage: hotswap - %s", storage->hotswap);
        char * server = strtok(servers_tmp, ", ");
        storage->n_servers = 0;
        storage->servers = malloc(0);
        while(server != NULL){
            storage->servers = realloc(storage->servers, (storage->n_servers + 1) * sizeof*storage->servers);
            strcpy(storage->servers[storage->n_servers].name, server);
            storage->servers[storage->n_servers].state = connector_ping(server);
            storage->servers[storage->n_servers].n_fds = 0;
            storage->servers[storage->n_servers].fds = NULL;
            if(!storage->servers[storage->n_servers].state){
                loggerf("parse_storage: server - %s couldn't be parsed",  storage->servers[storage->n_servers].name);
                connector_reconnect(&storage->servers[storage->n_servers], storage->hotswap, storage->diskname);
            }
            server = strtok(NULL, ", ");
            loggerf("parse_storage: server - %s", storage->servers[storage->n_servers].name);
            storage->n_servers++;
        }
        assert(storage->n_servers > 0);
        return 1;
    }else{
        return 0;
    }
}

void config_init(struct config * config, char * filename){
    assert(config != NULL);
    assert(filename != NULL);
    loggerf("config is initializing");
    FILE* file = fopen(filename, "r");
    config->n_storages = n_storages(file);
    parse_config(config, file);
    FILE* error_file = fopen(config->errorlog, "w");
    assert(file != NULL);
    loggerf("from now on %s will be used for logs (see next messages there)", config->errorlog);
    logger_set_file(error_file);
    loggerf("------------- LOGS -----------------");
    config->storages = malloc(config->n_storages * sizeof(struct storage));
    memset(config->storages, 0, sizeof * config->storages);
    int i = 0;
    loggerf("config->n_storages: %d", config->n_storages);
    while(parse_storage(&config->storages[i++], file));
    fclose(file);
    assert(config->n_storages > 0);
}

void config_dest(struct config * config){
    free(config->errorlog);
    free(config->cache_replacement);
    int i = 0; for(; i < config->n_storages; i++){
        free(config->storages[i].diskname);
        free(config->storages[i].hotswap);
        free(config->storages[i].mountpoint);
        int j = 0; for(; j < config->storages[i].n_servers; j++){
            free(config->storages[i].servers[j].fds);
        }
        free(config->storages[i].servers);
    }
}

struct fd_wrapper* fd_wrapper_create(long fd, long server_fd){
    struct fd_wrapper* result = malloc(sizeof*result);
    result->fd = fd;
    result->server_fd = server_fd;
    return result;
}

int insert_fd_wrapper(struct server* server, struct fd_wrapper* fd_wrapper){
    int i = 0; for(; i < server->n_fds; i++){
        if(server->fds[i].fd == fd_wrapper->fd){
            server->fds[i].server_fd = fd_wrapper->server_fd;
            free(fd_wrapper);
            return 0;
        }
    }
    server->fds = realloc(server->fds, sizeof(struct fd_wrapper) * (++server->n_fds));
    memcpy(&server->fds[server->n_fds - 1], fd_wrapper, sizeof*fd_wrapper);
    free(fd_wrapper);
    return 0;
}

long get_server_fd(struct server* server, long fd){
    int i = 0; for(; i < server->n_fds; i++){
        if(server->fds[i].fd == fd){
            return server->fds[i].server_fd;
        }
    }
    return -1;
}