#pragma once
#include "fuse.h"

struct getattr_ans {
    int retval;
    struct stat stat;
};