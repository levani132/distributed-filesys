#include <errno.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <linux/limits.h>

#include "../logger.h"
#include "server_methods.h"
#include "../message.h"
#include "server_hasher.h"

FileManager this;
void* send_and_recv_data(struct message* message_to_send, const char* server);

void get_fullpath(char* fullpath, const char* path){
    strcpy(fullpath, this->root_path);
    strcpy(fullpath + strlen(this->root_path), path);
    fullpath[strlen(this->root_path) + strlen(path)] = '\0';
}

intptr_t server_opendir (const char * path){
    DIR *dp;
    char fullpath[PATH_MAX];
    get_fullpath(fullpath, path);
    dp = opendir(fullpath);
    if (dp == NULL){
		dp = (DIR*)-(long)errno;
    }
    return (intptr_t) dp;
}

struct message* server_open (const char * path, int flags){
    int fd;
    char fullpath[PATH_MAX];
    
    get_fullpath(fullpath, path);
    fd = open(fullpath, flags);
    if (fd < 0){
		fd = -errno;
    }
    char hash[HASH_SIZE];
    char old_hash[HASH_SIZE];
    hasher(fullpath, hash);
    hasher_get_for(fullpath, old_hash);
    int not_same = strcmp(hash, old_hash);
    struct stat tmpstat;
    lstat(fullpath, &tmpstat);
    return create_ext_message(0, not_same ? ERRHASH : (long)fd, 0, tmpstat.st_mtime, 0, hash);
}

char* server_readdir (intptr_t dp, char* path){
    int need = dp == -1;
    if(need){
        char fullpath[PATH_MAX];
        get_fullpath(fullpath, path);
        dp = (intptr_t)opendir(fullpath);
    }
    struct dirent *de;
    int count = 0;
    while ((de = readdir((DIR *) dp)) != NULL) {
    	count++;
    }
    if(count == 0){
        LOGGER_ERROR("%s %d", strerror(errno), errno);
        char * data = malloc(sizeof(int) + sizeof(char));
        ((int*)data)[0] = 0;
        data[4] = '\0';
        return data;
    }
    rewinddir((DIR*)dp);
    int offset = (count + 1) * sizeof(int);
    char * data = malloc(offset);
    if(!data){
        LOGGER_ERROR("%s %d", strerror(errno), errno);
    }
    ((int*)data)[0] = count;
    int i = 1; for(; i <= count; i++){
        de = readdir((DIR *) dp);
        int new_size = offset + strlen(de->d_name) + 1;
        data = realloc(data, new_size);
        if(!data){
            LOGGER_ERROR("%s %d", strerror(errno), errno);
        }
        ((int*)data)[i] = offset;
        strcpy(data + offset, de->d_name);
        offset = new_size;
    }
    if(need){
        closedir((DIR *)dp);
    }
    return data;
}

struct getattr_ans* server_getattr(const char * path) {
    char fullpath[PATH_MAX];
    struct getattr_ans* ans = malloc(sizeof*ans);
    memset(ans, 0, sizeof*ans);
    get_fullpath(fullpath, path);
    ans->retval = lstat(fullpath, &ans->stat);
    ans->retval = ans->retval < 0 ? -errno : ans->retval;
    return ans;
}

int server_truncate(char* path, off_t size){
    char fullpath[256];
    get_fullpath(fullpath, path);
    int res = truncate(fullpath, size);
    return res < 0 ? -errno : res;
}

int server_utime(const char *__file, struct utimbuf *__file_times){
    char fullpath[256];
    get_fullpath(fullpath, __file);
    int res = utime(fullpath, __file_times);
    free(__file_times);
    return res < 0 ? -errno : res;
}

int server_mknod(const char *path, mode_t mode, dev_t dev){
    char fullpath[PATH_MAX];
    get_fullpath(fullpath, path);
    int retval = open(fullpath, O_CREAT | O_EXCL | O_WRONLY, mode);
    if (retval >= 0){
        retval = close(retval);
    }
	if(retval < 0){
        retval = -errno;
    }
    hasher_save_for(fullpath);
    return retval;
}

