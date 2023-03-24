// SPDX-License-Identifier: Zlib
//
// Copyright (c) 2023 Antonio Niño Díaz

#include <errno.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <sys/_timeval.h>
#include <time.h>

#include "libnds_internal.h"

// This file implements stubs for system calls. For more information about it,
// check the documentation of newlib:
//
//     https://sourceware.org/newlib/libc.html#Syscalls

// TODO: Use stderr console to send messages to no$gba?

int getpid(void)
{
    // The PID of this process is 1
    return 1;
}

void __attribute__((noreturn)) _exit(int status)
{
    (void)status;

    // Hang, there is nowhere to go. The ARM7 shouldn't call this directly, only
    // the ARM9 should.
    while (1);
}

int _kill(pid_t pid, int sig)
{
    // The only process that exists is this process, and it can be killed.
    if (pid == 1)
        _exit(128 + sig);

    errno = ESRCH;
    return -1;
}

int isatty(int file)
{
    (void)file;

    return 0;
}

off_t lseek(int fd, off_t offset, int whence)
{
    (void)fd;
    (void)offset;
    (void)whence;

    return 0;
}

_off64_t lseek64(int fd, _off64_t offset, int whence)
{
    return (_off64_t)lseek(fd, (off_t)offset, whence);
}

int open(const char *path, int flags, ...)
{
    (void)path;
    (void)flags;

    return -1;
}

ssize_t read(int fd, void *ptr, size_t len)
{
    (void)fd;
    (void)ptr;

    return len;
}

ssize_t write(int fd, const void *ptr, size_t len)
{
    (void)fd;
    (void)ptr;

    return len;
}

int close(int fd)
{
    (void)fd;

    return -1;
}

int unlink(const char *name)
{
    (void)name;

    errno = ENOENT;
    return -1;
}

clock_t times(struct tms *buf)
{
    (void)buf;

    // TODO

    return -1;
}

int stat(const char *path, struct stat *st)
{
    (void)path;

    st->st_mode = S_IFCHR;
    return 0;
}

int link(const char *old, const char *new)
{
    (void)old;
    (void)new;

    errno = EMLINK;
    return -1;
}

int fork(void)
{
    errno = ENOSYS;
    return -1;
}

int gettimeofday(struct timeval *tp, void *tz)
{
    // TODO

    (void)tp;
    (void)tz;

    if (tp != NULL)
    {
        tp->tv_sec = __transferRegion()->unixTime;
        tp->tv_usec = 0;
    }

    //return 0;
    return -1;
}

int execve(const char *name, char * const *argv, char * const *env)
{
    (void)name;
    (void)argv;
    (void)env;

    errno = ENOMEM;
    return -1;
}

void *sbrk(int incr)
{
    // Symbol defined by the linker
    extern char __end__[];
    const uintptr_t end = (uintptr_t)__end__;

    // Trick to get the current stack pointer
    register uintptr_t stack_ptr asm("sp");

    // Next address to be used. It is updated after every call to sbrk()
    static uintptr_t heap_start = 0;

    if (heap_start == 0)
        heap_start = end;

    // Current limit
    uintptr_t heap_end = stack_ptr;

    // Try to allocate
    if (heap_start + incr > heap_end)
    {
        errno = ENOMEM;
        return (void *)-1;
    }

    uintptr_t prev_heap_start = heap_start;

    heap_start += incr;

    return (void *)prev_heap_start;
}
