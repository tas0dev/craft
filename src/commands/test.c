/*
 * Copyright (C) 2026 tas0dev
 * This software is licensed under the GNU General Public License, version 3
 * only.
 *
 * Created by tas0dev
 */

#include "test.h"

#include "build/compiler.h"
#include "build/linker.h"
#include "build/manifest.h"
#include "build/profile.h"
#include "build/project.h"
#include "build/source.h"
#include "cli.h"
#include "util/process.h"
#include "util/thread.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define getcwd _getcwd
#else
#include <unistd.h>
#endif

static char *test_name(const SourceFile *source) {
	const char *filename = strrchr(source->relative_path, '/');

	if (filename)
		filename++;
	else
		filename = source->relative_path;

	const char *extension = strrchr(filename, '.');

	const size_t length =
		extension ? (size_t)(extension - filename) : strlen(filename);

	char *name = malloc(length + 1);

	if (!name) return NULL;

	memcpy(name, filename, length);
	name[length] = '\0';

	return name;
}

static char *
artifact_path(const char *root, const char *name, const BuildProfile profile) {
	const char *profile_name = build_profile_name(profile);

#ifdef _WIN32
	const size_t length = strlen(root) + strlen("/target/") +
			      strlen(profile_name) + 1 + strlen(name) +
			      strlen(".exe") + 1;
#else
	const size_t length = strlen(root) + strlen("/target/") +
			      strlen(profile_name) + 1 + strlen(name) + 1;
#endif

	char *path = malloc(length);

	if (!path) return NULL;

#ifdef _WIN32
	snprintf(path, length, "%s/target/%s/%s.exe", root, profile_name, name);
#else
	snprintf(path, length, "%s/target/%s/%s", root, profile_name, name);
#endif

	return path;
}

int run_tests(int argc, char **argv) {
	(void)argc;
	(void)argv;

	char cwd[4096];

	if (!getcwd(cwd, sizeof(cwd))) {
		fprintf(stderr, RED "Failed to run tests: failed to get "
				    "current directory\n" RESET);

		return 1;
	}

	char *root = project_find_root(cwd);

	if (!root) {
		fprintf(stderr, RED "Could not find craft.toml\n" RESET);

		return 1;
	}

	ManifestError error = {0};
	Manifest *manifest = manifest_load(root, &error);

	if (!manifest) {
		fprintf(stderr, RED "Failed to load manifest" RESET);

		if (error.message) fprintf(stderr, ": %s", error.message);

		if (error.line != 0) {
			fprintf(stderr, " (%zu:%zu)", error.line, error.column);
		}

		fputc('\n', stderr);

		free(root);
		return 1;
	}

	char *source_dirs[] = {"tests"};

	BuildTarget collect_target = {0};

	collect_target.target_type = Executable;
	collect_target.source_dirs = source_dirs;
	collect_target.source_dir_count = 1;

	SourceList *sources = source_collect(&collect_target, root);

	if (!sources) {
		manifest_free(manifest);
		free(root);

		return 1;
	}

	if (sources->count == 0) {
		printf("No tests found\n");

		source_list_free(sources);
		manifest_free(manifest);
		free(root);

		return 0;
	}

	const BuildProfile profile = Debug;
	const unsigned long long jobs = thread_cpu_count();

	int result = 0;

	for (size_t i = 0; i < sources->count; i++) {
		SourceFile *file = &sources->files[i];

		char *name = test_name(file);

		if (!name) {
			result = 1;
			continue;
		}

		BuildTarget target = {0};

		target.name = name;
		target.target_type = Executable;

		if (manifest->target_count > 0) {
			BuildTarget *base = &manifest->targets[0];

			target.include_dirs = base->include_dirs;
			target.include_dir_count = base->include_dir_count;

			target.cflags = base->cflags;
			target.cflags_count = base->cflags_count;

			target.ldflags = base->ldflags;
			target.ldflags_count = base->ldflags_count;

			target.dependencies = base->dependencies;
			target.dependency_count = base->dependency_count;

			target.linker_script = base->linker_script;
		}

		SourceList test_sources = {.files = file, .count = 1};

		ObjectList *objects =
			compile_sources(manifest, &target, &test_sources, root,
					profile, 0, jobs);

		if (!objects) {
			fprintf(stderr,
				RED "Failed to compile test target: " RESET
				    "%s\n",
				name);

			result = 1;
			free(name);

			continue;
		}

		if (!link_objects(manifest, &target, objects, root, profile,
				  0)) {
			fprintf(stderr,
				RED "Failed to link test target: " RESET "%s\n",
				name);

			object_list_free(objects);

			result = 1;
			free(name);

			continue;
		}

		object_list_free(objects);

		char *artifact = artifact_path(root, name, profile);

		if (!artifact) {
			result = 1;
			free(name);

			continue;
		}

		const char *run_argv[] = {artifact, NULL};

		const int test_result = process_run(artifact, run_argv);

		if (test_result != 0) result = 1;

		free(artifact);
		free(name);
	}

	source_list_free(sources);
	manifest_free(manifest);
	free(root);

	return result;
}