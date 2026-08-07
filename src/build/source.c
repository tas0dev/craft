/*
 * Copyright (C) 2026 tas0dev
 * This software is licensed under the GNU General Public License, version 3
 * only.
 *
 * Created by tas0dev
 */

#include "source.h"

#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

static int is_source_file(const char *path) {
	const char *extension = strrchr(path, '.');

	if (!extension) return 0;

	return strcmp(extension, ".c") == 0 || strcmp(extension, ".cpp") == 0 ||
	       strcmp(extension, ".cc") == 0 || strcmp(extension, ".s") == 0 ||
	       strcmp(extension, ".S") == 0;
}

static char *path_join(const char *left, const char *right) {
	const size_t len = strlen(right) + 1 + strlen(left) + 1;
	char *path = malloc(len);

	if (!path) return NULL;

	snprintf(path, len, "%s/%s", left, right);
	return path;
}

static int source_list_add(SourceList *sources, const char *path) {
	char **files =
		realloc(sources->files, (sources->count + 1) * sizeof(char *));
	if (!files) return 0;

	sources->files = files;
	sources->files[sources->count] = malloc(strlen(path) + 1);

	if (!sources->files[sources->count]) return 0;

	strcpy(sources->files[sources->count], path);
	sources->count++;

	return 1;
}

#ifdef _WIN32

static int collect_directory(SourceList *sources, const char *directory) {
	char *search_path = path_join(directory, "*");

	if (!search_path) return 0;

	WIN32_FIND_DATAA entry;
	HANDLE handle = FindFirstFileA(search_path, &entry);

	free(search_path);

	if (handle == INVALID_HANDLE_VALUE) return 0;

	do {
		if (strcmp(entry.cFileName, ".") == 0 ||
		    strcmp(entry.cFileName, "..") == 0)
			continue;

		char *path = path_join(directory, entry.cFileName);

		if (!path) {
			FindClose(handle);
			return 0;
		}

		if (entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			if (!collect_directory(sources, path)) {
				free(path);
				FindClose(handle);
				return 0;
			}
		} else if (is_source_file(path)) {
			if (!source_list_add(sources, path)) {
				free(path);
				FindClose(handle);
				return 0;
			}
		}

		free(path);
	} while (FindNextFileA(handle, &entry));

	FindClose(handle);

	return 1;
}

#else

static int collect_directory(SourceList *sources, const char *directory) {
	DIR *dir = opendir(directory);

	if (!dir) return 0;

	struct dirent *entry;

	while ((entry = readdir(dir))) {
		if (strcmp(entry->d_name, ".") == 0 ||
		    strcmp(entry->d_name, "..") == 0)
			continue;

		char *path = path_join(directory, entry->d_name);

		if (!path) {
			closedir(dir);
			return 0;
		}

		struct stat status;

		if (stat(path, &status) != 0) {
			free(path);
			closedir(dir);
			return 0;
		}

		if (S_ISDIR(status.st_mode)) {
			if (!collect_directory(sources, path)) {
				free(path);
				closedir(dir);
				return 0;
			}
		} else if (S_ISREG(status.st_mode) && is_source_file(path)) {
			if (!source_list_add(sources, path)) {
				free(path);
				closedir(dir);
				return 0;
			}
		}

		free(path);
	}

	closedir(dir);

	return 1;
}

#endif

SourceList *source_collect(Manifest *manifest) {
	if (!manifest) return NULL;

	SourceList *sources = calloc(1, sizeof(*sources));

	if (!sources) return NULL;

	for (size_t i = 0; i < manifest->source_dir_count; i++) {
		if (!collect_directory(sources, manifest->source_dirs[i])) {
			source_list_free(sources);
			return NULL;
		}
	}

	return sources;
}

void source_list_free(SourceList *sources) {
	if (!sources) return;

	for (size_t i = 0; i < sources->count; i++)
		free(sources->files[i]);

	free(sources->files);
	free(sources);
}