#include <arpa/inet.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>

#include "../logger.h"
#include "../message.h"
#include "../models/getattr_ans.h"

int index_of(const char * source, const char c){
    const char* ptr = strchr(source, c);
    int index = -1;
    if(ptr) {
        index = ptr - source;
    }
    return index;
}

int connect_to(const char * server){
	char server_name[16];
	int server_port = 0;
    sscanf(server, "%[^:]:%d", server_name, &server_port);

	struct sockaddr_in server_address;
	memset(&server_address, 0, sizeof(server_address));
	server_address.sin_family = AF_INET;
	inet_pton(AF_INET, server_name, &server_address.sin_addr);
	server_address.sin_port = htons(server_port);
    
    int sock;
    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        loggerf("could not create socket");
    }
    if (connect(sock, (struct sockaddr*)&server_address,
            sizeof(server_address)) < 0) {
        loggerf("could not connect to server");
        loggerf(strerror(errno));
    }
    return sock;
}

struct message* send_and_recv_message(struct message* message_to_send, const char* server){
    struct message* message_to_receive = malloc(sizeof(struct message));
    int sock = connect_to(server);
    int sent = 0, received = 0;
    if((sent = send(sock, message_to_send, sizeof(struct message), 0)) < 0){
        loggerf("something went wrong while sending");
    }
    if((received = recv(sock, message_to_receive, sizeof(struct message), 0)) < 0){
        loggerf("something went wrong while receiveing");
    }
    close(sock);
    return message_to_receive;
}

void* send_and_recv_data(struct message* message_to_send, const char* server){
    struct message* message_to_receive = malloc(sizeof(struct message));
    int sock = connect_to(server);
    int sent = 0, received = 0;
    if((sent = send(sock, message_to_send, sizeof(struct message), 0)) < 0){
        loggerf("something went wrong while sending");
    }
    if((received = recv(sock, message_to_receive, sizeof(struct message), 0)) < 0){
        loggerf("something went wrong while receiveing message");
    }
    char* data = NULL;
    if(message_to_receive->wait_for_message){
        data = malloc(sizeof(char) * message_to_receive->wait_for_message);
        if((received = recv(sock, data, sizeof(char) * message_to_receive->wait_for_message, 0)) < 0){
            loggerf("something went wrong while receiveing data");
        }
    }
    free(message_to_receive);
    close(sock);
    return data;
}

struct message* send_data_recv_message(struct message* message, 
                                       const char* data, 
                                       int size, 
                                       const char* server){
    // Receive answer
    struct message* to_receive = malloc(sizeof(struct message));
    int sock = connect_to(server);
    int sent = 0, received = 0;
    if((sent = send(sock, message, sizeof(struct message), 0)) < 0){
        loggerf("something went wrong while sending message");
        to_receive->status = -errno;
    }

    // Send data
    if((sent = send(sock, data, size, 0)) < 0){
        loggerf("something went wrong while sending message");
        to_receive->status = -errno;
    }

    if((received = recv(sock, to_receive, sizeof(struct message), 0)) < 0){
        loggerf("something went wrong while receiveing message");
        to_receive->status = -errno;
    }
    close(sock);
    return to_receive;
}

intptr_t connector_opendir(const char * path, const char * server){
    struct message* message_to_send = create_message(fnc_opendir, 0, 0, path);
    struct message* message_to_receive = send_and_recv_message(message_to_send, server);
    intptr_t res = message_to_receive[0].status;
    free(message_to_send);
    free(message_to_receive);
    return res;
}

char* connector_readdir(uintptr_t dp, const char * server){
    struct message* message_to_send = create_message(fnc_readdir, dp, 0, "");
    char* data = (char*)send_and_recv_data(message_to_send, server);
    free(message_to_send);
    return data;
}

struct getattr_ans* connector_getattr(const char* path, const char * server){
    struct message* message_to_send = create_message(fnc_getattr, 0, 0, path);
    struct getattr_ans* data = (struct getattr_ans*)send_and_recv_data(message_to_send, server);
    free(message_to_send);
    return data;
}

int connector_open(const char * path, int flags, const char * server) {
    struct message* message_to_send = create_message(fnc_open, flags, 0, path);
    struct message* message_to_receive = send_and_recv_message(message_to_send, server);
    int res = message_to_receive[0].status;
    free(message_to_send);
    free(message_to_receive);
    return res;
}

char* connector_read(int fd, size_t size, off_t offset, const char * server){
    struct message* message_to_send = create_ext_message(fnc_read, fd, 0, size, offset, "");
    char* data = (char*)send_and_recv_data(message_to_send, server);
    free(message_to_send);
    return data;
}

int connector_write(int fd, const char* buf, size_t size, off_t offset, const char* server){
    struct message* to_send = create_ext_message(fnc_write, fd, size, size, offset, "");
    struct message* to_receive = send_data_recv_message(to_send, buf, size, server);
    int retval = to_receive->status;
    free(to_receive);
    return retval;
}

int connector_utime(const char* path, struct utimbuf* ubuf, const char* server){
    struct message* to_send = create_message(fnc_utime, 0, sizeof*ubuf, path);
    struct message* to_receive = send_data_recv_message(to_send, (const char*)ubuf, sizeof*ubuf, server);
    int retval = to_receive->status;
    free(to_send);
    free(to_receive);
    return retval;
}

int connector_truncate(const char* path, off_t newsize, const char* server){
    struct message* to_send = create_ext_message(fnc_truncate, 0, 0, 0, newsize, path);
    struct message* to_receive = send_and_recv_message(to_send, server);
    int retval = to_receive->status;
    free(to_send);
    free(to_receive);
    return retval;
}

int connector_release(int fd, const char * server){
    struct message* to_send = create_message(fnc_release, fd, 0, "");
    struct message* to_receive = send_and_recv_message(to_send, server);
    int retval = to_receive->status;
    free(to_send);
    free(to_receive);
    return retval;
}

int connector_releasedir(uintptr_t dp, const char * server){
    struct message* to_send = create_message(fnc_releasedir, dp, 0, "");
    struct message* to_receive = send_and_recv_message(to_send, server);
    int retval = to_receive->status;
    free(to_send);
    free(to_receive);
    return retval;
}