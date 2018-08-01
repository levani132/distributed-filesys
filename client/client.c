#define FUSE_USE_VERSION 30
#include <fuse.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <error.h>
#include <stdio.h>
#include <dirent.h>
#include <linux/limits.h>
#include <sys/stat.h>

#include "client_config.h"
#include "client_connector.h"
#include "../logger.h"
#include "../message.h"
#include "../models/getattr_ans.h"
#define STORAGE (*(struct storage*)fuse_get_context()->private_data)

struct config config;

void log_start(int server, const char * fnc){
    logger(STORAGE.diskname, STORAGE.servers[server], "starting [%s] from connector", fnc);
}

void log_end(int server, const char * fnc){
    logger(STORAGE.diskname, STORAGE.servers[server], "ended    [%s] from connector", fnc);
}


int client_getattr(const char *path, struct stat *statbuf)
{
    log_start(0, "client_getattr");
    struct getattr_ans* data = connector_getattr(path, STORAGE.servers[0]);
    log_end(0, "client_getattr");
    int retval = data->retval;
    memcpy(statbuf, &data->stat, sizeof(struct stat));
    return retval;
}


/** Create a file node */
int client_mknod(const char *path, mode_t mode, dev_t dev)
{
    return 0;
}

/** Create a directory */
int client_mkdir(const char *path, mode_t mode)
{
    return 0;
}

/** Remove a file */
int client_unlink(const char *path)
{
    return 0;
}

/** Remove a directory */
int client_rmdir(const char *path)
{
    return 0;
}

/** Rename a file */
// both path and newpath are fs-relative
int client_rename(const char *path, const char *newpath)
{
    return 0;
}

/** Change the size of a file */
int client_truncate(const char *path, off_t newsize)
{
    log_start(0, "client_truncate");
    int retval = connector_truncate(path, newsize, STORAGE.servers[0]);
    log_end(0, "client_truncate");
    return retval;
}

/** Change the access and/or modification times of a file */
int client_utime(const char *path, struct utimbuf *ubuf)
{
    log_start(0, "client_utime");
    int retval = connector_utime(path, ubuf, STORAGE.servers[0]);
    log_end(0, "client_utime");
    return retval;
}

/** File open operation */
int client_open(const char *path, struct fuse_file_info *fi)
{
    int fd;
    log_start(0, "client_open");
    fd = connector_open(path, fi->flags, STORAGE.servers[0]);
    log_end(0, "client_open");
    fi->fh = fd;
    if(fd < 0){
        loggerf("%s %d", strerror(-fd), -fd);
        return -fd;
    }
    return 0;
}

/** Read data from an open file */
int client_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    log_start(0, "client_read");
    char * server_buf = connector_read(fi->fh, size, offset, STORAGE.servers[0]);
    log_end(0, "client_read");
    strcpy(buf, server_buf);
	return size;
}

/** Write data to an open file */
int client_write(const char *path, const char *buf, size_t size, off_t offset,
	     struct fuse_file_info *fi)
{
    log_start(0, "client_write");
    int res = connector_write(fi->fh, buf, size, offset, STORAGE.servers[0]);
    log_end(0, "client_write");
	if(res < 0)
		res = -errno;
    return res;
}


/** Release an open file */
int client_release(const char *path, struct fuse_file_info *fi)
{
    log_start(0, "client_release");
    int res = connector_release(fi->fh, STORAGE.servers[0]);
    log_end(0, "client_release");
	if(res < 0)
		res = -errno;
    return res;
}

/** Open directory */
int client_opendir(const char *path, struct fuse_file_info *fi)
{
    intptr_t dp;
    log_start(0, "client_opendir");
    dp = connector_opendir(path, STORAGE.servers[0]);
    log_end(0, "client_opendir");
    fi->fh = dp;
    if(dp < 0){
        loggerf("%s %d", strerror(-dp), -dp);
        return -dp;
    }
    return 0;
}

/** Read directory */
int client_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
	       struct fuse_file_info *fi)
{
    log_start(0, "client_readdir");
    char* entries = connector_readdir((uintptr_t) fi->fh, STORAGE.servers[0]);
    log_end(0, "client_readdir");
    int* offsets = (int*)entries;
    int count = offsets[0];
    offsets++;
    if(count < 0){
        logger(STORAGE.diskname, STORAGE.servers[0], "couldn't read from server");
        return -ENOMEM;
    }
	int i = 0; for(; i < count; i++){
        if (filler(buf, (entries + offsets[i]), NULL, 0) != 0) {
    	    return -ENOMEM;
        }
    }
    free(entries);
    return 0;
}

/** Release directory */
int client_releasedir(const char *path, struct fuse_file_info *fi)
{
    log_start(0, "client_releasedir");
    int res = connector_releasedir(fi->fh, STORAGE.servers[0]);
    log_end(0, "client_releasedir");
	if(res < 0)
		res = -errno;
    return res;
}

struct fuse_operations client_operations = {
  .getattr = client_getattr,
//   .mknod = client_mknod,
//   .mkdir = client_mkdir,
//   .unlink = client_unlink,
//   .rmdir = client_rmdir,
//   .rename = client_rename,
  .truncate = client_truncate,
  .utime = client_utime,
  .open = client_open,
  .read = client_read,
  .write = client_write,
  .release = client_release,
  .opendir = client_opendir,
  .readdir = client_readdir,
  .releasedir = client_releasedir,
};

int main (int argc, char *argv[]) {
    if(argc < 2){
        loggerf("specify config file");
        return -1;
    }
    config_init(&config, argv[1]);
    int res = 0;
    int i = 0;for(; i < config.n_storages; i++){
        int pid = 0;
        pid = fork();
        if(pid == 0){
            char *argv_i[3];
            argv_i[0] = argv[0];
            argv_i[1] = config.storages[i].mountpoint;
            argv_i[2] = strdup("-s");
            loggerf("mounting %s", config.storages[i].mountpoint);
            res |= fuse_main(2, argv_i, &client_operations, &config.storages[i]);
            break;
        }
    }
	return res;
}