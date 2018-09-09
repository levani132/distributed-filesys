THIS_FILE := $(lastword $(MAKEFILE_LIST))
all: build client server


debug:
	@$(MAKE) -f $(THIS_FILE) all debug_flag='-g'

build:
	mkdir build
	mkdir test0
	mkdir test1
	mkdir test2
	mkdir test3
	touch error.log

client: client/client.c
	gcc -Wall $(debug_flag) logger.c message.c protocol.c client/client.c client/client_cache.c client/client_config.c `pkg-config fuse --cflags --libs` -o build/client -lpthread

server: server/server.c
	gcc -Wall $(debug_flag) server/server.c server/server_methods.c server/server_hasher.c protocol.c logger.c message.c `pkg-config fuse --cflags --libs` -o build/server -lssl -lcrypto

clean:
	rm -fdr build
	rm -fdr test0
	rm -fdr test1
	rm -fdr test2
	rm -fdr test3
	rm error.log