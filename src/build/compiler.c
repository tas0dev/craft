/*
 * Copyright (C) 2026 tas0dev
 * This software is licensed under the GNU General Public License, version 3
 * only.
 *
 * Created by tas0dev
 */

#include "build/compiler.h"
#include "util/process.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <errno.h>
#include <sys/stat.h>
#endif

static char *path_join(const char *left, const char *right) {
	const size_t length = strlen(left) + 1 + strlen(right) + 1;

	char *path = malloc(length);

	if (!path) return NULL;

	snprintf(path, length, "%s/%s", left, right);

	return path;
}

static int create_directory(const char *path) {
#ifdef _WIN32
	if (_mkdir(path) == 0) return 1;

	return errno == EEXIST;
#else
	if (mkdir(path, 0755) == 0) return 1;

	return errno == EEXIST;
#endif
}

static int create_parent_directories(const char *path) {
	char *copy = malloc(strlen(path) + 1);

	if (!copy) return 0;

	strcpy(copy, path);

	for (char *current = copy + 1; *current; current++) {
		if (*current != '/' && *current != '\\') continue;

#ifdef _WIN32
		if (current == copy + 2 && copy[1] == ':') { continue; }
#endif

		const char separator = *current;
		*current = '\0';

		if (!create_directory(copy)) {
			free(copy);
			return 0;
		}

		*current = separator;
	}

	free(copy);

	return 1;
}

static char *object_path_from_source(const char *project_root,
				     const SourceFile *source) {
	char *build_root = path_join(project_root, "target/build");

	if (!build_root) return NULL;

	char *relative = malloc(strlen(source->relative_path) + 1);

	if (!relative) {
		free(build_root);
		return NULL;
	}

	strcpy(relative, source->relative_path);

	char *extension = strrchr(relative, '.');

	if (extension)
		strcpy(extension, ".o");

	char *object_path =
		path_join(build_root, relative);

	free(relative);
	free(build_root);

	return object_path;
}

static int object_list_add(
	ObjectList *objects,
	const char *path
) {
	char **files = realloc(
		objects->files,
		(objects->count + 1) * sizeof(char *)
	);

	if (!files)
		return 0;

	objects->files = files;

	objects->files[objects->count] =
		malloc(strlen(path) + 1);

	if (!objects->files[objects->count])
		return 0;

	strcpy(
		objects->files[objects->count],
		path
	);

	objects->count++;

	return 1;
}

static int compile_source(
	const SourceFile *source,
	const char *object_path
) {
	const char *argv[] = {
		"cc",
		"-c",
		source->path,
		"-o",
		(char *)object_path,
		NULL
	};

	printf(
		"Compiling %s\n",
		source->relative_path
	);

	return process_run("cc", argv);
}

ObjectList *compile_sources(
	const Manifest *manifest,
	const SourceList *sources,
	const char *project_root
) {
	if (
		!manifest ||
		!sources ||
		!project_root
	) {
		return NULL;
	}

	ObjectList *objects =
		calloc(1, sizeof(*objects));

	if (!objects)
		return NULL;

	for (size_t i = 0; i < sources->count; i++) {
		const SourceFile *source =
			&sources->files[i];

		char *object_path =
			object_path_from_source(
				project_root,
				source
			);

		if (!object_path) {
			object_list_free(objects);
			return NULL;
		}

		if (!create_parent_directories(object_path)) {
			free(object_path);
			object_list_free(objects);
			return NULL;
		}

		const int result =
			compile_source(
				source,
				object_path
			);

		if (result != 0) {
			fprintf(
				stderr,
				"Failed to compile %s\n",
				source->relative_path
			);

			free(object_path);
			object_list_free(objects);
			return NULL;
		}

		if (!object_list_add(
			    objects,
			    object_path
		    )) {
			free(object_path);
			object_list_free(objects);
			return NULL;
		}

		free(object_path);
	}

	return objects;
}

void object_list_free(
	ObjectList *objects
) {
	if (!objects)
		return;

	for (size_t i = 0; i < objects->count; i++)
		free(objects->files[i]);

	free(objects->files);
	free(objects);
}