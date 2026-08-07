/*
 * Copyright (C) 2026 tas0dev
 * This software is licensed under the GNU General Public License, version 3
 * only.
 *
 * Created by tas0dev
 */

#include "build/project.h"
#include "app.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

static int manifest_exists(const char *path) {
	const size_t length = strlen(path) + 1 + strlen(MANIFEST_FILE) + 1;

	char *manifest_path = malloc(length);

	if (!manifest_path) return 0;

	snprintf(manifest_path, length, "%s/%s", path, MANIFEST_FILE);

#ifdef _WIN32
	const DWORD attributes = GetFileAttributesA(manifest_path);

	const int exists = attributes != INVALID_FILE_ATTRIBUTES &&
			   !(attributes & FILE_ATTRIBUTE_DIRECTORY);
#else
	struct stat status;

	const int exists =
		stat(manifest_path, &status) == 0 && S_ISREG(status.st_mode);
#endif

	free(manifest_path);

	return exists;
}

static char *path_parent(const char *path) {
	const size_t length = strlen(path);

	if (length == 0) return NULL;

	char *parent = malloc(length + 1);

	if (!parent) return NULL;

	strcpy(parent, path);

	while (length > 1 && (parent[strlen(parent) - 1] == '/' ||
			      parent[strlen(parent) - 1] == '\\')) {
		parent[strlen(parent) - 1] = '\0';
	}

	char *slash = strrchr(parent, '/');
	char *backslash = strrchr(parent, '\\');

	char *separator = slash;

	if (!separator || (backslash && backslash > separator))
		separator = backslash;

	if (!separator) {
		free(parent);
		return NULL;
	}

#ifdef _WIN32
	if (separator == parent + 2 && parent[1] == ':') {
		separator[1] = '\0';
		return parent;
	}
#endif

	if (separator == parent) {
		separator[1] = '\0';
		return parent;
	}

	*separator = '\0';

	return parent;
}

char *project_find_root(const char *path) {
	if (!path) return NULL;

	char *current = malloc(strlen(path) + 1);

	if (!current) return NULL;

	strcpy(current, path);

	while (1) {
		if (manifest_exists(current)) return current;

		char *parent = path_parent(current);

		if (!parent) {
			free(current);
			return NULL;
		}

		if (strcmp(parent, current) == 0) {
			free(parent);
			free(current);
			return NULL;
		}

		free(current);
		current = parent;
	}
}