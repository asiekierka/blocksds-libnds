// SPDX-License-Identifier: Zlib
//
// Copyright (c) 2024 Adrian "asie" Siekierka

#include <stdio.h>
#include <nds.h>
#include <nds/cothread.h>

#define GUARD_INITIALIZED(obj) ((*(obj)) & 0x1)
// GCC checks the first byte of the four-byte guard type.
#define GUARD_LOCKED(obj) ((*(obj)) & 0x100)

int __cxa_guard_acquire(int *obj)
{
    if (GUARD_INITIALIZED(obj))
        return 0;

    while (GUARD_LOCKED(obj))
        cothread_yield();
    *obj |= 0x100;

    return 1;
}

void __cxa_guard_release(int *obj)
{
    *obj &= ~0x100;
    *obj |= 1;
}

void __cxa_guard_abort(int *obj)
{
    *obj &= ~0x100;
}
