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

#include "../logger.h"
#include "../models/getattr_ans.h"
#include "../message.h"
#include "server_connector.h"

char ip[16];
int port;
char root_path[256];

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
    struct getattr_ans* ans = malloc(sizeof(*ans));
    get_fullpath(fullpath, path);
    ans->retval = lstat(fullpath, &ans->stat);
    return ans;
}

char* server_read(int fd, size_t size, off_t offset){
    char* buf = malloc((size + 1) * sizeof(char));
    pread(fd, buf, size, offset);
    return buf;
}

int handle_message(int sock, struct message* mr){
    int n_sent;
    loggerf("server is calling %s", function_name[mr->function_id]);
    if(mr->function_id == fnc_opendir){
        struct message* res = create_message(0, server_opendir(mr->small_data), 0, "");
        if((n_sent = connector_send_message(sock, res)) < 0){
            free(res);
            return n_sent;
        }
        free(res);
    }else if(mr->function_id == fnc_readdir){
        char * res = server_readdir(mr->status);
        int byte_count = ((int*)res)[((int*)res)[0]] + strlen(res + ((int*)res)[((int*)res)[0]]) + 1;
        if((n_sent = connector_send_data(sock, res, byte_count)) < 0) {
            free(res);
            return n_sent;
        }
        free(res);
    }else if(mr->function_id == fnc_getattr){
        struct getattr_ans * res = server_getattr(mr->small_data);
        if((n_sent = connector_send_data(sock, res, sizeof(struct getattr_ans))) < 0) {
            free(res);
            return n_sent;
        }
        free(res);
    }else if(mr->function_id == fnc_open){
        struct message* res = create_message(0, server_open(mr->small_data, mr->status), 0, "");
        if((n_sent = connector_send_message(sock, res)) < 0){
            free(res);
            return n_sent;
        }
        free(res);
    }else if(mr->function_id == fnc_read){
        char* res = server_read(mr->status, mr->size, mr->offset);
        if((n_sent = connector_send_data(sock, res, mr->size + 1)) < 0){
            free(res);
            return n_sent;
        }
        free(res);
    }else if(mr->function_id == fnc_write){
        char* data = connector_get_data(sock, mr->wait_for_message);
        int res = pwrite(mr->status, data, mr->size, mr->offset);
        if(res < 0){
            res = -errno;
        }
        free(data);
        struct message* to_send = create_ext_message(0, res, 0, 0, 0, "");
        if((n_sent = connector_send_message(sock, to_send)) < 0){
            free(to_send);
            return n_sent;
        }
        free(to_send);
    }else if(mr->function_id == fnc_utime){
        struct utimbuf* data = (struct utimbuf*)connector_get_data(sock, mr->wait_for_message);
        char fullpath[256];
        get_fullpath(fullpath, mr->small_data);
        int res = utime(fullpath, data);
        if(res < 0){
            res = -errno;
        }
        struct message* to_send = create_message(0, res, 0, "");
        if((n_sent = connector_send_message(sock, to_send)) < 0){
            free(to_send);
            return n_sent;
        }
        free(to_send);
    }else if(mr->function_id == fnc_truncate){
        char fullpath[256];
        get_fullpath(fullpath, mr->small_data);
        int res = truncate(fullpath, mr->offset);
        if(res < 0){
            res = -errno;
        }
        struct message* to_send = create_message(0, res, 0, "");
        if((n_sent = connector_send_message(sock, to_send)) < 0){
            free(to_send);
            return n_sent;
        }
        free(to_send);
    }else if(mr->function_id == fnc_release){
        struct message* to_send = malloc(sizeof*to_send);
        to_send->status = close(mr->status);
        if((n_sent = connector_send_message(sock, to_send)) < 0){
            free(to_send);
            return n_sent;
        }
        free(to_send);
    }else if(mr->function_id == fnc_releasedir){
        struct message* to_send = malloc(sizeof*to_send);
        to_send->status = closedir((DIR *)(uintptr_t)mr->status);
        if((n_sent = connector_send_message(sock, to_send)) < 0){
            free(to_send);
            return n_sent;
        }
        free(to_send);
    }
    return 0;
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
    
    int listen_sock = connector_open_server_on(ip, port);
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