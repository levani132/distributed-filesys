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
#include <sys/mount.h>

#include "client_config.h"
#include "../logger.h"
#include "../message.h"
#include "../protocol.h"
#define STORAGE (*(struct storage*)fuse_get_context()->private_data)

struct config config;
long fd_counter = 0;
Request request;

void log_start(int i, const char * fnc){
    LOGGER("starting [%s] from connector", fnc);
}

void log_end(int i, const char * fnc){
    LOGGER("ended    [%s] from connector", fnc);
}

void shut_down () {
    console.log("fuse is unmounting and shutting down");
    console.log("------------------------------------");
    fuse_unmount(STORAGE.mountpoint, NULL);
    umount(STORAGE.mountpoint);
    config_dest(&config);
    console.unset_file();
    exit(0);
}

void reconnect(int i){
    LOGGER("server down will try to reconnect");
    STORAGE.servers[i].state = SERVER_DOWN;
    int i2 = i;
    while(i2 != STORAGE.n_servers - 1 && STORAGE.servers[i + 1].state){
        memswap(&STORAGE.servers[i2], &STORAGE.servers[(i2 + 1) % STORAGE.n_servers], sizeof(struct server));
        i2++;
        i2 %= STORAGE.n_servers;
    }
    if(i2 == 0){
        LOGGER("all servers down");
        shut_down();
    }
    connector_reconnect(&STORAGE.servers[i2], &STORAGE, config.timeout);
}



int restoreall(int* j){
    int i = *j;
    int ind = ((STORAGE.n_servers + i - 1) % STORAGE.n_servers);
    int status = request->msg_status(create_message(fnc_restoreall, 0, 0, STORAGE.servers[ind].name), STORAGE.servers[i].name);
    if(status == 0){
        LOGGER("restored data from %s", STORAGE.servers[ind].name);
        STORAGE.servers[i].state = SERVER_UP;
        return 1;
    }else if(status == ERRCONNECTION){
        LOGGER("couldn't restore data");
        LOGGER("connection still down");
        LOGGER("starting reconnection");
        reconnect(*j--);
        return 0;
    }else{
        LOGGER_ERROR("%s %d", strerror(status), status);
        return 0;
    }
}

int client_getattr(const char *path, struct stat *statbuf)
{
    int retval = 0;
    int i = 0;for(; i < 1; i++){
        if(STORAGE.servers[i].state == SERVER_STARTING){
            if(!restoreall(&i))
                continue;
        }
        log_start(i, "client_getattr");
        struct getattr_ans* data = (struct getattr_ans*)request->msg_data(create_message(fnc_getattr, 0, 0, path), STORAGE.servers[i].name);
        log_end(i, "client_getattr");
        if((long)data == ERRCONNECTION){
            reconnect(i--);
            continue;
        }
        memcpy(statbuf, &data->stat, sizeof(struct stat));
        retval = data->retval;
        free(data);
    }
    return retval;
}


/** Create a file node */
int client_mknod(const char *path, mode_t mode, dev_t dev)
{
    int retval = 0;
    int i = 0; for(; i < STORAGE.n_servers; i++){
        if(!STORAGE.servers[i].state)
            continue;
        if(STORAGE.servers[i].state == SERVER_STARTING){
            if(!restoreall(&i))
                continue;
        }
        log_start(i, "connector_mknod");
        retval = request->msg_status(create_mk_message(fnc_mknod, mode, dev, path), STORAGE.servers[i].name);
        log_end(i, "connector_mknod");
        if(retval == ERRCONNECTION){
            reconnect(i--);
        }
    }
    return retval;
}

/** Create a directory */
int client_mkdir(const char *path, mode_t mode)
{
    int retval = 0;
    int i = 0; for(; i < STORAGE.n_servers; i++){
        if(!STORAGE.servers[i].state)
            continue;
        if(STORAGE.servers[i].state == SERVER_STARTING){
            if(!restoreall(&i))
                continue;
        }
        log_start(i, "connector_mkdir");
        retval = request->msg_status(create_mk_message(fnc_mkdir, mode, 0, path), STORAGE.servers[i].name);
        log_end(i, "connector_mkdir");
        if(retval == ERRCONNECTION){
            reconnect(i--);
        }
    }
    return retval;
}

