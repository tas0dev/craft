/*
 * Copyright (C) 2026 tas0dev
 * This software is licensed under the GNU General Public License, version 3
 * only.
 *
 * Created by tas0dev
 */

#include "build.h"
#include "app.h"
#include "build/compiler.h"
#include "build/linker.h"
#include "build/manifest.h"
#include "build/profile.h"
#include "build/project.h"
#include "build/source.h"
#include "cli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define getcwd _getcwd
#else
#include <unistd.h>
#endif

static int parse_build_arguments(const int argc,
				 char **argv,
				 const char **target_name,
				 BuildProfile *profile) {
	*target_name = NULL;
	*profile = Debug;

	for (int i = 2; i < argc; i++) {
		if (strcmp(argv[i], "--release") == 0) {
			*profile = Release;
			continue;
		}

		if (strcmp(argv[i], "--debug") == 0) {
			*profile = Debug;
			continue;
		}

		if (argv[i][0] == '-') {
			fprintf(stderr, RED "Unknown option: " RESET "%s\n",
				argv[i]);

			return 0;
		}

		if (*target_name) {
			fprintf(stderr,
				RED "Multiple targets specified\n" RESET);

			return 0;
		}

		*target_name = argv[i];
	}

	return 1;
}

static int build_target(const Manifest *manifest,
			const BuildTarget *target,
			const char *project_root,
			const BuildProfile *profile) {
	SourceList *sources = source_collect(target, project_root);

	if (!sources) {
		fprintf(stderr,
			RED "Failed to collect sources for target %s\n" RESET,
			target->name);

		return 1;
	}

	ObjectList *objects =
		compile_sources(manifest, target, sources,
					      project_root, *profile);

	if (!objects) {
		fprintf(stderr, RED "Failed to compile target %s\n" RESET,
			target->name);

		source_list_free(sources);
		return 1;
	}

	if (!link_objects(manifest, target, objects, project_root, *profile)) {
		fprintf(stderr, RED "Failed to link target %s\n" RESET,
			target->name);

		object_list_free(objects);
		source_list_free(sources);

		return 1;
	}

	object_list_free(objects);
	source_list_free(sources);

	return 0;
}

static int build_target_recursive(const Manifest *manifest,
				  const BuildTarget *target,
				  const char *project_root,
				  unsigned char *states,
				  const BuildProfile *profile) {
	const size_t target_index = (size_t)(target - manifest->targets);

	if (states[target_index] == 2) return 0;

	if (states[target_index] == 1) {
		fprintf(stderr,
			RED "Circular dependency involving target: " RESET
			    "%s\n",
			target->name);

		return 1;
	}

	states[target_index] = 1;

	for (size_t i = 0; i < target->dependency_count; i++) {
		const char *dependency_name = target->dependencies[i];

		BuildTarget *dependency =
			manifest_find_target(manifest, dependency_name);

		if (!dependency) {
			fprintf(stderr, RED "Unknown dependency: " RESET "%s\n",
				dependency_name);

			return 1;
		}

		if (build_target_recursive(manifest, dependency, project_root,
					   states, profile) != 0) {
			return 1;
		}
	}

	if (build_target(manifest, target, project_root, profile) != 0) {
		return 1;
	}

	states[target_index] = 2;

	return 0;
}

int run_build(const int argc, char **argv) {
	char cwd[4096];

	if (!getcwd(cwd, sizeof(cwd))) {
		fprintf(stderr, RED "Failed to get current directory\n" RESET);

		return 1;
	}

	const char *target_name = NULL;
	BuildProfile profile = Debug;

	if (!parse_build_arguments(argc, argv, &target_name, &profile)) {
		return 1;
	}

	char *project_root = project_find_root(cwd);

	if (!project_root) {
		fprintf(stderr, RED "Could not find " MANIFEST_FILE "\n" RESET);

		return 1;
	}

	ManifestError error = {0};

	Manifest *manifest =
		manifest_load(project_root, &error);

	if (!manifest) {
		fprintf(stderr, RED "Failed to load manifest" RESET);

		if (error.message)
			fprintf(stderr, ": %s", error.message);

		if (error.line != 0) {
			fprintf(stderr,
				" (%zu:%zu)",
				error.line, error.column);
		}

		fputc('\n', stderr);

		free(project_root);
		return 1;
	}

	unsigned char *states =
		calloc(
			manifest->target_count,
			sizeof(unsigned char));

	if (!states) {
		fprintf(stderr, RED "Failed to allocate build state\n" RESET);

		manifest_free(manifest);
		free(project_root);

		return 1;
	}

	if (target_name) {
		BuildTarget *target =
			manifest_find_target(manifest, target_name);

		if (!target) {
			fprintf(stderr, RED "Unknown target: " RESET "%s\n",
				target_name
			);

			free(states);
			manifest_free(manifest);
			free(project_root);

			return 1;
		}

		const int result =
			build_target_recursive(
				manifest,
				target,
				project_root, states, &profile);

		free(states);
		manifest_free(manifest);
		free(project_root);

		return result;
	}

	for (
		size_t i = 0;
		i < manifest->target_count;
		i++
	) {
		if (
			build_target_recursive(
				manifest, &manifest->targets[i],
					   project_root, states,
					   &profile) != 0) {
			free(states);
			manifest_free(manifest);
			free(project_root);

			return 1;
		}
	}

	free(states);
	manifest_free(manifest);
	free(project_root);

	return 0;
}