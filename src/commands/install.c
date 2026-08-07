/*
 * Copyright (C) 2026 tas0dev
 * This software is licensed under the GNU General Public License, version 3
 * only.
 *
 * Created by tas0dev
 */

#include "commands/install.h"
#include "cli.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

static char *path_join(const char *left, const char *right) {
	const size_t length = strlen(left) + 1 + strlen(right) + 1;

	char *path = malloc(length);

	if (!path) {
		fprintf(stderr, "Failed to allocate path\n");

		return NULL;
	}

	snprintf(path, length, "%s/%s", left, right);

	return path;
}

static char *get_executable_path(void) {
#ifdef _WIN32
	DWORD capacity = 4096;

	char *path = malloc(capacity);

	if (!path) {
		fprintf(stderr, "Failed to allocate executable path\n");

		return NULL;
	}

	const DWORD length = GetModuleFileNameA(NULL, path, capacity);

	if (length == 0 || length >= capacity) {
		fprintf(stderr, "Failed to get executable path\n");

		free(path);
		return NULL;
	}

	return path;
#else
	size_t capacity = 4096;

	char *path = malloc(capacity);

	if (!path) {
		fprintf(stderr, "Failed to allocate executable path\n");

		return NULL;
	}

	const ssize_t length = readlink("/proc/self/exe", path, capacity - 1);

	if (length < 0) {
		fprintf(stderr, "Failed to get executable path\n");

		free(path);
		return NULL;
	}

	path[length] = '\0';

	return path;
#endif
}

static char *get_install_path(void) {
#ifdef _WIN32
	const char *local_app_data = getenv("LOCALAPPDATA");

	if (!local_app_data) {
		fprintf(stderr, "LOCALAPPDATA is not set\n");

		return NULL;
	}

	char *windows_apps = path_join(local_app_data, "Microsoft/WindowsApps");

	if (!windows_apps) return NULL;

	char *install_path = path_join(windows_apps, "craft.exe");

	free(windows_apps);

	return install_path;
#else
	const char *home = getenv("HOME");

	if (!home) {
		fprintf(stderr, "HOME is not set\n");

		return NULL;
	}

	char *local_bin = path_join(home, ".local/bin");

	if (!local_bin) return NULL;

	char *install_path = path_join(local_bin, "craft");

	free(local_bin);

	return install_path;
#endif
}

static int copy_file(const char *source, const char *destination) {
	FILE *input = fopen(source, "rb");

	if (!input) {
		fprintf(stderr, "Failed to open %s: %s\n", source,
			strerror(errno));

		return 0;
	}

	FILE *output = fopen(destination, "wb");

	if (!output) {
		fprintf(stderr, "Failed to open %s: %s\n", destination,
			strerror(errno));

		fclose(input);
		return 0;
	}

	char buffer[16384];

	size_t size;

	while ((size = fread(buffer, 1, sizeof(buffer), input)) != 0) {
		if (fwrite(buffer, 1, size, output) != size) {
			fprintf(stderr, "Failed to write %s\n", destination);

			fclose(output);
			fclose(input);

			return 0;
		}
	}

	if (ferror(input)) {
		fprintf(stderr, "Failed to read %s\n", source);

		fclose(output);
		fclose(input);

		return 0;
	}

	fclose(output);
	fclose(input);

#ifndef _WIN32
	if (chmod(destination, 0755) != 0) {
		fprintf(stderr, "Failed to make %s executable: %s\n",
			destination, strerror(errno));

		return 0;
	}
#endif

	return 1;
}

int run_install(const int argc, char **argv) {
	(void)argc;
	(void)argv;

	char *source = get_executable_path();

	if (!source) return 1;

	char *destination = get_install_path();

	if (!destination) {
		free(source);
		return 1;
	}

#ifdef _WIN32
	if (strcmp(source, destination) == 0) {
		printf(GREEN "Craft is already installed\n" RESET);

		free(destination);
		free(source);

		return 0;
	}
#else
	char *local_bin = NULL;

	const char *home = getenv("HOME");

	if (home) { local_bin = path_join(home, ".local/bin"); }

	if (!local_bin) {
		free(destination);
		free(source);

		return 1;
	}

	if (mkdir(local_bin, 0755) != 0 && errno != EEXIST) {
		fprintf(stderr, "Failed to create %s: %s\n", local_bin,
			strerror(errno));

		free(local_bin);
		free(destination);
		free(source);

		return 1;
	}

	free(local_bin);
#endif

	if (!copy_file(source, destination)) {
		free(destination);
		free(source);

		return 1;
	}

	printf(GREEN "Installed Craft to " RESET "%s\n", destination);

	free(destination);
	free(source);

	return 0;
}
