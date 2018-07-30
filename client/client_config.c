#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <error.h>
#include <assert.h>
#include "client_config.h"
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
    logger(NULL, NULL, "parse_config: errorlog - %s", config->errorlog);
    logger(NULL, NULL, "parse_config: cache_size - %d", config->cache_size);
    logger(NULL, NULL, "parse_config: cache_replacement - %s", config->cache_replacement);
    logger(NULL, NULL, "parse_config: timeout - %d", config->timeout);
    FILE* error_file = fopen(config->errorlog, "w");
    assert(file != NULL);
    logger(NULL, NULL, "from now on %s will be used for logs (see next messages there)", config->errorlog);
    logger_set_file(error_file);
    logger(NULL, NULL, "logs moved in here");
}

int parse_storage(struct storage * storage, FILE * file){
    char diskname_tmp[256], mountpoint_tmp[256], servers_tmp[256], hotswap_tmp[256];
    if(fscanf(file, " diskname = %s mountpoint = %s raid = %d servers = %[^\n] hotswap = %s",
            diskname_tmp, mountpoint_tmp, &storage->raid, servers_tmp, hotswap_tmp) != EOF){
        logger(NULL, NULL, "created storage %s in %s with hotswap %s", diskname_tmp, mountpoint_tmp, hotswap_tmp);
        storage->mountpoint = strdup(mountpoint_tmp);
        storage->diskname = strdup(diskname_tmp);
        storage->hotswap = strdup(hotswap_tmp);
        logger(NULL, NULL, "parse_storage: diskname - %s", storage->diskname);
        logger(NULL, NULL, "parse_storage: mountpoint - %s", storage->mountpoint);
        logger(NULL, NULL, "parse_storage: raid - %d", storage->raid);
        logger(NULL, NULL, "parse_storage: hotswap - %s", storage->hotswap);
        char * server = strtok(servers_tmp, ", ");
        storage->n_servers = 0;
        storage->servers = malloc(0);
        while(server != NULL){
            storage->servers = realloc(storage->servers, (storage->n_servers + 1) * sizeof(char*));
            storage->servers[storage->n_servers] = strdup(server);
            server = strtok(NULL, ", ");
            logger(NULL, NULL, "parse_storage: server - %s", storage->servers[storage->n_servers]);
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
    logger(NULL, NULL, "config is initializing");
    FILE* file = fopen(filename, "r");
    config->n_storages = n_storages(file);
    logger(NULL, NULL, "config->n_storages: %d", config->n_storages);
    parse_config(config, file);
    config->storages = malloc(config->n_storages * sizeof(struct storage));
    memset(config->storages, 0, sizeof * config->storages);
    int i = 0;
    while(parse_storage(&config->storages[i], file)){i++;}
    logger(NULL, NULL, "vaaax %s", config->storages[1].mountpoint);
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
            free(config->storages[i].servers[j]);
        }
        free(config->storages[i].servers);
    }

}