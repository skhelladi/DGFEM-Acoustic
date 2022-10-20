#pragma once
#include <stdio.h>

//error handle
#define Fatal_Error(comment)                                                \
    {                                                                  \
        printf("\033[1m\033[31mFatal error '%s' at %s:%d\033[0m\n", comment, __FILE__, __LINE__); \
        exit(1);                                                       \
    }
