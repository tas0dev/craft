/*
 * Copyright (C) 2026 tas0dev
 * This software is licensed under the GNU General Public License, version 3
 * only.
 *
 * Created by tas0dev
 */

#include "build/compiler.h"

#include "cli.h"
#include "util/process.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <errno.h>
#endif

typedef struct {
	char **items;
	size_t count;
} IncludeList;

static char *path_join(const char *left, const char *right) {
	const size_t length = strlen(left) + 1 + strlen(right) + 1;

	char *path = malloc(length);

	if (!path) return NULL;

	snprintf(path, length, "%s/%s", left, right);

	return path;
}

static void include_list_free(IncludeList *list) {
	if (!list) return;

	for (size_t i = 0; i < list->count; i++)
		free(list->items[i]);

	free(list->items);

	list->items = NULL;
	list->count = 0;
}

static int include_list_contains(const IncludeList *list, const char *path) {
	for (size_t i = 0; i < list->count; i++) {
		if (strcmp(list->items[i], path) == 0) return 1;
	}

	return 0;
}

static int include_list_add(IncludeList *list, const char *path) {
	if (include_list_contains(list, path)) return 1;

	char **items = realloc(list->items, (list->count + 1) * sizeof(char *));

	if (!items) {
		fprintf(stderr, "Failed to allocate include list\n");

		return 0;
	}

	list->items = items;

	list->items[list->count] = malloc(strlen(path) + 1);

	if (!list->items[list->count]) {
		fprintf(stderr, "Failed to allocate include path\n");

		return 0;
	}

	strcpy(list->items[list->count], path);

	list->count++;

	return 1;
}

static int collect_target_includes(const Manifest *manifest,
				   const BuildTarget *target,
				   const char *project_root,
				   IncludeList *includes) {
	for (size_t i = 0; i < target->include_dir_count; i++) {
		char *include_path =
			path_join(project_root, target->include_dirs[i]);

		if (!include_path) {
			fprintf(stderr, "Failed to create include path: %s\n",
				target->include_dirs[i]);

			return 0;
		}

		if (!include_list_add(includes, include_path)) {
			free(include_path);
			return 0;
		}

		free(include_path);
	}

	for (size_t i = 0; i < target->dependency_count; i++) {
		BuildTarget *dependency =
			manifest_find_target(manifest, target->dependencies[i]);

		if (!dependency) {
			fprintf(stderr, "Unknown dependency: %s\n",
				target->dependencies[i]);

			return 0;
		}

		if (!collect_target_includes(manifest, dependency, project_root,
					     includes)) {
			return 0;
		}
	}

	return 1;
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

	if (extension) strcpy(extension, ".o");

	char *object_path = path_join(build_root, relative);

	free(relative);
	free(build_root);

	return object_path;
}

static int object_list_add(ObjectList *objects, const char *path) {
	char **files =
		realloc(objects->files, (objects->count + 1) * sizeof(char *));

	if (!files) return 0;

	objects->files = files;

	objects->files[objects->count] = malloc(strlen(path) + 1);

	if (!objects->files[objects->count]) return 0;

	strcpy(objects->files[objects->count], path);

	objects->count++;

	return 1;
}

static char *build_compile_command(
	const Manifest *manifest,
	const BuildTarget *target,
	const SourceFile *source,
	const char *object_path,
	const char *dependency_path,
				   const char *project_root) {
	IncludeList includes = {0};

	if (!collect_target_includes(manifest, target, project_root,
				     &includes)) {
		include_list_free(&includes);
		return NULL;
	}

	size_t length = strlen(manifest->cc);

	for (size_t i = 0; i < includes.count; i++) {
		length += strlen(" -I");
		length += strlen(includes.items[i]);
	}

	for (size_t i = 0; i < target->cflags_count; i++) {
		length += 1;
		length += strlen(target->cflags[i]);
	}

	length += strlen(" -MMD -MF ");
	length += strlen(dependency_path);

	length += strlen(" -c ");
	length += strlen(source->path);

	length += strlen(" -o ");
	length += strlen(object_path);

	char *command =
		malloc(length + 1);

	if (!command) {
		fprintf(
			stderr,
			"Failed to allocate compile command for %s\n",
			source->relative_path
		);

		include_list_free(&includes);
		return NULL;
	}

	command[0] = '\0';

	strcat(command, manifest->cc);

	for (size_t i = 0; i < includes.count; i++) {
		strcat(command, " -I");
		strcat(command, includes.items[i]
		);
	}

	for (
		size_t i = 0;
		i < target->cflags_count; i++) {
		strcat(command, " ");
		strcat(command, target->cflags[i]
		);
	}

	strcat(command, " -MMD -MF ");
	strcat(command, dependency_path);

	strcat(command, " -c ");
	strcat(command, source->path);

	strcat(command, " -o ");
	strcat(command, object_path);

	include_list_free(&includes);

	return command;
}

