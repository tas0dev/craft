/*
 * Copyright (C) 2026 tas0dev
 * This software is licensed under the GNU General Public License, version 3
 * only.
 *
 * Created by tas0dev
 */

#include "thread.h"

#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
struct Thread {
	HANDLE handle;
};

typedef struct {
	ThreadFunction func;
	void *args;
} ThreadStart;

static DWORD WINAPI thread_entry(const LPVOID parameter) {
	ThreadStart *start = parameter;
	const int result = start->func(start->args);

	free(start);

	return (DWORD)result;
}

Thread *thread_create(ThreadFunction func, void *args) {
	Thread *thread = malloc(sizeof(Thread));

	if (!thread) {
		fprintf(stderr,
			"Failed to create thread: Thread Allocation error\n");
		return NULL;
	}

	ThreadStart *start = malloc(sizeof(ThreadStart));

	if (!start) {
		fprintf(stderr,
			"Failed to create thread: Entry Allocation error\n");

		free(thread);
		return NULL;
	}

	start->func = func;
	start->args = args;

	thread->handle = CreateThread(NULL, 0, thread_entry, start, 0, NULL);

	if (!thread->handle) {
		fprintf(stderr, "Failed to create thread: Thread creation "
				"error(WIN32)\n");
		free(start);
		free(thread);
		return NULL;
	}

	return thread;
}

int thread_join(Thread *t) {
	if (!t) return -1;

	if (WaitForSingleObject(t->handle, INFINITE) != WAIT_OBJECT_0) {
		fprintf(stderr,
			"Failed to join thread: WaitForSingleObject error\n");
		CloseHandle(t->handle);
		free(t);

		return -1;
	}

	DWORD result = 0;

	if (!GetExitCodeThread(t->handle, &result)) {
		fprintf(stderr,
			"Failed to join thread: GetExitCodeThread error\n");
		CloseHandle(t->handle);
		free(t);

		return -1;
	}

	CloseHandle(t->handle);
	free(t);

	return result;
}

size_t thread_cpu_count(void) {
	SYSTEM_INFO info;
	GetSystemInfo(&info);

	if (info.dwNumberOfProcessors == 0) return 0;

	return (size_t)info.dwNumberOfProcessors;
}

#else

#include <pthread.h>
#include <unistd.h>

struct Thread {
	pthread_t handle;
};

typedef struct {
	ThreadFunction func;
	void *args;
	int result;
} ThreadStart;

struct UnixThread {
	Thread t;
	ThreadStart *start;
};

static void *thread_entry(void *parameter) {
	ThreadStart *start = (ThreadStart *)parameter;

	start->result = start->func(start->args);

	return NULL;
}

Thread *thread_create(ThreadFunction func, void *args) {
	struct UnixThread *t = malloc(sizeof(struct UnixThread));

	if (!t) {
		fprintf(stderr,
			"Failed to create thread: Thread Allocation error\n")

			return NULL;
	}

	thread->start.func = func;
	thread->start.args = args;
	thread->start.result = -1;

	if (pthread_create(&t->handle, NULL, thread_entry, &thread->start) !=
	    0) {
		fprintf(stderr, "Failed to create thread: Thread creation "
				"error(UNIX)\n");
		free(t);

		return NULL;
	}

	return &t->t;
}

int thread_join(CraftThread *thread) {
	if (!thread) return -1;

	struct LinuxThread *linux_thread = (struct LinuxThread *)thread;

	if (pthread_join(thread->handle, NULL) != 0) {
		fprintf(stderr, "Failed to join thread\n");

		free(linux_thread);
		return -1;
	}

	const int result = linux_thread->start.result;

	free(linux_thread);

	return result;
}

size_t thread_cpu_count(void) {
	const long count = sysconf(_SC_NPROCESSORS_ONLN);

	if (count <= 0) return 1;

	return (size_t)count;
}

#endif