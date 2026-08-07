/*
 * Copyright (C) 2026 tas0dev
 * This software is licensed under the GNU General Public License, version 3
 * only.
 *
 * Created by tas0dev
 */

#include "process.h"

#ifdef _WIN32

#include <process.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

int process_run(const char *program, const char *const argv[]) {
	const int has_path =
		strchr(program, '/') != NULL || strchr(program, '\\') != NULL;

	const intptr_t result = has_path ? _spawnv(_P_WAIT, program, argv)
					 : _spawnvp(_P_WAIT, program, argv);

	if (result == -1) {
		fprintf(
			stderr, "Failed to execute %s: %s\n", program,
			strerror(errno));

		return -1;
	}

	return (int)result;
}

#else

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int process_run(const char *program, char *const argv[]) {
	const pid_t pid = fork();

	if (pid < 0) return -1;

	if (pid == 0) {
		execvp(program, argv);
		_exit(127);
	}

	int status;

	if (waitpid(pid, &status, 0) < 0) return -1;

	if (WIFEXITED(status)) return WEXITSTATUS(status);

	return -1;
}

#endif