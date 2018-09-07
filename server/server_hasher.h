#pragma once
#include <stdio.h>
#define HASH_SIZE 33

int hasher(const char * path, char * hash);
int hasher_get_for(const char * path, char * hash);
int hasher_save_for(const char * path);