/** Remove a file */
int client_unlink(const char *path)
{
    int retval = 0;
    int i = 0; for(; i < STORAGE.n_servers; i++){
        if(!STORAGE.servers[i].state)
            continue;
        if(STORAGE.servers[i].state == SERVER_STARTING){
            if(!restoreall(&i))
                continue;
        }
        log_start(i, "connector_unlink");
        retval = request->msg_status(create_ext_message(fnc_unlink, 0, 0, 0, 0, path), STORAGE.servers[i].name);
        log_end(i, "connector_unlink");
        if(retval == ERRCONNECTION){
            reconnect(i--);
        }
    }
    return retval;
}

/** Remove a directory */
int client_rmdir(const char *path)
{
    int retval = 0;
    int i = 0; for(; i < STORAGE.n_servers; i++){
        if(!STORAGE.servers[i].state)
            continue;
        if(STORAGE.servers[i].state == SERVER_STARTING){
            if(!restoreall(&i))
                continue;
        }
        log_start(i, "connector_rmdir");
        retval = request->msg_status(create_ext_message(fnc_rmdir, 0, 0, 0, 0, path), STORAGE.servers[i].name);
        log_end(i, "connector_rmdir");
        if(retval == ERRCONNECTION){
            reconnect(i--);
        }
    }
    return retval;
}

/** Rename a file */
// both path and newpath are fs-relative
int client_rename(const char *path, const char *newpath)
{
    int retval = 0;
    int i = 0; for(; i < STORAGE.n_servers; i++){
        if(!STORAGE.servers[i].state)
            continue;
        if(STORAGE.servers[i].state == SERVER_STARTING){
            if(!restoreall(&i))
                continue;
        }
        log_start(i, "client_rename");
        retval = request->data_status(create_message(fnc_rename, 0, 0, path), newpath, strlen(newpath) + 1, STORAGE.servers[i].name);
        log_end(i, "client_rename");
        if(retval == ERRCONNECTION){
            reconnect(i--);
        }
    }
    return retval;
}

/** Change the size of a file */
int client_truncate(const char *path, off_t newsize)
{
    int retval = 0;
    int i = 0; for(; i < STORAGE.n_servers; i++){
        if(!STORAGE.servers[i].state)
            continue;
        if(STORAGE.servers[i].state == SERVER_STARTING){
            if(!restoreall(&i))
                continue;
        }
        log_start(i, "client_truncate");
        retval = request->msg_status(create_ext_message(fnc_truncate, 0, 0, 0, newsize, path), STORAGE.servers[i].name);
        log_end(i, "client_truncate");
        if(retval == ERRCONNECTION){
            reconnect(i--);
        }
    }
    return retval;
}

/** Change the access and/or modification times of a file */
int client_utime(const char *path, struct utimbuf *ubuf)
{
    int retval = 0;
    int i = 0; for(; i < STORAGE.n_servers; i++){
        if(!STORAGE.servers[i].state)
            continue;
        if(STORAGE.servers[i].state == SERVER_STARTING){
            if(!restoreall(&i))
                continue;
        }
        log_start(i, "client_utime");
        retval = request->data_status(create_message(fnc_utime, 0, sizeof*ubuf, path), (const char*)ubuf, sizeof*ubuf, STORAGE.servers[i].name);
        log_end(i, "client_utime");
        if(retval == ERRCONNECTION){
            reconnect(i--);
        }
    }
    return retval;
}

