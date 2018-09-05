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
#include "server_methods.h"
#include "../client/client_connector.h"

#define UNUSED __attribute__((unused))

char ip[16];
int port;
char root_path[PATH_MAX];
int listen_sock;
Server server;

void cleanup(UNUSED int sig){
    console.log("cleaning up");
    close(listen_sock);
}

int handle_message(int sock, struct message* mr){
    int retval;
    console.log("server is calling %s", function_name[mr->function_id]);
    if(mr->function_id == fnc_ping){
        retval = connector_send_status(sock, 1);
    }else if(mr->function_id == fnc_opendir){
        retval = connector_send_status(sock,
            server->opendir(mr->small_data)
        );
    }else if(mr->function_id == fnc_open){
        retval = connector_send_message(sock,
            server->open(mr->small_data, mr->status)
        );
    }else if(mr->function_id == fnc_truncate){
        retval = connector_send_status(sock,
            server->truncate(mr->small_data, mr->offset)
        );
    }else if(mr->function_id == fnc_release){
        retval = connector_send_status(sock,
            close((int)mr->status)
        );
    }else if(mr->function_id == fnc_releasedir){
        retval = connector_send_status(sock,
            closedir((DIR *)(intptr_t)mr->status)
        );
    }else if(mr->function_id == fnc_readdir){
        char * res = server->readdir(mr->status, mr->small_data);
        int byte_count = ((int*)res)[((int*)res)[0]] + strlen(res + ((int*)res)[((int*)res)[0]]) + 1;
        retval = connector_send_data(sock, res, byte_count);
    }else if(mr->function_id == fnc_getattr){
        retval = connector_send_data(sock, 
            server->getattr(mr->small_data), sizeof(struct getattr_ans)
        );
    }else if(mr->function_id == fnc_read){
        retval = connector_send_data(sock,
            server->read(mr->status, mr->size, mr->offset), mr->size + 1
        );
    }else if(mr->function_id == fnc_write){
        retval = connector_send_status(sock,
            server->write(mr->small_data, mr->status,
                connector_get_data(sock, mr->wait_for_message), mr->size, mr->offset
            )
        );
    }else if(mr->function_id == fnc_utime){
        retval = connector_send_status(sock,
            server->utime(mr->small_data,
                connector_get_data(sock, mr->wait_for_message)
            )
        );
    }else if(mr->function_id == fnc_mknod){
        retval = connector_send_status(sock,
            server->mknod(mr->small_data, mr->mode, mr->dev)
        );
    }else if(mr->function_id == fnc_mkdir){
        retval = connector_send_status(sock,
            server->mkdir(mr->small_data, mr->mode)
        );
    }else if(mr->function_id == fnc_rename){
        retval = connector_send_status(sock,
            server->rename(mr->small_data,
                connector_get_data(sock, mr->wait_for_message)
            )
        );
    }else if(mr->function_id == fnc_unlink){
        retval = connector_send_status(sock,
            server->unlink(mr->small_data)
        );
    }else if(mr->function_id == fnc_rmdir){
        retval = connector_send_status(sock,
            server->rmdir(mr->small_data)
        );
    }else if(mr->function_id == fnc_restore){
        retval = connector_send_status(sock, 
            server->restore(mr->small_data, 
                connector_get_data(sock, mr->wait_for_message)
            )
        );
    }else if(mr->function_id == fnc_readall){
        char* data = server->readall(mr->small_data);
        retval = connector_send_data(sock, data, ((int*)data)[0] + sizeof(int));
    }else if(mr->function_id == fnc_restoreall){
        retval = connector_send_status(sock,
            server->restoreall("/", mr->small_data, 1)
        );
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
        console.log("specify at least three arguments");
        return -1;
    }
    readargs(argv);
    server = new_server(root_path);
    listen_sock = connector_open_server_on(ip, port);
	struct sockaddr_in client_address;
	socklen_t client_address_len = 0;
	while (1) {
		console.log("--------------------------");
		console.log("waiting for new connection");
		int sock;
		if ((sock = accept(listen_sock, (struct sockaddr *)&client_address, &client_address_len)) < 0) {
			console.log("could not open a socket to accept data");
            console.log("%s %d", strerror(errno), errno);
			return 1;
		}
		console.log("client connected with ip address: %s", inet_ntoa(client_address.sin_addr));
        struct message* message_received;;
        while ((message_received = connector_get_message(sock))) {
            int status = handle_message(sock, message_received);
            if (status < 0) {
                console.log("%s %d", strerror(-status), -status);
            }
            free(message_received);
        }
		close(sock);
        console.log("sock closed");
	}
	close(listen_sock);
    return 0;
}