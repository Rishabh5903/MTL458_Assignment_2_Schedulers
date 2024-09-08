#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/time.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <stdint.h>
#include <errno.h>  

uint64_t current_time(uint64_t start_time)
{
    struct timeval time;
    gettimeofday(&time, NULL);
    return (long long)time.tv_sec * 1000 + time.tv_usec / 1000 - start_time;
}
