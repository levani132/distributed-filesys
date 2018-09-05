#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

#include "logger.h"
#include "message.h"
#include "protocol.h"


// This method creates server with params ip and port
int connector_open_server_on(const char * ip, int port){
	struct sockaddr_in server_address;
	memset(&server_address, 0, sizeof(server_address));
	server_address.sin_family = AF_INET;

	server_address.sin_port = htons(port);

	inet_aton(ip, &server_address.sin_addr);

	int listen_sock;
	if ((listen_sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		console.log("could not create listen socket");
		return 1;
	}

	if ((bind(listen_sock, (struct sockaddr *)&server_address,
	          sizeof(server_address))) < 0) {
		console.log("could not bind socket");
		return 1;
	}

    ;
    if(fcntl (listen_sock, F_SETFL, fcntl (listen_sock, F_GETFL, 0) | O_NONBLOCK) < 0){
        console.log("could not make socket nonblocking");
    }

	int wait_size = 16;
	if (listen(listen_sock, wait_size) < 0) {
		console.log("could not open socket for listening");
		return 1;
	}
    return listen_sock;
}

// This method connects to server (assuming server has format {ip}:{port})
// It's private
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
        return ERRCONNECTION;
    }
    return sock;
}

struct message* connector_get_message(int sock){
    int n = 0;
    struct message* message = malloc(sizeof(struct message));
    memset(message, 0, sizeof(struct message));

    if ((n = recv(sock, message, sizeof(struct message), 0)) > 0) {
        return message;
    }else{
        free(message);
        return NULL;
    }
}

void* connector_get_data(int sock, int size){
    int n = 0;
    char* message = malloc(size);
    memset(message, 0, size);

    if ((n = recv(sock, message, size, 0)) > 0) {
        return message;
    }else{
        return NULL;
    }
}

int connector_send_message(int sock, struct message* message){
    int n_sent;
    if((n_sent = send(sock, message, sizeof*message, 0)) != sizeof*message){
        console.log("something went wrong when sending message");
        return -errno;
    }
    return n_sent;
}

int connector_send_data(int sock, void* data, int size){
    struct message* message = create_message(0, 0, size, "");
    int n_sent;
    if((n_sent = connector_send_message(sock, message)) < 0){
        free(message);
        return n_sent;
    }
    free(message);
    if(size && (n_sent = send(sock, data, size, 0)) != size){
        console.log("something went wrong when sending data");
        return -errno;
    }
    free(data);
    return 0;
}

int connector_send_status(int sock, long status) {
    int n_sent;
    struct message* to_send = create_message(0, status, 0, "");
    if((n_sent = connector_send_message(sock, to_send)) < 0){
        free(to_send);
        return n_sent;
    }
    free(to_send);
    return 0;
}

struct message* send_and_recv_message(struct message* message_to_send, const char* server){
    struct message* message_to_receive = malloc(sizeof(struct message));
    int sock, sent = 0, received = 0;
    if((sock = connect_to(server)) < 0){
        message_to_receive->status = sock;
        return message_to_receive;
    }
    if((sent = send(sock, message_to_send, sizeof(struct message), 0)) < 0){
        LOGGER_ERROR("something went wrong while sending");
        message_to_receive->status = sock;
        close(sock);
        return message_to_receive;
    }
    free(message_to_send);
    if((received = recv(sock, message_to_receive, sizeof(struct message), 0)) < 0){
        LOGGER_ERROR("something went wrong while receiveing");
        message_to_receive->status = sock;
        close(sock);
        return message_to_receive;
    }
    close(sock);
    return message_to_receive;
}

void* send_and_recv_data(struct message* message_to_send, const char* server){
    struct message* message_to_receive = malloc(sizeof(struct message));
    long sock = 0, sent = 0, received = 0;
    if((sock = connect_to(server)) < 0){
        free(message_to_receive);
        return (void*)sock;
    }
    if((sent = send(sock, message_to_send, sizeof(struct message), 0)) < 0){
        LOGGER_ERROR("something went wrong while sending");
        close(sock);
        free(message_to_receive);
        return (void*)ERRCONNECTION;
    }
    free(message_to_send);
    if((received = recv(sock, message_to_receive, sizeof(struct message), 0)) < 0){
        LOGGER_ERROR("something went wrong while receiveing message");
        close(sock);
        free(message_to_receive);
        return (void*)ERRCONNECTION;
    }
    char* data = NULL;
    if(message_to_receive->wait_for_message){
        data = malloc(sizeof(char) * message_to_receive->wait_for_message);
        if((received = recv(sock, data, sizeof(char) * message_to_receive->wait_for_message, 0)) < 0){
            LOGGER_ERROR("something went wrong while receiveing data");
            close(sock);
            free(message_to_receive);
            free(data);
            return (void*)ERRCONNECTION;
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
    int sock = 0, sent = 0, received = 0;
    if((sock = connect_to(server)) < 0){
        to_receive->status = sock;
        close(sock);
        free(message);
        return to_receive;
    }
    message->wait_for_message = size;
    if((sent = send(sock, message, sizeof(struct message), 0)) < 0){
        LOGGER_ERROR("something went wrong while sending message");
        to_receive->status = ERRCONNECTION;
        close(sock);
        free(message);
        return to_receive;
    }
    free(message);
    if((sent = send(sock, data, size, 0)) < 0){
        LOGGER_ERROR("something went wrong while sending message");
        to_receive->status = ERRCONNECTION;
        close(sock);
        return to_receive;
    }
    if((received = recv(sock, to_receive, sizeof(struct message), 0)) < 0){
        LOGGER_ERROR("something went wrong while receiveing message");
        to_receive->status = ERRCONNECTION;
        close(sock);
        return to_receive;
    }
    close(sock);
    return to_receive;
}

long parse_status(struct message * msg){
    long res = msg->status;
    free(msg);
    return res;
}

long send_and_recv_status(struct message* message_to_send, const char * server){
    return parse_status(send_and_recv_message(message_to_send, server));
}

long send_data_recv_status(struct message * to_send, const char* data, int size, const char* server){
    return parse_status(send_data_recv_message(to_send, data, strlen(data) + 1, server));
}

long connector_ping(const char* server){
    return send_and_recv_status(create_message(fnc_ping, 0, 0, ""), server);
}

Protocol new_protocol () {
    Protocol p = malloc(sizeof(struct protocol));
    struct protocol tmp = {
        .open_server = connector_open_server_on,
        .get_data = connector_get_data,
        .get_message = connector_get_message,
        .send_data = connector_send_data,
        .send_message = connector_send_message,
        .send_status = connector_send_status,
        .request = {
            .msg_msg = send_and_recv_message,
            .msg_data = send_and_recv_data,
            .data_msg = send_data_recv_message,
            .msg_status = send_and_recv_status,
            .data_status = send_data_recv_status,
            .ping = connector_ping
        }
    };
    memcpy(p, &tmp, sizeof(struct protocol));
    return p;
}