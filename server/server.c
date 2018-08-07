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
#include "server_connector.h"

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

int server_open (const char * path, int flags){
    int fd;
    char fullpath[PATH_MAX];
    
    get_fullpath(fullpath, path);
    fd = open(fullpath, flags);
    if (fd < 0){
		fd = -errno;
    }
    return fd;
}

char* server_readdir (intptr_t dp){
    struct dirent *de;
    int count = 0;
    while ((de = readdir((DIR *) dp)) != NULL) {
    	count++;
    }
    if(count == 0){
        return NULL;
    }
    rewinddir((DIR*)dp);
    int offset = (count + 1) * sizeof(int);
    char * data = malloc(offset);
    ((int*)data)[0] = count;
    int i = 1; for(; i <= count; i++){
        de = readdir((DIR *) dp);
        int new_size = offset + strlen(de->d_name) + 1;
        data = realloc(data, new_size);
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

int server_utime(const char *__file, const struct utimbuf *__file_times){
    char fullpath[256];
    get_fullpath(fullpath, __file);
    int res = utime(fullpath, __file_times);
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

int server_rename(const char *path, const char *newpath){
    char fullpath[PATH_MAX];
    char new_fullpath[PATH_MAX];
    get_fullpath(fullpath, path);
    get_fullpath(new_fullpath, newpath);
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

int handle_message(int sock, struct message* mr){
    int retval;
    loggerf("server is calling %s", function_name[mr->function_id]);
    if(mr->function_id == fnc_opendir){
        retval = connector_send_status(sock, server_opendir(mr->small_data));
    }else if(mr->function_id == fnc_open){
        retval = connector_send_status(sock, server_open(mr->small_data, mr->status));
    }else if(mr->function_id == fnc_truncate){
        retval = connector_send_status(sock, server_truncate(mr->small_data, mr->offset));
    }else if(mr->function_id == fnc_release){
        retval = connector_send_status(sock, close((int)mr->status));
    }else if(mr->function_id == fnc_releasedir){
        retval = connector_send_status(sock, closedir((DIR *)(uintptr_t)mr->status));
    }else if(mr->function_id == fnc_readdir){
        char * res = server_readdir(mr->status);
        int byte_count = ((int*)res)[((int*)res)[0]] + strlen(res + ((int*)res)[((int*)res)[0]]) + 1;
        retval = connector_send_data(sock, res, byte_count);
        free(res);
    }else if(mr->function_id == fnc_getattr){
        struct getattr_ans * res = server_getattr(mr->small_data);
        retval = connector_send_data(sock, res, sizeof(struct getattr_ans));
        free(res);
    }else if(mr->function_id == fnc_read){
        char* res = server_read(mr->status, mr->size, mr->offset);
        retval = connector_send_data(sock, res, mr->size + 1);
        free(res);
    }else if(mr->function_id == fnc_write){
        char* data = connector_get_data(sock, mr->wait_for_message);
        int res = pwrite(mr->status, data, mr->size, mr->offset);
        free(data);
        retval = connector_send_status(sock, res);
    }else if(mr->function_id == fnc_utime){
        struct utimbuf* data = connector_get_data(sock, mr->wait_for_message);
        int res = server_utime(mr->small_data, data);
        free(data);
        retval = connector_send_status(sock, res);
    }else if(mr->function_id == fnc_mknod){
        retval = connector_send_status(sock, server_mknod(mr->small_data, mr->mode, mr->dev));
    }else if(mr->function_id == fnc_mkdir){
        retval = connector_send_status(sock, server_mkdir(mr->small_data, mr->mode));
    }else if(mr->function_id == fnc_rename){
        char* data = connector_get_data(sock, mr->wait_for_message);
        int res = server_rename(mr->small_data, data);
        free(data);
        retval = connector_send_status(sock, res);
    }else if(mr->function_id == fnc_unlink){
        retval = connector_send_status(sock, server_unlink(mr->small_data));
    }else if(mr->function_id == fnc_rmdir){
        retval = connector_send_status(sock, server_rmdir(mr->small_data));
    }
    return retval;
}

int main(int argc, char* argv[]) {
    if(argc < 4){
        loggerf("specify at least three arguments");
        return -1;
    }
    strcpy(ip, argv[1]);
    ip[strlen(argv[1])] = '\0';
    sscanf(argv[2], "%d", &port);
    strcpy(root_path, argv[3]);
    root_path[strlen(argv[3])] = '\0';
    
    listen_sock = connector_open_server_on(ip, port);
    signal(SIGINT, cleanup);
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