int server_mkdir(const char *path, mode_t mode)
{
    char fullpath[PATH_MAX];
    get_fullpath(fullpath, path);
    if(mkdir(fullpath, mode) < 0)
    	return -errno;
    return 0;
}

int server_rename(const char *path, char *newpath){
    char fullpath[PATH_MAX];
    char new_fullpath[PATH_MAX];
    get_fullpath(fullpath, path);
    get_fullpath(new_fullpath, newpath);
    free(newpath);
    if(rename(fullpath, new_fullpath) < 0)
    	return -errno;
    return 0;
}

int server_unlink(const char *path){
    char fullpath[PATH_MAX];
    get_fullpath(fullpath, path);

    int retval = unlink(fullpath);
    if(retval < 0)
    	retval = -errno;

    return retval;
}

int server_rmdir(const char *path){
    char fullpath[PATH_MAX];
    get_fullpath(fullpath, path);

    int retval = rmdir(fullpath);
    if(retval < 0)
    	retval = -errno;

    return retval;
}

int server_write(const char* path, int fd, void* data, size_t size, off_t offset){
    int retval = 0;
    char fullpath[PATH_MAX];
    get_fullpath(fullpath, path);
    console.log(data);
    if((retval = pwrite(fd, data, size, offset)) < 0){
        return -errno;
    }
    free(data);
    if(hasher_save_for(fullpath) < 0){
        return -errno;
    }
    return retval;
}

char* server_read(int fd, size_t size, off_t offset){
    char* buf = malloc((size + 1) * sizeof(char));
    pread(fd, buf, size, offset);
    console.log(buf);
    return buf;
}

int mkdir_recursive(char* fullpath){
    if(!strcmp(fullpath, this->root_path)){
        return -errno;
    }
    char path_cpy[PATH_MAX];
    int i = strlen(fullpath) - 1;
    while(fullpath[i] != '/') fullpath[i--] = '\0';
    fullpath[i] = '\0';
    strcpy(path_cpy, fullpath);
    if(mkdir(fullpath, 0777) >= 0){
        return 0;
    }else if(mkdir_recursive(path_cpy) == 0 && mkdir(fullpath, 0777) >= 0){
        return 0;
    }else{
        return -errno;
    }
}

int server_restore(const char* path, const char* server){
    int retval;
    char * data_to_restore = send_and_recv_data(create_message(fnc_readall, 0, 0, path), server);
    char fullpath[PATH_MAX];
    get_fullpath(fullpath, path);
    int fd = open(fullpath, O_WRONLY | O_CREAT | O_TRUNC, 0777);
    if(fd < 0){
        if((retval = mkdir_recursive(fullpath)) < 0){
            LOGGER_ERROR("%s %d", strerror(-retval), -retval);
            return retval;
        }
        get_fullpath(fullpath, path);
        fd = open(fullpath, O_WRONLY | O_CREAT | O_TRUNC, 0777);
        if(fd < 0){
            return -errno;
        }
    }
    if(write(fd, data_to_restore + sizeof(int), ((int*)data_to_restore)[0]) < 0){
        return -errno;
    }
    close(fd);
    hasher_save_for(fullpath);
    return 0;
}

