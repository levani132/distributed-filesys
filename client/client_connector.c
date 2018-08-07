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
        LOGGER_ERROR("%s", "could not create socket");
    }
    if (connect(sock, (struct sockaddr*)&server_address,
            sizeof(server_address)) < 0) {
        LOGGER_ERROR("%s", "could not connect to server");
        LOGGER_ERROR("%s", strerror(errno));
    }
    return sock;
}

struct message* send_and_recv_message(struct message* message_to_send, const char* server){
    struct message* message_to_receive = malloc(sizeof(struct message));
    int sock = connect_to(server);
    int sent = 0, received = 0;
    if((sent = send(sock, message_to_send, sizeof(struct message), 0)) < 0){
        LOGGER_ERROR("%s", "something went wrong while sending");
    }
    free(message_to_send);
    if((received = recv(sock, message_to_receive, sizeof(struct message), 0)) < 0){
        LOGGER_ERROR("%s", "something went wrong while receiveing");
    }
    close(sock);
    return message_to_receive;
}

void* send_and_recv_data(struct message* message_to_send, const char* server){
    struct message* message_to_receive = malloc(sizeof(struct message));
    int sock = connect_to(server);
    int sent = 0, received = 0;
    if((sent = send(sock, message_to_send, sizeof(struct message), 0)) < 0){
        LOGGER_ERROR("%s", "something went wrong while sending");
    }
    free(message_to_send);
    if((received = recv(sock, message_to_receive, sizeof(struct message), 0)) < 0){
        LOGGER_ERROR("%s", "something went wrong while receiveing message");
    }
    char* data = NULL;
    if(message_to_receive->wait_for_message){
        data = malloc(sizeof(char) * message_to_receive->wait_for_message);
        if((received = recv(sock, data, sizeof(char) * message_to_receive->wait_for_message, 0)) < 0){
            LOGGER_ERROR("%s", "something went wrong while receiveing data");
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
    struct message* to_receive = malloc(sizeof(struct message));
    int sock = connect_to(server);
    int sent = 0, received = 0;
    message->wait_for_message = size;
    if((sent = send(sock, message, sizeof(struct message), 0)) < 0){
        LOGGER_ERROR("%s", "something went wrong while sending message");
        to_receive->status = -errno;
    }
    free(message);
    if((sent = send(sock, data, size, 0)) < 0){
        LOGGER_ERROR("%s", "something went wrong while sending message");
        to_receive->status = -errno;
    }
    if((received = recv(sock, to_receive, sizeof(struct message), 0)) < 0){
        LOGGER_ERROR("%s", "something went wrong while receiveing message");
        to_receive->status = -errno;
    }
    close(sock);
    return to_receive;
}

long send_and_recv_status(struct message* message_to_send, const char * server){
    struct message* message_to_receive = send_and_recv_message(message_to_send, server);
    long res = message_to_receive[0].status;
    free(message_to_receive);
    return res;
}

long send_data_recv_status(struct message * to_send, const char* data, int size, const char* server){
    struct message* to_receive = send_data_recv_message(to_send, data, strlen(data) + 1, server);
    int retval = to_receive->status;
    free(to_receive);
    return retval;
}