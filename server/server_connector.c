#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "../logger.h"
#include "../message.h"


int connector_open_server_on(const char * ip, int port){
	struct sockaddr_in server_address;
	memset(&server_address, 0, sizeof(server_address));
	server_address.sin_family = AF_INET;

	server_address.sin_port = htons(port);

	inet_aton(ip, &server_address.sin_addr);

	int listen_sock;
	if ((listen_sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		loggerf("could not create listen socket");
		return 1;
	}

	if ((bind(listen_sock, (struct sockaddr *)&server_address,
	          sizeof(server_address))) < 0) {
		loggerf("could not bind socket");
		return 1;
	}

	int wait_size = 16;
	if (listen(listen_sock, wait_size) < 0) {
		loggerf("could not open socket for listening");
		return 1;
	}
    return listen_sock;
}

struct message* connector_get_message(int sock){
    int n = 0;
    struct message* message = malloc(sizeof(struct message));
    memset(message, 0, sizeof(struct message));

    if ((n = recv(sock, message, sizeof(struct message), 0)) > 0) {
        return message;
    }else{
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
        loggerf("something went wrong when sending message");
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
        loggerf("something went wrong when sending data");
        return -errno;
    }
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