/** File open operation */
int client_open(const char *path, struct fuse_file_info *fi)
{
    int i = 0;
    int retval = 0;
    struct message* msgs[STORAGE.n_servers];
    fi->fh = fd_counter++;
    for(; i < STORAGE.n_servers; i++){
        msgs[i] = NULL;
        if(!STORAGE.servers[i].state)
            continue;
        if(STORAGE.servers[i].state == SERVER_STARTING){
            if(!restoreall(&i))
                continue;
        }
        log_start(i, "client_open");
        struct message* to_send = create_message(fnc_open, fi->flags, 0, path);
        struct message* to_receive = request->msg_msg(to_send, STORAGE.servers[i].name);
        long dp = to_receive->status;
        msgs[i] = to_receive;
        insert_fd_wrapper(&STORAGE.servers[i], fd_wrapper_create(fi->fh, dp));
        int old_hash = i > 0 && strcmp(msgs[i - 1]->small_data, msgs[i]->small_data);
        log_end(i, "client_open");
        if(dp < 0 || old_hash){
            if(dp == ERRCONNECTION){
                reconnect(i--);
                continue;
            }else if(dp == ERRHASH || dp == ENOENT || old_hash){
                int ind = ((STORAGE.n_servers + i - 1) % STORAGE.n_servers);
                int size = strlen(STORAGE.servers[ind].name) + 1;
                int i2 = i;
                if(dp != ERRHASH && old_hash && msgs[i - 1]->size < msgs[i]->size)
                    memswap(&i, &ind, sizeof(int));
                if(request->data_status(create_message(fnc_restore, 0, size, path), STORAGE.servers[ind].name, size, STORAGE.servers[i].name) < 0)
                    LOGGER("couldn't restore data from %s", STORAGE.servers[ind].name);
                else{
                    LOGGER("restored data from %s", STORAGE.servers[ind].name);
                    i2 = i - 1;
                }
                i = i2;
                continue;
            }
            LOGGER_ERROR("%s %d", strerror(-dp), -dp);
            retval = dp;
            break;
        }
    }
    int j = 0; for(; j <= i && i < STORAGE.n_servers; j++){
        free(msgs[i]);
    }
    return retval;
}

/** Read data from an open file */
int client_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    int i = 0;for(; i < 1; i++){
        if(STORAGE.servers[i].state == SERVER_STARTING){
            if(!restoreall(&i))
                continue;
        }
        log_start(i, "client_read");
        long fd = get_server_fd(&STORAGE.servers[i], fi->fh);
        if(fd == -1)
            return EBADF;
        char * server_buf = (char*)request->msg_data(create_ext_message(fnc_read, fd, 0, size, offset, ""), STORAGE.servers[i].name);
        log_end(i, "client_read");
        if((long)server_buf == ERRCONNECTION){
            reconnect(i--);
            continue;
        }
        strcpy(buf, server_buf);
        free(server_buf);
    }
	return size;
}

/** Write data to an open file */
int client_write(const char *path, const char *buf, size_t size, off_t offset,
	     struct fuse_file_info *fi)
{
    int res;
    int i = 0;for(; i < STORAGE.n_servers; i++){
        if(!STORAGE.servers[i].state)
            continue;
        if(STORAGE.servers[i].state == SERVER_STARTING){
            if(!restoreall(&i))
                continue;
        }
        long fd = get_server_fd(&STORAGE.servers[i], fi->fh);
        if(fd == -1)
            return EBADF;
        log_start(i, "client_write");
        res = request->data_status(
            create_ext_message(fnc_write, fd, size, size, offset, path),
            buf, size, STORAGE.servers[i].name
        );
        log_end(i, "client_write");
        if(res < 0){
            if(res == ERRCONNECTION){
                reconnect(i--);
                continue;
            }
            LOGGER_ERROR("%s %d", strerror(errno), errno);
            res = -errno;
        }
    }
    return res;
}


