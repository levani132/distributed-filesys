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
#define STORAGE (*(struct storage*)fuse_get_context()->private_data)

struct config config;

void log_start(int i, const char * fnc){
    LOGGER("starting [%s] from connector", fnc);
}

void log_end(int i, const char * fnc){
    LOGGER("ended    [%s] from connector", fnc);
}


int client_getattr(const char *path, struct stat *statbuf)
{
    log_start(0, "client_getattr");
    struct getattr_ans* data = (struct getattr_ans*)send_and_recv_data(create_message(fnc_getattr, 0, 0, path), STORAGE.servers[0]);
    log_end(0, "client_getattr");
    memcpy(statbuf, &data->stat, sizeof(struct stat));
    int retval = data->retval;
    free(data);
    return retval;
}


/** Create a file node */
int client_mknod(const char *path, mode_t mode, dev_t dev)
{
    log_start(0, "connector_mknod");
    int retval = send_and_recv_status(create_mk_message(fnc_mknod, mode, dev, path), STORAGE.servers[0]);
    log_end(0, "connector_mknod");
    return retval;
}

/** Create a directory */
int client_mkdir(const char *path, mode_t mode)
{
    log_start(0, "connector_mkdir");
    int retval = send_and_recv_status(create_mk_message(fnc_mkdir, mode, 0, path), STORAGE.servers[0]);
    log_end(0, "connector_mkdir");
    return retval;
}

/** Remove a file */
int client_unlink(const char *path)
{
    log_start(0, "connector_unlink");
    int retval = send_and_recv_status(create_ext_message(fnc_unlink, 0, 0, 0, 0, path), STORAGE.servers[0]);
    log_end(0, "connector_unlink");
    return retval;
}

/** Remove a directory */
int client_rmdir(const char *path)
{
    log_start(0, "connector_rmdir");
    int retval = send_and_recv_status(create_ext_message(fnc_rmdir, 0, 0, 0, 0, path), STORAGE.servers[0]);
    log_end(0, "connector_rmdir");
    return retval;
}

/** Rename a file */
// both path and newpath are fs-relative
int client_rename(const char *path, const char *newpath)
{
    log_start(0, "client_rename");
    int retval = send_data_recv_status(create_message(fnc_rename, 0, 0, path), newpath, strlen(newpath) + 1, STORAGE.servers[0]);
    log_end(0, "client_rename");
    return retval;
}

/** Change the size of a file */
int client_truncate(const char *path, off_t newsize)
{
    log_start(0, "client_truncate");
    int retval = send_and_recv_status(create_ext_message(fnc_truncate, 0, 0, 0, newsize, path), STORAGE.servers[0]);
    log_end(0, "client_truncate");
    return retval;
}

/** Change the access and/or modification times of a file */
int client_utime(const char *path, struct utimbuf *ubuf)
{
    log_start(0, "client_utime");
    int retval = send_data_recv_status(create_message(fnc_utime, 0, sizeof*ubuf, path), (const char*)ubuf, sizeof*ubuf, STORAGE.servers[0]);
    log_end(0, "client_utime");
    return retval;
}

/** File open operation */
int client_open(const char *path, struct fuse_file_info *fi)
{
    log_start(0, "client_open");
    int fd = send_and_recv_status(create_message(fnc_open, fi->flags, 0, path), STORAGE.servers[0]);
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
    char * server_buf = (char*)send_and_recv_data(create_ext_message(fnc_read, fi->fh, 0, size, offset, ""), STORAGE.servers[0]);
    log_end(0, "client_read");
    strcpy(buf, server_buf);
	return size;
}

/** Write data to an open file */
int client_write(const char *path, const char *buf, size_t size, off_t offset,
	     struct fuse_file_info *fi)
{
    log_start(0, "client_write");
    int res = send_data_recv_status(create_ext_message(fnc_write, fi->fh, size, size, offset, ""), buf, size, STORAGE.servers[0]);
    log_end(0, "client_write");
	if(res < 0)
		res = -errno;
    return res;
}


/** Release an open file */
int client_release(const char *path, struct fuse_file_info *fi)
{
    log_start(0, "client_release");
    int res = send_and_recv_status(create_message(fnc_release, fi->fh, 0, ""), STORAGE.servers[0]);
    log_end(0, "client_release");
	if(res < 0)
		res = -errno;
    return res;
}

/** Open directory */
int client_opendir(const char *path, struct fuse_file_info *fi)
{
    intptr_t dp;
    int i = 0;
    log_start(i, "client_opendir");
    struct message* message_to_send = create_message(fnc_opendir, 0, 0, path);
    dp = send_and_recv_status(message_to_send, STORAGE.servers[i]);
    log_end(i, "client_opendir");
    fi->fh = dp;
    if(dp < 0){
        LOGGER_ERROR("%s %d", strerror(-dp), -dp);
        return -dp;
    }
    return 0;
}

/** Read directory */
int client_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
	       struct fuse_file_info *fi)
{
    log_start(0, "client_readdir");
    char* entries = (char*)send_and_recv_data(create_message(fnc_readdir, (uintptr_t) fi->fh, 0, ""), STORAGE.servers[0]);
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
    int res = send_and_recv_status(create_message(fnc_releasedir, fi->fh, 0, ""), STORAGE.servers[0]);
    log_end(0, "client_releasedir");
	if(res < 0)
		res = -errno;
    return res;
}

struct fuse_operations client_operations = {
  .getattr = client_getattr,
  .mknod = client_mknod,
  .mkdir = client_mkdir,
  .unlink = client_unlink,
  .rmdir = client_rmdir,
  .rename = client_rename,
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
            res = fuse_main(3, argv_i, &client_operations, &config.storages[i]);
            break;
        }
    }
	return res;
}