#include <stdint.h>

intptr_t connector_opendir(const char * path, const char * server);
char* connector_readdir(uintptr_t dp, const char * server);
int connector_releasedir(uintptr_t dp, const char * server);
int connector_open(const char * path, int flags, const char * server);
char* connector_read(int fd, size_t size, off_t offset, const char * server);
int connector_write(int fd, const char* buf, size_t size, off_t offset, const char * server);
int connector_release(int fd, const char * server);
struct getattr_ans* connector_getattr(const char* path, const char * server);
int connector_utime(const char* path, struct utimbuf* ubuf, const char * server);
int connector_truncate(const char* path, off_t newsize, const char * server);