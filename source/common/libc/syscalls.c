// SPDX-License-Identifier: Zlib
//
// Copyright (c) 2023 Antonio Niño Díaz

#include <errno.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <sys/_timeval.h>
#include <time.h>

#include "../libc_private.h"

// This file implements stubs for system calls. For more information about it,
// check the documentation of newlib and picolibc:
//
//     https://sourceware.org/newlib/libc.html#Syscalls
//     https://github.com/picolibc/picolibc/blob/main/doc/os.md

void __attribute__((noreturn)) _exit(int status)
{
    __libnds_exit(status);

    // Hang, there is nowhere to go
    while (1);
}

pid_t getpid(void)
{
    // The PID of this process is 1
    return 1;
}

int kill(pid_t pid, int sig)
{
    // The only process that exists is this process, and it can be killed.
    if (pid == 1)
        _exit(128 + sig);

    errno = ESRCH;
    return -1;
}

clock_t times(struct tms *buf)
{
    (void)buf;

    // TODO

    return -1;
}

int fork(void)
{
    errno = ENOSYS;
    return -1;
}

int gettimeofday(struct timeval *tp, void *tz)
{
    (void)tz;

    if (tp != NULL)
    {
        tp->tv_sec = *punixTime;
        tp->tv_usec = 0;
    }

    return 0;
}

int execve(const char *name, char * const *argv, char * const *env)
{
    (void)name;
    (void)argv;
    (void)env;

    errno = ENOMEM;
    return -1;
}

int getentropy(void *buffer, size_t length) {
    (void)buffer;
    (void)length;

    // TODO

    errno = ENOSYS;
    return -1;
}
