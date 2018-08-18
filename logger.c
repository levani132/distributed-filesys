#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

FILE * file = NULL;

void logger(const char * storage_name, const char * server_addr, const char * fmt, ...){
    va_list ar;
    va_start(ar, fmt);
    time_t current_time;
    char* c_time_string;
    current_time = time(NULL);
    c_time_string = ctime(&current_time);
	c_time_string[strlen(c_time_string) - 1] = '\0';
    fprintf(file ? file : stdout, (file ? "[%s] " : "[\x1B[35m%s\x1B[0m] "), c_time_string);
    if(storage_name != NULL){
        fprintf(file ? file : stdout, (file ? "%s " : "\x1B[32m%s\x1B[0m "), storage_name);
    }
    if(server_addr != NULL){
        fprintf(file ? file : stdout, (file ? "%s " : "\x1B[36m%s\x1B[0m "), server_addr);
    }
    vfprintf(file ? file : stdout, fmt, ar);
    fprintf(file ? file : stdout, "\n");
    fflush(file ? file : stdout);
}

void loggerf(const char * fmt, ...){
    va_list ar;
    va_start(ar, fmt);
    time_t current_time;
    char* c_time_string;
    current_time = time(NULL);
    c_time_string = ctime(&current_time);
	c_time_string[strlen(c_time_string) - 1] = '\0';
    fprintf(file ? file : stdout, (file ? "[%s] " : "[\x1B[35m%s\x1B[0m] "), c_time_string);
    vfprintf(file ? file : stdout, fmt, ar);
    fprintf(file ? file : stdout, "\n");
    fflush(file ? file : stdout);
}

void logger_error(const char* filename, int line, const char * fmt, ...){
    va_list ar;
    va_start(ar, fmt);
    time_t current_time;
    char* c_time_string;
    current_time = time(NULL);
    c_time_string = ctime(&current_time);
	c_time_string[strlen(c_time_string) - 1] = '\0';
    fprintf(file ? file : stdout, (file ? "[%s] " : "[\x1B[35m%s\x1B[0m] "), c_time_string);
    fprintf(file ? file : stdout, (file ? "%s:%d " : "\x1B[33m%s:%d\x1B[0m "), filename, line);
    vfprintf(file ? file : stdout, fmt, ar);
    fprintf(file ? file : stdout, "\n");
    fflush(file ? file : stdout);
}

void logger_set_file(FILE * file_in){
    file = file_in;
}

void logger_unset_file(){
    fclose(file);
    file = NULL;
}

void memswap(void * left, void * right, size_t size){
    void * tmp = malloc(size);
    memcpy(tmp, left, size);
    memcpy(left, right, size);
    memcpy(right, tmp, size);
    free(tmp);
}