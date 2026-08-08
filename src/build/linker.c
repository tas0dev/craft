/*
 * Copyright (C) 2026 tas0dev
 * This software is licensed under the GNU General Public License, version 3
 * only.
 *
 * Created by tas0dev
 */

#include "linker.h"

#include "cli.h"
#include "util/process.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/types.h>
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

static int create_directory(const char *path) {
#ifdef _WIN32
	if (_mkdir(path) == 0) return 1;
#else
	if (mkdir(path, 0755) == 0) return 1;
#endif

	return errno == EEXIST;
}

static char *artifact_path(const BuildTarget *target,
			   const char *project_root) {
	char *target_path = path_join(project_root, "target");

	if (!target_path) return NULL;

	if (!create_directory(target_path)) {
		fprintf(stderr, "Failed to create target directory: %s\n",
			target_path);

		free(target_path);
		return NULL;
	}

	char *artifact_name = NULL;

	switch (target->target_type) {
	case Executable:
#ifdef _WIN32
	{
		const size_t length = strlen(target->name) + strlen(".exe") + 1;

		artifact_name = malloc(length);

		if (artifact_name) {
			snprintf(artifact_name, length, "%s.exe", target->name);
		}
	}
#else
		artifact_name = malloc(strlen(target->name) + 1);

		if (artifact_name) strcpy(artifact_name, target->name);
#endif
	break;

	case StaticLibrary: {
		const size_t length =
			strlen("lib") + strlen(target->name) + strlen(".a") + 1;

		artifact_name = malloc(length);

		if (artifact_name) {
			snprintf(artifact_name, length, "lib%s.a",
				 target->name);
		}
	} break;

	case DynamicLibrary:
#ifdef _WIN32
	{
		const size_t length = strlen(target->name) + strlen(".dll") + 1;

		artifact_name = malloc(length);

		if (artifact_name) {
			snprintf(artifact_name, length, "%s.dll", target->name);
		}
	}
#else
	{
		const size_t length = strlen("lib") + strlen(target->name) +
				      strlen(".so") + 1;

		artifact_name = malloc(length);

		if (artifact_name) {
			snprintf(artifact_name, length, "lib%s.so",
				 target->name);
		}
	}
#endif
	break;
	}

	if (!artifact_name) {
		fprintf(stderr, "Failed to allocate artifact name\n");

		free(target_path);
		return NULL;
	}

	char *path = path_join(target_path, artifact_name);

	free(artifact_name);
	free(target_path);

	return path;
}

static char *command_path_from_artifact(const char *artifact) {
	const size_t length = strlen(artifact) + strlen(".cmd") + 1;

	char *path = malloc(length);

	if (!path) {
		fprintf(stderr, "Failed to allocate linker command path\n");

		return NULL;
	}

	snprintf(path, length, "%s.cmd", artifact);

	return path;
}

static char *build_link_command(const Manifest *manifest,
				const BuildTarget *target,
				const ObjectList *objects,
				const char *artifact,
				const char *project_root) {
	const char *program =
		target->target_type == StaticLibrary ? "ar" : manifest->ld;

	size_t length = strlen(program);

	if (target->target_type == StaticLibrary) {
		length += strlen(" rcs ");
		length += strlen(artifact);
	} else {
		if (target->target_type == DynamicLibrary)
			length += strlen(" -shared");

		for (size_t i = 0; i < target->ldflags_count; i++) {
			length += 1;
			length += strlen(target->ldflags[i]);
		}

		if (target->linker_script) {
			length += strlen(" -T ");
			length += strlen(project_root);
			length += 1;
			length += strlen(target->linker_script);
		}
	}

	for (size_t i = 0; i < objects->count; i++) {
		length += 1 + strlen(objects->files[i]);
	}

	if (target->target_type != StaticLibrary) {
		length += strlen(" -o ");
		length += strlen(artifact);
	}

	char *command = malloc(length + 1);

	if (!command) {
		fprintf(stderr, "Failed to allocate linker command\n");

		return NULL;
	}

	command[0] = '\0';

	strcat(command, program);

	if (target->target_type == StaticLibrary) {
		strcat(command, " rcs ");
		strcat(command, artifact);
	} else if (target->target_type == DynamicLibrary) {
		strcat(command, " -shared");
	}

	for (size_t i = 0; i < objects->count; i++) {
		strcat(command, " ");
		strcat(command, objects->files[i]);
	}

	if (target->target_type != StaticLibrary) {
		for (size_t i = 0; i < target->ldflags_count; i++) {
			strcat(command, " ");
			strcat(command, target->ldflags[i]);
		}

		if (target->linker_script) {
			strcat(command, " -T ");
			strcat(command, project_root);
			strcat(command, "/");
			strcat(command, target->linker_script);
		}

		strcat(command, " -o ");
		strcat(command, artifact);
	}

	return command;
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

	if (!file) {
		fprintf(stderr, "Failed to open linker command file: %s\n",
			command_path);

		return 0;
	}

	const size_t length = strlen(command);

	const size_t written = fwrite(command, 1, length, file);

	fclose(file);

	if (written != length) {
		fprintf(stderr, "Failed to write linker command file: %s\n",
			command_path);

		return 0;
	}

	return 1;
}

