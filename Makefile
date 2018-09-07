all: build client server

build:
	mkdir build

client: client/client.c
	gcc -Wall logger.c message.c protocol.c client/client.c client/client_cache.c client/client_config.c `pkg-config fuse --cflags --libs` -o build/client -lpthread

server: server/server.c
	gcc -Wall server/server.c server/server_methods.c server/server_hasher.c protocol.c logger.c message.c `pkg-config fuse --cflags --libs` -o build/server -lssl -lcrypto

clean:
	rm -fdr build