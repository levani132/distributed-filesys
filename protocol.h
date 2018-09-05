#pragma once

typedef struct request {
    struct message* (*msg_msg)(struct message* message_to_send, const char* server);
    void* (*msg_data)(struct message* message_to_send, const char* server);
    struct message* (*data_msg)(struct message* message, const char* data, int size, const char* server);
    long (*msg_status)(struct message* message_to_send, const char * server);
    long (*data_status)(struct message * to_send, const char* data, int size, const char* server);
    long (*ping)(const char* server);
} *Request;

typedef struct protocol {
    int (*open_server)(const char * ip, int port);
    void* (*get_data)(int sock, int size);
    struct message* (*get_message)(int sock);
    int (*send_data)(int sock, void* data, int size);
    int (*send_message)(int sock, struct message* message);
    int (*send_status)(int sock, long status);
    struct request request;
} *Protocol;

Protocol new_protocol();