static int should_link(const ObjectList *objects,
		       const char *artifact,
		       const char *command_path,
		       const char *command) {
	struct stat artifact_stat;

	if (stat(artifact, &artifact_stat) != 0) return 1;

	if (!command_matches(command_path, command)) { return 1; }

	for (size_t i = 0; i < objects->count; i++) {
		struct stat object_stat;

		if (stat(objects->files[i], &object_stat) != 0) { return 1; }

		if (object_stat.st_mtime > artifact_stat.st_mtime) { return 1; }
	}

	return 0;
}

static int link_artifact(const Manifest *manifest,
			 const BuildTarget *target,
			 const ObjectList *objects,
			 const char *artifact,
			 const char *project_root) {
	if (target->target_type == StaticLibrary) {
		const size_t argument_count = 3 + objects->count + 1;

		const char **argv = calloc(argument_count, sizeof(char *));

		if (!argv) {
			fprintf(stderr,
				"Failed to allocate archiver arguments\n");

			return -1;
		}

		size_t index = 0;

		argv[index++] = "ar";
		argv[index++] = "rcs";
		argv[index++] = artifact;

		for (size_t i = 0; i < objects->count; i++) {
			argv[index++] = objects->files[i];
		}

		argv[index] = NULL;

		const int result = process_run("ar", argv);

		free(argv);

		return result;
	}

	char **dependency_paths =
		calloc(target->dependency_count, sizeof(char *));

	if (target->dependency_count != 0 && !dependency_paths) {
		fprintf(stderr, "Failed to allocate dependency paths\n");

		return -1;
	}

	for (size_t i = 0; i < target->dependency_count; i++) {
		BuildTarget *dependency =
			manifest_find_target(manifest, target->dependencies[i]);

		if (!dependency) {
			fprintf(stderr, "Unknown dependency: %s\n",
				target->dependencies[i]);

			goto error;
		}

		dependency_paths[i] = artifact_path(dependency, project_root);

		if (!dependency_paths[i]) goto error;
	}

	const size_t linker_script_argument_count =
		target->linker_script ? 2 : 0;

	const size_t dynamic_argument_count =
		target->target_type == DynamicLibrary ? 1 : 0;

	const size_t argument_count =
		1 + objects->count + target->dependency_count +
		target->ldflags_count + linker_script_argument_count +
		dynamic_argument_count + 2 + 1;

	const char **argv = calloc(argument_count, sizeof(char *));

	if (!argv) {
		fprintf(stderr, "Failed to allocate linker arguments\n");

		goto error;
	}

	char *linker_script_path = NULL;

	if (target->linker_script) {
		linker_script_path =
			path_join(project_root, target->linker_script);

		if (!linker_script_path) {
			free(argv);
			goto error;
		}
	}

	size_t index = 0;

	argv[index++] = manifest->ld;

	if (target->target_type == DynamicLibrary) argv[index++] = "-shared";

	for (size_t i = 0; i < objects->count; i++) {
		argv[index++] = objects->files[i];
	}

	for (size_t i = 0; i < target->dependency_count; i++) {
		argv[index++] = dependency_paths[i];
	}

	for (size_t i = 0; i < target->ldflags_count; i++) {
		argv[index++] = target->ldflags[i];
	}

	if (linker_script_path) {
		argv[index++] = "-T";
		argv[index++] = linker_script_path;
	}

	argv[index++] = "-o";
	argv[index++] = artifact;
	argv[index] = NULL;

	const int result = process_run(manifest->ld, argv);

	free(linker_script_path);
	free(argv);

	for (size_t i = 0; i < target->dependency_count; i++) {
		free(dependency_paths[i]);
	}

	free(dependency_paths);

	return result;

error:
	for (size_t i = 0; i < target->dependency_count; i++) {
		free(dependency_paths[i]);
	}

	free(dependency_paths);

	return -1;
}

int link_objects(const Manifest *manifest,
		 const BuildTarget *target,
		 const ObjectList *objects,
		 const char *project_root) {
	if (!manifest) {
		fprintf(stderr, "link_objects: manifest is NULL\n");

		return 0;
	}

	if (!target) {
		fprintf(stderr, "link_objects: target is NULL\n");

		return 0;
	}

	if (!objects) {
		fprintf(stderr, "link_objects: objects is NULL\n");

		return 0;
	}

	if (!project_root) {
		fprintf(stderr, "link_objects: project_root is NULL\n");

		return 0;
	}

	if (objects->count == 0) {
		fprintf(stderr, "No object files to link for target %s\n",
			target->name);

		return 0;
	}

	char *artifact = artifact_path(target, project_root);

	if (!artifact) return 0;

	char *command_path = command_path_from_artifact(artifact);

	if (!command_path) {
		free(artifact);
		return 0;
	}

	char *command = build_link_command(manifest, target, objects, artifact,
					   project_root);

	if (!command) {
		free(command_path);
		free(artifact);
		return 0;
	}

	if (should_link(objects, artifact, command_path, command)) {
		printf(BLUE "Linking\t\t\t" RESET "%s\n", target->name);

		const int result = link_artifact(manifest, target, objects,
						 artifact, project_root);

		if (result != 0) {
			fprintf(stderr, "Failed to link %s (exit code %d)\n",
				target->name, result);

			free(command);
			free(command_path);
			free(artifact);

			return 0;
		}

		if (!command_write(command_path, command)) {
			free(command);
			free(command_path);
			free(artifact);

			return 0;
		}
	}

	free(command);
	free(command_path);
	free(artifact);

	return 1;
}