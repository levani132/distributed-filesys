all: build client server

build:
	mkdir build

client: client/client.c
	gcc -Wall logger.c message.c client/client.c client/client_config.c client/client_connector.c `pkg-config fuse --cflags --libs`  -o build/client -lpthread

server: server/server.c
	gcc -Wall server/server.c server/server_connector.c logger.c message.c hasher.c client/client_connector.c `pkg-config fuse --cflags --libs` -o build/server -lssl -lcrypto

clean:
	rm -fdr build