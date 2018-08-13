#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <openssl/md5.h>
#include <sys/xattr.h>

#include "logger.h"
#include "hasher.h"

int hasher(const char *path, char* hash){
    FILE *file = fopen (path, "rb");
    if(file == NULL){
        return -errno;
    }
    unsigned char chars[HASH_SIZE / 2];
    char result[HASH_SIZE];
    MD5_CTX md5_ctx;
    int bytes;
    unsigned char data[1024];
    MD5_Init (&md5_ctx);
    while ((bytes = fread (data, 1, 1024, file)) != 0){
        MD5_Update (&md5_ctx, data, bytes);
    }
    MD5_Final (chars, &md5_ctx);

    int i = 0; for(; i < HASH_SIZE / 2; ++i)
        sprintf (&result[i * 2], "%02x", (unsigned int)chars[i]);
    result[HASH_SIZE - 1] = '\0';
    strcpy (hash, result);
    fclose(file);
    return 0;
}

int hasher_save_for(const char * path){
    char hash[HASH_SIZE];
    hasher(path, hash);
    if(setxattr(path, "user.hash", hash, HASH_SIZE, 0) < 0){
        return -errno;
    }
    return 0;
}

int hasher_get_for(const char * path, char * hash){
    if(getxattr(path, "user.hash", hash, HASH_SIZE) < 0){
        return -errno;
    }
    return 0;
}