/** Release an open file */
int client_release(const char *path, struct fuse_file_info *fi)
{
    int res;
    int i = 0;for(; i < STORAGE.n_servers; i++){
        if(!STORAGE.servers[i].state)
            continue;
        if(STORAGE.servers[i].state == SERVER_STARTING){
            if(!restoreall(&i))
                continue;
        }
        log_start(i, "client_release");
        long fd = get_server_fd(&STORAGE.servers[i], fi->fh);
        if(fd == -1)
            return EBADF;
        res = request->msg_status(create_message(fnc_release, fd, 0, ""), STORAGE.servers[i].name);
        log_end(i, "client_release");
        if(res == ERRCONNECTION){
            reconnect(i--);
        }
        else if(res < 0)
            res = -errno;
    }
    return res;
}

/** Open directory */
int client_opendir(const char *path, struct fuse_file_info *fi)
{
    intptr_t dp = fd_counter++;
    fi->fh = dp;
    int i = 0;for(; i < STORAGE.n_servers; i++){
        if(!STORAGE.servers[i].state)
            continue;
        if(STORAGE.servers[i].state == SERVER_STARTING){
            if(!restoreall(&i))
                continue;
        }
        log_start(i, "client_opendir");
        struct message* message_to_send = create_message(fnc_opendir, 0, 0, path);
        intptr_t dp_server = request->msg_status(message_to_send, STORAGE.servers[i].name);
        insert_fd_wrapper(&STORAGE.servers[i], fd_wrapper_create(dp, dp_server));
        log_end(i, "client_opendir");
        if(dp == ERRCONNECTION){
            reconnect(i--);
        }
        else if(dp < 0){
            LOGGER_ERROR("%s %d", strerror(-dp), -dp);
            return -dp;
        }
    }
    return 0;
}

/** Read directory */
int client_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
	       struct fuse_file_info *fi)
{
    int i = 0; for(; i < 1; i++){
        if(STORAGE.servers[i].state == SERVER_STARTING){
            if(!restoreall(&i))
                continue;
        }
        log_start(i, "client_readdir");
        long fd = get_server_fd(&STORAGE.servers[i], fi->fh);
        if(fd == -1)
            return EBADF;
        char* entries = (char*)request->msg_data(create_message(fnc_readdir, fd, 0, ""), STORAGE.servers[i].name);
        log_end(i, "client_readdir");
        if((long)entries == ERRCONNECTION){
            reconnect(i--);
            continue;
        }
        int* offsets = (int*)entries;
        int count = offsets[0];
        offsets++;
        if(count < 0){
            console.logger(STORAGE.diskname, STORAGE.servers[i].name, "couldn't read from server");
            return -ENOMEM;
        }
        int j = 0; for(; j < count; j++){
            if (filler(buf, (entries + offsets[j]), NULL, 0) != 0) {
                return -ENOMEM;
            }
        }
        free(entries);
    }
    return 0;
}

/** Release directory */
int client_releasedir(const char *path, struct fuse_file_info *fi)
{
    int res;
    int i = 0; for(; i < STORAGE.n_servers; i++){
        if(!STORAGE.servers[i].state)
            continue;
        if(STORAGE.servers[i].state == SERVER_STARTING){
            if(!restoreall(&i))
                continue;
        }
        log_start(i, "client_releasedir");
        intptr_t fd = get_server_fd(&STORAGE.servers[i], fi->fh);
        if(fd == -1)
            return EBADF;
        res = request->msg_status(create_message(fnc_releasedir, fd, 0, ""), STORAGE.servers[i].name);
        log_end(i, "client_releasedir");
        if(res == ERRCONNECTION){
            reconnect(i--);
        }
    }
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
        console.log("specify config file");
        return -1;
    }
    Protocol p = new_protocol();
    request = &p->request;
    config_init(&config, argv[1], request);
    int res = 0;
    int i = 0;for(; i < config.n_storages; i++){
        int pid = 0;
        pid = fork();
        if(pid == 0){
            char *argv_i[3];
            argv_i[0] = argv[0];
            argv_i[1] = config.storages[i].mountpoint;
            argv_i[2] = strdup("-s");
            console.log("mounting %s", config.storages[i].mountpoint);
            res = fuse_main(3, argv_i, &client_operations, &config.storages[i]);
            break;
        }
    }
    free(p);
	return res;
}