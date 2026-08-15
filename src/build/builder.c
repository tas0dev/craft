/*
 * Copyright (C) 2026 tas0dev
 * This software is licensed under the GNU General Public License, version 3
 * only.
 *
 * Created by tas0dev
 */

#include "builder.h"
#include "cli.h"
#include "compiler.h"
#include "linker.h"
#include "manifest.h"
#include "profile.h"
#include "source.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

int build_target(const Manifest *manifest,
		 const BuildTarget *target,
		 const char *project_root,
		 const BuildProfile *profile,
		 const int verbose,
		 const unsigned long long jobs) {
	SourceList *sources = source_collect(target, project_root);

	if (!sources) {
		fprintf(stderr,
			RED "Failed to collect sources for target %s\n" RESET,
			target->name);

		return 1;
	}

	ObjectList *objects =
		compile_sources(manifest, target, sources, project_root,
				*profile, verbose, jobs);

	if (!objects) {
		fprintf(stderr, RED "Failed to compile target %s\n" RESET,
			target->name);

		source_list_free(sources);
		return 1;
	}

	if (!link_objects(manifest, target, objects, project_root, *profile,
			  verbose)) {
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

int build_target_recursive(const Manifest *manifest,
			   const BuildTarget *target,
			   const char *project_root,
			   unsigned char *states,
			   const BuildProfile *profile,
			   const int verbose,
			   const unsigned long long jobs) {
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
					   states, profile, verbose,
					   jobs) != 0) {
			return 1;
		}
	}

	if (build_target(manifest, target, project_root, profile, verbose,
			 jobs) != 0) {
		return 1;
	}

	states[target_index] = 2;

	return 0;
}

char *artifact_path(const BuildTarget *target,
		    const char *project_root,
		    const BuildProfile profile) {
	char *target_root = path_join(project_root, "target");

	if (!target_root) return NULL;

	char *profile_root =
		path_join(target_root, build_profile_name(profile));

	free(target_root);

	if (!profile_root) return NULL;

#ifdef _WIN32
	const size_t name_length = strlen(target->name) + strlen(".exe") + 1;

	char *name = malloc(name_length);

	if (!name) {
		fprintf(stderr, "Failed to allocate artifact name\n");

		free(profile_root);
		return NULL;
	}

	snprintf(name, name_length, "%s.exe", target->name);
#else
	char *name = malloc(strlen(target->name) + 1);

	if (!name) {
		fprintf(stderr, "Failed to allocate artifact name\n");

		free(profile_root);
		return NULL;
	}

	strcpy(name, target->name);
#endif

	char *path = path_join(profile_root, name);

	free(name);
	free(profile_root);

	return path;
}