char* server_readall(const char* path){
    char fullpath[PATH_MAX];
    char *file_content = malloc(sizeof(int));
    ((int*)file_content)[0] = 0;
    get_fullpath(fullpath, path);
    FILE* file = fopen(fullpath, "r");
    if(file == NULL){
        LOGGER_ERROR("%s %d", strerror(errno), errno);
        return file_content;
    }
    if(fseek(file, 0, SEEK_END) < 0){
        LOGGER_ERROR("%s %d", strerror(errno), errno);
    }
    long fsize;
    if((fsize = ftell(file)) < 0){
        LOGGER_ERROR("%s %d", strerror(errno), errno);
    }
    rewind(file);
    file_content = realloc(file_content, fsize + sizeof(int));
    if(file_content == NULL){
        LOGGER_ERROR("%s %d", strerror(errno), errno);
    }
    ((int*)file_content)[0] = fsize;
    if(fread(file_content + sizeof(int), fsize, 1, file) < 0){
        LOGGER_ERROR("%s %d", strerror(errno), errno);
    }
    //fclose(file);
    return file_content;
}

int empty_directory(const char *path){
    DIR *dir = opendir(path);
    size_t path_len = strlen(path);
    int r = -1;
    if (dir){
        struct dirent *dirent;
        r = 0;
        while (!r && (dirent = readdir(dir))){
            int r2 = -1;
            if (!strcmp(dirent->d_name, ".") || !strcmp(dirent->d_name, ".."))
                continue;
            size_t len = path_len + strlen(dirent->d_name) + 2;
            char *buf = malloc(len);
            if (buf){
                struct stat statbuf;
                snprintf(buf, len, "%s/%s", path, dirent->d_name);
                if (!stat(buf, &statbuf)){
                    if (S_ISDIR(statbuf.st_mode)){
                        r2 = empty_directory(buf);
                        r2 = rmdir(buf);
                    }else
                        r2 = unlink(buf);
                }
                free(buf);
            }
            r = r2;
        }
        closedir(dir);
    }
    return r;
}

int server_restoreall(const char* path, const char* server, int first){
    char fullpath[PATH_MAX];
    get_fullpath(fullpath, path);
    if(first && empty_directory(fullpath)){
        LOGGER_ERROR("%s %d", strerror(errno), errno);
    }
    int retval = 0;
    char* entries = (char*)send_and_recv_data(create_message(fnc_readdir, -1, 0, path), server);
    if((long)entries == ERRCONNECTION){
        free(entries);
        return -1;
    }
    int* offsets = (int*)entries;
    int count = offsets[0];
    offsets++;
    if(count < 0){
        LOGGER_ERROR("couldn't read dir from server %s", server);
        return -ENOMEM;
    }
    int j = 0; for(; j < count; j++){
        char child[PATH_MAX];
        char fullchild[PATH_MAX];
        strcpy(child, path);
        if(!strcmp((entries + offsets[j]), ".") || !strcmp((entries + offsets[j]), ".."))
            continue;
        strcat(child, (entries + offsets[j]));
        get_fullpath(fullchild, child);
        struct getattr_ans* ans = (struct getattr_ans*)send_and_recv_data(create_message(fnc_getattr, 0, 0, child), server);
        if(S_ISREG(ans->stat.st_mode)){
            retval = this->restore(child, server);
        }else if(S_ISDIR(ans->stat.st_mode)){
            strcat(child, "/");
            if((retval = this->restoreall(child, server, 0)) < 0){
                LOGGER_ERROR("%s %d", strerror(-retval), -retval);
            }
        }
        free(ans);
    }
    free(entries);
    return retval;
}

FileManager new_server(char* root_path, void* (*req_msg_data)(struct message* message_to_send, const char* server)){
    this = malloc(sizeof(struct FileManager));
    struct FileManager s = {
        .opendir = server_opendir,
        .open = server_open,
        .readdir = server_readdir,
        .getattr = server_getattr,
        .read = server_read,
        .truncate = server_truncate,
        .utime = server_utime,
        .mknod = server_mknod,
        .mkdir = server_mkdir,
        .rename = server_rename,
        .unlink = server_unlink,
        .rmdir = server_rmdir,
        .write = server_write,
        .restore = server_restore,
        .readall = server_readall,
        .restoreall = server_restoreall,
    };
    strcpy(s.root_path, root_path);
    memcpy(this, &s, sizeof(struct FileManager));
    return this;
}