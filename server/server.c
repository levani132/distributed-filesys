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

#include "server_methods.h"
#include "../logger.h"
#include "../message.h"
#include "../hasher.h"
#include "../protocol.h"

#define UNUSED __attribute__((unused))

char ip[16];
int port;
char root_path[PATH_MAX];
int listen_sock;
FileManager file_manager;
Protocol protocol;

void cleanup(UNUSED int sig){
    console.log("cleaning up");
    close(listen_sock);
}

int handle_message(int sock, struct message* mr){
    int retval;
    console.log("server is calling %s", function_name[mr->function_id]);
    switch(mr->function_id){
        case fnc_ping: retval = protocol->send_status(sock, 1); break;
        case fnc_opendir: retval = protocol->send_status(sock, file_manager->opendir(mr->small_data)); break;
        case fnc_open: retval = protocol->send_message(sock, file_manager->open(mr->small_data, mr->status)); break;
        case fnc_truncate: retval = protocol->send_status(sock, file_manager->truncate(mr->small_data, mr->offset)); break;
        case fnc_release: retval = protocol->send_status(sock, close((int)mr->status)); break;
        case fnc_releasedir: retval = protocol->send_status(sock, closedir((DIR *)(intptr_t)mr->status)); break;
        case fnc_readdir: {
            char * res = file_manager->readdir(mr->status, mr->small_data);
            retval = protocol->send_data(sock, res, ((int*)res)[((int*)res)[0]] + strlen(res + ((int*)res)[((int*)res)[0]]) + 1);
            break;
        }
        case fnc_getattr: retval = protocol->send_data(sock, file_manager->getattr(mr->small_data), sizeof(struct getattr_ans)); break;
        case fnc_read: retval = protocol->send_data(sock,file_manager->read(mr->status, mr->size, mr->offset), mr->size + 1); break;
        case fnc_write: retval = protocol->send_status(sock, file_manager->write(mr->small_data, mr->status, protocol->get_data(sock, mr->wait_for_message), mr->size, mr->offset)); break;
        case fnc_utime: retval = protocol->send_status(sock, file_manager->utime(mr->small_data, protocol->get_data(sock, mr->wait_for_message))); break;
        case fnc_mknod: retval = protocol->send_status(sock, file_manager->mknod(mr->small_data, mr->mode, mr->dev)); break;
        case fnc_mkdir: retval = protocol->send_status(sock, file_manager->mkdir(mr->small_data, mr->mode)); break;
        case fnc_rename: retval = protocol->send_status(sock, file_manager->rename(mr->small_data, protocol->get_data(sock, mr->wait_for_message))); break;
        case fnc_unlink: retval = protocol->send_status(sock, file_manager->unlink(mr->small_data)); break;
        case fnc_rmdir: retval = protocol->send_status(sock, file_manager->rmdir(mr->small_data)); break;
        case fnc_restore: retval = protocol->send_status(sock, file_manager->restore(mr->small_data, protocol->get_data(sock, mr->wait_for_message))); break;
        case fnc_readall: {
            char* data = file_manager->readall(mr->small_data);
            retval = protocol->send_data(sock, data, ((int*)data)[0] + sizeof(int));
            break;
        }
        case fnc_restoreall: retval = protocol->send_status(sock, file_manager->restoreall("/", mr->small_data, 1)); break;
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
    protocol = new_protocol();
    file_manager = new_server(root_path, protocol->request.msg_data);
    listen_sock = protocol->open_server(ip, port);
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
        while ((message_received = protocol->get_message(sock))) {
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