static char *command_path_from_object(const char *object_path) {
	const char *extension = strrchr(object_path, '.');

	if (!extension) {
		fprintf(stderr,
			"Failed to create command path: object path has no "
			"extension: %s\n",
			object_path);
		return NULL;
	}

	const size_t base_length = (size_t)(extension - object_path);

	const size_t length = base_length + strlen(".cmd") + 1;

	char *path = malloc(length);

	if (!path) {
		fprintf(stderr, "Failed to allocate command path for %s\n",
			object_path);
		return NULL;
	}

	memcpy(path, object_path, base_length);

	strcpy(path + base_length, ".cmd");

	return path;
}

static int command_matches(const char *command_path, const char *command) {
	FILE *file = fopen(command_path, "rb");

	if (!file) return 0;

	if (fseek(file, 0, SEEK_END) != 0) {
		fclose(file);
		return 0;
	}

	const long size = ftell(file);

	if (size < 0) {
		fclose(file);
		return 0;
	}

	rewind(file);

	const size_t command_length = strlen(command);

	if ((size_t)size != command_length) {
		fclose(file);
		return 0;
	}

	char *saved = malloc(command_length + 1);

	if (!saved) {
		fclose(file);
		return 0;
	}

	const size_t read_size = fread(saved, 1, command_length, file);

	fclose(file);

	if (read_size != command_length) {
		free(saved);
		return 0;
	}

	saved[command_length] = '\0';

	const int matches = strcmp(saved, command) == 0;

	free(saved);

	return matches;
}

static int command_write(const char *command_path, const char *command) {
	FILE *file = fopen(command_path, "wb");

	if (!file) return 0;

	const size_t length = strlen(command);

	const size_t written = fwrite(command, 1, length, file);

	fclose(file);

	return written == length;
}

static char *dependency_path_from_object(const char *object_path) {
	char *path = malloc(strlen(object_path) + 1);

	if (!path) return NULL;

	strcpy(path, object_path);

	char *extension = strrchr(path, '.');

	if (!extension) {
		free(path);
		return NULL;
	}

	strcpy(extension, ".d");

	return path;
}

static int should_compile(const char *object_path,
			  const char *dependency_path,
			  const char *command_path,
			  const char *command) {
	struct stat object_stat;

	if (stat(object_path, &object_stat) != 0) return 1;

	if (!command_matches(command_path, command)) { return 1; }

	FILE *file = fopen(dependency_path, "r");

	if (!file) return 1;

	char line[4096];
	int first_token = 1;

	while (fgets(line, sizeof(line), file)) {
		char *token = strtok(line, " \t\r\n");

		while (token) {
			if (first_token) {
				first_token = 0;

				token = strtok(NULL, " \t\r\n");

				continue;
			}

			if (strcmp(token, "\\") == 0) {
				token = strtok(NULL, " \t\r\n");

				continue;
			}

			struct stat dependency_stat;

			if (stat(token, &dependency_stat) != 0) {
				fclose(file);
				return 1;
			}

			if (dependency_stat.st_mtime > object_stat.st_mtime) {
				fclose(file);
				return 1;
			}

			token = strtok(NULL, " \t\r\n");
		}
	}

	fclose(file);

	return 0;
}

