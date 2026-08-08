/*
 * Copyright (C) 2026 tas0dev
 * This software is licensed under the GNU General Public License, version 3
 * only.
 *
 * Created by tas0dev
 */

#ifndef CRAFT_THREAD_H
#define CRAFT_THREAD_H
#include <stddef.h>

typedef struct Thread Thread;

typedef int (*ThreadFunction)(void *args);

Thread *thread_create(ThreadFunction func, void *args);
int thread_join(Thread *t);
size_t thread_cpu_count(void);

#endif // CRAFT_THREAD_H
