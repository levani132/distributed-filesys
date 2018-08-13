#pragma once
#include "client_config.h"

struct message* send_and_recv_message(struct message* message_to_send, const char* server);
void* send_and_recv_data(struct message* message_to_send, const char* server);
struct message* send_data_recv_message(struct message* message, const char* data, int size, const char* server);
long send_and_recv_status(struct message* message_to_send, const char * server);
long send_data_recv_status(struct message * to_send, const char* data, int size, const char* server);
long connector_ping(const char* server);
long connector_reconnect(struct server * server, struct storage* storage, int timeout);