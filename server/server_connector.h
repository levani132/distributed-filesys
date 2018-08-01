int connector_open_server_on(const char * ip, int port);

int connector_send_message(int sock, struct message* message);
int connector_send_data(int sock, void* data, int size);

struct message* connector_get_message(int sock);
void* connector_get_data(int sock, int size);