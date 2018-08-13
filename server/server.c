#include <fuse.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <dirent.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <linux/limits.h>
#include <signal.h>

#include "../logger.h"
#include "../message.h"
#include "../hasher.h"
#include "server_connector.h"
#include "../client/client_connector.h"

#define UNUSED __attribute__((unused))

char ip[16];
int port;
char root_path[256];
int listen_sock;

void cleanup(UNUSED int sig){
    loggerf("cleaning up");
    close(listen_sock);
}

void get_fullpath(char* fullpath, const char* path){
    strcpy(fullpath, root_path);
    strcpy(fullpath + strlen(root_path), path);
    fullpath[strlen(root_path) + strlen(path)] = '\0';
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
    loggerf("fd: %ld", fd);
    struct stat tmpstat;
    lstat(fullpath, &tmpstat);
    return create_ext_message(0, not_same ? ERRHASH : (long)fd, 0, tmpstat.st_mtime, 0, hash);
}

char* server_readdir (intptr_t dp){
    struct dirent *de;
    int count = 0;
    while ((de = readdir((DIR *) dp)) != NULL) {
    	count++;
    }
    if(count == 0){
        LOGGER_ERROR("%s %d", strerror(errno), errno);
        return NULL;
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

char* server_read(int fd, size_t size, off_t offset){
    char* buf = malloc((size + 1) * sizeof(char));
    pread(fd, buf, size, offset);
    return buf;
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
    loggerf("writing %s %ld %s %d %d", fullpath, fd, data, size, offset);
    if((retval = pwrite(fd, data, size, offset)) < 0){
        return -errno;
    }
    free(data);
    if(hasher_save_for(fullpath) < 0){
        return -errno;
    }
    return retval;
}

int mkdir_recursive(char* fullpath){
    if(!strcmp(fullpath, root_path)){
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
    loggerf("starting restoring from %s", server);
    char * data_to_restore = send_and_recv_data(create_message(fnc_readall, 0, 0, path), server);
    loggerf("%s data_to_restore: %s", path, data_to_restore + sizeof(int));
    char fullpath[PATH_MAX];
    get_fullpath(fullpath, path);
    int fd = open(fullpath, O_WRONLY | O_CREAT | O_TRUNC, 0777);
    if(fd < 0){
        if((retval = mkdir_recursive(fullpath)) < 0){
            LOGGER_ERROR("%s %d", strerror(-retval), -retval);
            return retval;
        }
        get_fullpath(fullpath, path);
        fd = open(fullpath, O_WRONLY | O_CREAT | O_TRUNC);
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
    loggerf("%s", fullpath);
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
    loggerf("fsize: %d", fsize);
    file_content = realloc(file_content, fsize + sizeof(int));
    if(file_content == NULL){
        LOGGER_ERROR("%s %d", strerror(errno), errno);
    }
    ((int*)file_content)[0] = fsize;
    if(fread(file_content + sizeof(int), fsize, 1, file) < 0){
        LOGGER_ERROR("%s %d", strerror(errno), errno);
    }
    fclose(file);
    loggerf("returning %s", file_content + sizeof(int));
    return file_content;
}

int handle_message(int sock, struct message* mr){
    int retval;
    loggerf("server is calling %s", function_name[mr->function_id]);
    if(mr->function_id == fnc_ping){
        retval = connector_send_status(sock, 1);
    }else if(mr->function_id == fnc_opendir){
        retval = connector_send_status(sock,
            server_opendir(mr->small_data)
        );
    }else if(mr->function_id == fnc_open){
        retval = connector_send_message(sock,
            server_open(mr->small_data, mr->status)
        );
    }else if(mr->function_id == fnc_truncate){
        retval = connector_send_status(sock,
            server_truncate(mr->small_data, mr->offset)
        );
    }else if(mr->function_id == fnc_release){
        retval = connector_send_status(sock,
            close((int)mr->status)
        );
    }else if(mr->function_id == fnc_releasedir){
        retval = connector_send_status(sock,
            closedir((DIR *)(uintptr_t)mr->status)
        );
    }else if(mr->function_id == fnc_readdir){
        char * res = server_readdir(mr->status);
        int byte_count = ((int*)res)[((int*)res)[0]] + strlen(res + ((int*)res)[((int*)res)[0]]) + 1;
        retval = connector_send_data(sock, res, byte_count);
    }else if(mr->function_id == fnc_getattr){
        retval = connector_send_data(sock, 
            server_getattr(mr->small_data), sizeof(struct getattr_ans)
        );
    }else if(mr->function_id == fnc_read){
        retval = connector_send_data(sock,
            server_read(mr->status, mr->size, mr->offset), mr->size + 1
        );
    }else if(mr->function_id == fnc_write){
        retval = connector_send_status(sock,
            server_write(mr->small_data, mr->status,
                connector_get_data(sock, mr->wait_for_message), mr->size, mr->offset
            )
        );
    }else if(mr->function_id == fnc_utime){
        retval = connector_send_status(sock,
            server_utime(mr->small_data,
                connector_get_data(sock, mr->wait_for_message)
            )
        );
    }else if(mr->function_id == fnc_mknod){
        retval = connector_send_status(sock,
            server_mknod(mr->small_data, mr->mode, mr->dev)
        );
    }else if(mr->function_id == fnc_mkdir){
        retval = connector_send_status(sock,
            server_mkdir(mr->small_data, mr->mode)
        );
    }else if(mr->function_id == fnc_rename){
        retval = connector_send_status(sock,
            server_rename(mr->small_data,
                connector_get_data(sock, mr->wait_for_message)
            )
        );
    }else if(mr->function_id == fnc_unlink){
        retval = connector_send_status(sock,
            server_unlink(mr->small_data)
        );
    }else if(mr->function_id == fnc_rmdir){
        retval = connector_send_status(sock,
            server_rmdir(mr->small_data)
        );
    }else if(mr->function_id == fnc_restore){
        retval = connector_send_status(sock, 
            server_restore(mr->small_data, 
                connector_get_data(sock, mr->wait_for_message)
            )
        );
    }else if(mr->function_id == fnc_readall){
        char* data = server_readall(mr->small_data);
        retval = connector_send_data(sock, data, ((int*)data)[0] + sizeof(int));
    }
    return retval;
}

int readargs(char* argv[]){
    strcpy(ip, argv[1]);
    ip[strlen(argv[1])] = '\0';
    sscanf(argv[2], "%d", &port);
    strcpy(root_path, argv[3]);
    root_path[strlen(argv[3])] = '\0';
    return 0;
}

int main(int argc, char* argv[]) {
    if(argc < 4){
        loggerf("specify at least three arguments");
        return -1;
    }
    readargs(argv);
    listen_sock = connector_open_server_on(ip, port);
	struct sockaddr_in client_address;
	socklen_t client_address_len = 0;
	while (1) {
		loggerf("--------------------------");
		loggerf("waiting for new connection");
		int sock;
		if ((sock = accept(listen_sock, (struct sockaddr *)&client_address, &client_address_len)) < 0) {
			loggerf("could not open a socket to accept data");
            loggerf("%s %d", strerror(errno), errno);
			return 1;
		}
		loggerf("client connected with ip address: %s", inet_ntoa(client_address.sin_addr));
        struct message* message_received;;
        while ((message_received = connector_get_message(sock))) {
            int status = handle_message(sock, message_received);
            if(status < 0){
                loggerf("%s %d", strerror(-status), -status);
            }
            free(message_received);
        }
        free(message_received);
		close(sock);
        loggerf("sock closed");
	}
	close(listen_sock);
    return 0;
}