static int compile_source(
	const Manifest *manifest,
	const BuildTarget *target,
	const SourceFile *source,
	const char *object_path,
	const char *dependency_path,
	const char *project_root
) {
	IncludeList includes = {0};

	if (!collect_target_includes(manifest, target, project_root,
				     &includes)) {
		include_list_free(&includes);
		return -1;
	}

	const size_t argument_count =
		1 + includes.count * 2 + target->cflags_count + 8 + 1;

	const char **argv = calloc(argument_count, sizeof(char *));

	if (!argv) {
		fprintf(stderr,
			"Failed to allocate compiler arguments for %s\n",
			source->relative_path);

		include_list_free(&includes);
		return -1;
	}

	size_t argument_index = 0;

	argv[argument_index++] = manifest->cc;

	for (
		size_t i = 0;
		i < includes.count; i++
	) {
		argv[argument_index++] = "-I";
		argv[argument_index++] = includes.items[i];
	}

	for (size_t i = 0; i < target->cflags_count;
		i++
	) {
		argv[argument_index++] = target->cflags[i];
	}

	argv[argument_index++] = "-MMD";
	argv[argument_index++] = "-MF";
	argv[argument_index++] = dependency_path;

	argv[argument_index++] = "-c";
	argv[argument_index++] = source->path;

	argv[argument_index++] = "-o";
	argv[argument_index++] = object_path;

	argv[argument_index] = NULL;

	printf(
		GREEN "Compiling" RESET "\t%s\n", source->relative_path);

	const int result =
		process_run(
			manifest->cc,
			argv
		);

	free(argv);
	include_list_free(&includes);

	return result;
}

ObjectList *compile_sources(const Manifest *manifest,
			    const BuildTarget *target,
			    const SourceList *sources,
			    const char *project_root) {
	if (!manifest) {
		fprintf(stderr, "compile_sources: manifest is NULL\n");
		return NULL;
	}

	if (!sources) {
		fprintf(stderr, "compile_sources: sources is NULL\n");
		return NULL;
	}

	if (!project_root) {
		fprintf(stderr, "compile_sources: project_root is NULL\n");
		return NULL;
	}

	ObjectList *objects = calloc(1, sizeof(*objects));

	if (!objects) {
		fprintf(stderr, "Failed to allocate object list\n");
		return NULL;
	}

	for (size_t i = 0; i < sources->count; i++) {
		const SourceFile *source = &sources->files[i];

		char *object_path =
			object_path_from_source(project_root, source);

		if (!object_path) {
			fprintf(stderr, "Failed to create object path for %s\n",
				source->relative_path);

			object_list_free(objects);
			return NULL;
		}

		char *dependency_path =
			dependency_path_from_object(object_path);

		if (!dependency_path) {
			fprintf(stderr,
				"Failed to create dependency path for %s\n",
				source->relative_path);

			free(object_path);
			object_list_free(objects);
			return NULL;
		}

		char *command_path = command_path_from_object(object_path);

		if (!command_path) {
			fprintf(stderr,
				"Failed to create command path for %s\n",
				source->relative_path);

			free(dependency_path);
			free(object_path);
			object_list_free(objects);
			return NULL;
		}

		char *command = build_compile_command(
			manifest, target, source, object_path, dependency_path,
			project_root);

		if (!command) {
			fprintf(stderr,
				"Failed to build compile command for %s\n",
				source->relative_path);

			free(command_path);
			free(dependency_path);
			free(object_path);
			object_list_free(objects);
			return NULL;
		}

		if (!create_parent_directories(object_path)) {
			fprintf(stderr,
				"Failed to create parent directories for %s\n",
				object_path);

			free(command);
			free(command_path);
			free(dependency_path);
			free(object_path);
			object_list_free(objects);
			return NULL;
		}

		if (should_compile(object_path, dependency_path, command_path,
				   command)) {
			const int result = compile_source(
				manifest, target, source, object_path,
				dependency_path, project_root);

			if (result != 0) {
				fprintf(stderr,
					"Failed to compile %s (exit code %d)\n",
					source->relative_path, result);

				free(command);
				free(command_path);
				free(dependency_path);
				free(object_path);
				object_list_free(objects);
				return NULL;
			}

			if (!command_write(command_path, command)) {
				fprintf(stderr,
					"Failed to write compile state for "
					"%s\n",
					source->relative_path);

				free(command);
				free(command_path);
				free(dependency_path);
				free(object_path);
				object_list_free(objects);
				return NULL;
			}
		}

		if (!object_list_add(objects, object_path)) {
			fprintf(stderr, "Failed to add object %s\n",
				object_path);

			free(command);
			free(command_path);
			free(dependency_path);
			free(object_path);
			object_list_free(objects);
			return NULL;
		}

		free(command);
		free(command_path);
		free(dependency_path);
		free(object_path);
	}

	return objects;
}

void object_list_free(ObjectList *objects) {
	if (!objects) return;

	for (size_t i = 0; i < objects->count; i++)
		free(objects->files[i]);

	free(objects->files);
	free(objects);
}