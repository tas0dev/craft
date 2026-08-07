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

static int source_list_add(SourceList *sources,
			   const char *path,
			   const char *relative_path) {
	SourceFile *files = realloc(sources->files,
				    (sources->count + 1) * sizeof(SourceFile));

	if (!files) return 0;

	sources->files = files;

	SourceFile *file = &sources->files[sources->count];

	file->path = malloc(strlen(path) + 1);

	if (!file->path) return 0;

	strcpy(file->path, path);

	file->relative_path = malloc(strlen(relative_path) + 1);

	if (!file->relative_path) {
		free(file->path);
		file->path = NULL;
		return 0;
	}

	strcpy(file->relative_path, relative_path);

	sources->count++;

	return 1;
}

#ifdef _WIN32

static int collect_directory(SourceList *sources,
			     const char *directory,
			     const char *relative_dir) {
	char *search_path = path_join(directory, "*");

	if (!search_path) return 0;

	WIN32_FIND_DATAA entry;

	HANDLE handle = FindFirstFileA(search_path, &entry);

	free(search_path);

	if (handle == INVALID_HANDLE_VALUE) return 0;

	do {
		if (
			strcmp(entry.cFileName, ".") == 0 ||
		    strcmp(entry.cFileName, "..") == 0) {
			continue;
		}

		char *path = path_join(directory, entry.cFileName);

		if (!path) {
			FindClose(handle);
			return 0;
		}

		char *relative_path = NULL;

		if (relative_dir && relative_dir[0] != '\0')
			relative_path =
				path_join(relative_dir, entry.cFileName);
		else {
			relative_path = malloc(strlen(entry.cFileName) + 1);

			if (relative_path)
				strcpy(relative_path, entry.cFileName);
		}

		if (!relative_path) {
			free(path);
			FindClose(handle);
			return 0;
		}

		if (entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			if (!collect_directory(sources, path, relative_path)) {
				free(relative_path);
				free(path);
				FindClose(handle);
				return 0;
			    }
		} else if (is_source_file(path)) {
			if (!source_list_add(
				    sources,
				    path,
				    relative_path)) {
				free(relative_path);
				free(path);
				FindClose(handle);
				return 0;
			}
		}

		free(relative_path);
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

SourceList *source_collect(
	const Manifest *manifest,
	const char *project_root
) {
	if (!manifest || !project_root) return NULL;

	SourceList *sources = calloc(1, sizeof(*sources));

	if (!sources) return NULL;

	for (size_t i = 0; i < manifest->source_dir_count; i++) {
		const char *source_dir_name = manifest->source_dirs[i];

		char *source_dir = path_join(project_root, source_dir_name);

		if (!source_dir) {
			source_list_free(sources);
			return NULL;
		}

		const char *relative_root = "";

		if (strcmp(source_dir_name, "src") != 0)
			relative_root = source_dir_name;

		if (!collect_directory(sources, source_dir, relative_root)) {
			free(source_dir);
			source_list_free(sources);
			return NULL;
		}

		free(source_dir);
	}

	return sources;
}

void source_list_free(SourceList *sources) {
	if (!sources)
		return;

	for (size_t i = 0; i < sources->count; i++) {
		free(sources->files[i].path);
		free(sources->files[i].relative_path);
	}

	free(sources->files);
	free(sources);
}