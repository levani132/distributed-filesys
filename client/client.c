#define FUSE_USE_VERSION 30
#include <fuse.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <error.h>
#include <stdio.h>
#include <sys/stat.h>

#include "client_config.h"
#include "../logger.h"
#define STORAGE (*(struct storage*)fuse_get_context()->private_data)

struct config config;



int client_getattr(const char *path, struct stat *statbuf)
{
    memset(statbuf, 0, sizeof(struct stat));
	if (strcmp(path, "/") == 0) {
		statbuf->st_mode = S_IFDIR | 0755;
		statbuf->st_nlink = 2;
	} else {
		statbuf->st_mode = S_IFREG | 0444;
		statbuf->st_nlink = 1;
		statbuf->st_size = 5;
	}
    return 0;
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
    return 0;
}

/** Change the access and/or modification times of a file */
int client_utime(const char *path, struct utimbuf *ubuf)
{
    return 0;
}

/** File open operation */
int client_open(const char *path, struct fuse_file_info *fi)
{
    return 0;
}

/** Read data from an open file */
int client_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	return 0;
}

/** Write data to an open file */
int client_write(const char *path, const char *buf, size_t size, off_t offset,
	     struct fuse_file_info *fi)
{
    return 0;
}


/** Release an open file */
int client_release(const char *path, struct fuse_file_info *fi)
{
    return 0;
}

/** Open directory */
int client_opendir(const char *path, struct fuse_file_info *fi)
{
    return 0;
}

/** Read directory */
int client_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
	       struct fuse_file_info *fi)
{
	(void) offset;
	(void) fi;

	if (strcmp(path, "/") != 0)
		return -ENOENT;
	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
	//filler(buf, fuse_get_context()->private_data, NULL, 0);
	filler(buf, config.errorlog, NULL, 0);
	filler(buf, STORAGE.diskname, NULL, 0);
    return 0;
}

/** Release directory */
int client_releasedir(const char *path, struct fuse_file_info *fi)
{
    return 0;
}

struct fuse_operations client_operations = {
  .getattr = client_getattr,
//   .mknod = client_mknod,
//   .mkdir = client_mkdir,
//   .unlink = client_unlink,
//   .rmdir = client_rmdir,
//   .rename = client_rename,
//   .truncate = client_truncate,
//   .utime = client_utime,
  .open = client_open,
  .read = client_read,
//   .write = client_write,
//   .release = client_release,
//   .opendir = client_opendir,
  .readdir = client_readdir,
//   .releasedir = client_releasedir
};

int main (int argc, char *argv[]) {
    if(argc < 2){
        logger(NULL, NULL, "specify config file");
        return -1;
    }
    config_init(&config, argv[1]);
    int res = 0;
    int i = 0;for(; i < config.n_storages; i++){
        logger(NULL, NULL, "%d %d", i, config.n_storages);
        int pid = 0;
        pid = fork();
        if(pid != 0){
            char *argv_i[2];
            argv_i[0] = argv[0];
            argv_i[1] = config.storages[i].mountpoint;
            logger(NULL, NULL, "mounting %s", config.storages[i].mountpoint);
            res |= fuse_main(2, argv_i, &client_operations, &config.storages[i]);
            break;
        }else{
            logger(NULL, NULL, "forked %d %d", i, config.n_storages);
        }
    }
	return res;
}