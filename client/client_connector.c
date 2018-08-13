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
#include <pthread.h>

#include "client_connector.h"
#include "client_config.h"
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
        return ERRCONNECTION;
    }
    return sock;
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

void* reconnect_thread(void* data){
    void** datatmp = (void**)data;
    struct server * server = (struct server*)datatmp[0];
    struct storage* storage = (struct storage*)datatmp[1];
    int timeout = (int)(long)datatmp[2];
    static int is_swap;
    int i = 1;
    while(1){
        sleep(1);
        int status = send_and_recv_status(create_message(fnc_ping, 0, 0, ""), server->name);
        if(status == 1){
            loggerf("try No: %d SUCCESS", i);
            loggerf("connection restored with %s", server->name);
            server->state = SERVER_STARTING;
            return NULL;
        }else{
            if(i >= timeout){
                loggerf("try No: %d FAILED", i);
                loggerf("timout exceeded, server is down");
                if(!is_swap){
                    char tmp[20];
                    strcpy(tmp, server->name);
                    strcpy(server->name, storage->hotswap);
                    strcpy(storage->hotswap, tmp);
                    loggerf("trying to connect with hotswap");
                    return reconnect_thread(data);
                }
                loggerf("couldn't connect to swap either");
                free(data);
                return NULL;
            }
            loggerf("trying to reconnect in 1s, try No: %d FAILED", i);
        }
    }
}

long connector_reconnect(struct server * server, struct storage* storage, int timeout){
    pthread_t tid;
    void** data = malloc(3 * sizeof(void*));
    data[0] = (void*)server;
    data[1] = (void*)storage;
    data[2] = (void*)(long)timeout;
    pthread_create(&tid, NULL, reconnect_thread, data);
    return 0;
}