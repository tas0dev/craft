/*
 * Copyright (C) 2026 tas0dev
 * This software is licensed under the GNU General Public License, version 3
 * only.
 *
 * Created by tas0dev
 */

#include "commands/clean.h"
#include "app.h"
#include "build/project.h"
#include "cli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define getcwd _getcwd
#else
#include <dirent.h>
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

#ifdef _WIN32

static int remove_directory_recursive(const char *path) {
	char *search_path = path_join(path, "*");

	if (!search_path) return 0;

	WIN32_FIND_DATAA entry;

	HANDLE handle = FindFirstFileA(search_path, &entry);

	free(search_path);

	if (handle == INVALID_HANDLE_VALUE) {
		const DWORD error = GetLastError();

		if (error == ERROR_FILE_NOT_FOUND ||
		    error == ERROR_PATH_NOT_FOUND) {
			return 1;
		}

		fprintf(stderr, "Failed to open directory: %s\n", path);

		return 0;
	}

	do {
		if (strcmp(entry.cFileName, ".") == 0 ||
		    strcmp(entry.cFileName, "..") == 0) {
			continue;
		}

		char *entry_path = path_join(path, entry.cFileName);

		if (!entry_path) {
			FindClose(handle);
			return 0;
		}

		if (entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			if (!remove_directory_recursive(entry_path)) {
				free(entry_path);
				FindClose(handle);
				return 0;
			}
		} else {
			if (!DeleteFileA(entry_path)) {
				fprintf(stderr, "Failed to remove file: %s\n",
					entry_path);

				free(entry_path);
				FindClose(handle);
				return 0;
			}
		}

		free(entry_path);

	} while (FindNextFileA(handle, &entry));

	FindClose(handle);

	if (!RemoveDirectoryA(path)) {
		fprintf(stderr, "Failed to remove directory: %s\n", path);

		return 0;
	}

	return 1;
}

#else

static int remove_directory_recursive(const char *path) {
	DIR *directory = opendir(path);

	if (!directory) return 1;

	struct dirent *entry;

	while ((entry = readdir(directory))) {
		if (strcmp(entry->d_name, ".") == 0 ||
		    strcmp(entry->d_name, "..") == 0) {
			continue;
		}

		char *entry_path = path_join(path, entry->d_name);

		if (!entry_path) {
			closedir(directory);
			return 0;
		}

		struct stat status;

		if (stat(entry_path, &status) != 0) {
			fprintf(stderr, "Failed to stat: %s\n", entry_path);

			free(entry_path);
			closedir(directory);
			return 0;
		}

		if (S_ISDIR(status.st_mode)) {
			if (!remove_directory_recursive(entry_path)) {
				free(entry_path);
				closedir(directory);
				return 0;
			}
		} else {
			if (remove(entry_path) != 0) {
				fprintf(stderr, "Failed to remove file: %s\n",
					entry_path);

				free(entry_path);
				closedir(directory);
				return 0;
			}
		}

		free(entry_path);
	}

	closedir(directory);

	if (rmdir(path) != 0) {
		fprintf(stderr, "Failed to remove directory: %s\n", path);

		return 0;
	}

	return 1;
}

#endif

int run_clean(const int argc, char **argv) {
	(void)argc;
	(void)argv;

	char cwd[4096];

	if (!getcwd(cwd, sizeof(cwd))) {
		fprintf(stderr, RED "Failed to get current directory\n" RESET);

		return 1;
	}

	char *project_root = project_find_root(cwd);

	if (!project_root) {
		fprintf(stderr, RED "Could not find " MANIFEST_FILE "\n" RESET);

		return 1;
	}

	char *target_path = path_join(project_root, "target");

	free(project_root);

	if (!target_path) return 1;

	if (!remove_directory_recursive(target_path)) {
		free(target_path);
		return 1;
	}

	printf("Cleaned target\n");

	free(target_path);

	return 0;
}
