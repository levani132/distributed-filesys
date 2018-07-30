all: build client server

build:
	mkdir build

client: client/client.c
	gcc -Wall client/client.c client/client_config.c logger.c `pkg-config fuse --cflags --libs`  -o build/client

server: server/server.c
	gcc server/server.c -o build/server

clean:
	rm -fdr build