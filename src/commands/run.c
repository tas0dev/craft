/*
 * Copyright (C) 2026 tas0dev
 * This software is licensed under the GNU General Public License, version 3
 * only.
 *
 * Created by tas0dev
 */

#include "commands/run.h"
#include "app.h"
#include "build/manifest.h"
#include "build/profile.h"
#include "build/project.h"
#include "cli.h"
#include "commands/build.h"
#include "util/process.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define getcwd _getcwd
#else
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

static char *artifact_path(const BuildTarget *target,
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

	snprintf(name, name_length,
		"%s.exe", target->name);
#else
	char *name = malloc(strlen(target->name) + 1
		);

	if (!name) {
		fprintf(stderr, "Failed to allocate artifact name\n");

		free(profile_root);
		return NULL;
	}

	strcpy(name, target->name);
#endif

	char *path =
		path_join(profile_root, name);

	free(name);
	free(profile_root);

	return path;
}

static int parse_run_arguments(const int argc,
			       char **argv,
			       const char **target_name,
			       BuildProfile *profile,
			       int *argument_start) {
	*target_name = NULL;
	*profile = Debug;
	*argument_start = argc;

	for (int i = 2; i < argc; i++) {
		if (strcmp(argv[i], "--") == 0) {
			*argument_start = i + 1;
			return 1;
		}

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

static BuildTarget *find_runnable_target(const Manifest *manifest) {
	BuildTarget *result = NULL;

	for (size_t i = 0; i < manifest->target_count; i++) {
		BuildTarget *target = &manifest->targets[i];

		if (target->target_type != Executable) continue;

		if (result) {
			fprintf(stderr,
				"Multiple executable targets found. "
				"Specify a target with 'craft run <target>'\n");

			return NULL;
		}

		result = target;
	}

	if (!result) { fprintf(stderr, "No executable target found\n"); }

	return result;
}

int run_run(const int argc, char **argv) {
	char cwd[4096];

	if (!getcwd(cwd, sizeof(cwd))) {
		fprintf(stderr,
			RED "Failed to get current directory\n" RESET);

		return 1;
	}

	const char *target_name = NULL;
	BuildProfile profile = Debug;
	int argument_start = argc;

	if (!parse_run_arguments(argc, argv, &target_name, &profile,
				 &argument_start)) {
		return 1;
	}

	char *project_root = project_find_root(cwd);

	if (!project_root) {
		fprintf(stderr,
			RED "Could not find " MANIFEST_FILE "\n" RESET);

		return 1;
	}

	ManifestError error = {0};

	Manifest *manifest =
		manifest_load(project_root, &error);

	if (!manifest) {
		fprintf(stderr,
			RED "Failed to load manifest" RESET);

		if (error.message) fprintf(stderr, ": %s", error.message);

		if (error.line != 0) {
			fprintf(stderr,
				" (%zu:%zu)",
				error.line, error.column);
		}

		fputc('\n', stderr);

		free(project_root);
		return 1;
	}

	BuildTarget *target = NULL;

	if (target_name) {
		target =
			manifest_find_target(
				manifest,
				target_name
			);

		if (!target) {
			fprintf(stderr,
				RED "Unknown target: " RESET "%s\n",
				target_name);

			manifest_free(manifest);
			free(project_root);

			return 1;
		}

		if (target->target_type != Executable) {
			fprintf(stderr,
				RED "Target is not executable: " RESET "%s\n",
				target->name);

			manifest_free(manifest);
			free(project_root);

			return 1;
		}
	} else {
		target = find_runnable_target(manifest);

		if (!target) {
			manifest_free(manifest);
			free(project_root);

			return 1;
		}
	}

	char *build_argv[5];
	int build_argc = 0;

	build_argv[build_argc++] = argv[0];
	build_argv[build_argc++] = "build";
	build_argv[build_argc++] = target->name;

	if (profile == Release)
		build_argv[build_argc++] = "--release";
	else
		build_argv[build_argc++] = "--debug";

	build_argv[build_argc] = NULL;

	if (run_build(build_argc, build_argv) != 0) {
		manifest_free(manifest);
		free(project_root);

		return 1;
	}

	char *artifact =
		artifact_path(
			target,
			project_root, profile);

	if (!artifact) {
		manifest_free(manifest);
		free(project_root);

		return 1;
	}

	const int forwarded_count =
		argc - argument_start;

	const size_t run_argument_count = 1 + (size_t)forwarded_count + 1;

	const char **run_argv = calloc(run_argument_count, sizeof(char *));

	if (!run_argv) {
		fprintf(stderr, RED "Failed to allocate run arguments\n" RESET);

		free(artifact);
		manifest_free(manifest);
		free(project_root);

		return 1;
	}

	size_t run_argument_index = 0;

	run_argv[run_argument_index++] = artifact;

	for (int i = argument_start; i < argc; i++) {
		run_argv[run_argument_index++] = argv[i];
	}

	run_argv[run_argument_index] = NULL;

	const int result = process_run(artifact, run_argv);

	free(run_argv);

	if (result == -1) {
		fprintf(stderr,
			RED "Failed to run %s\n" RESET,
			target->name);

		free(artifact);
		manifest_free(manifest);
		free(project_root);

		return 1;
	}

	free(artifact);
	manifest_free(manifest);
	free(project_root);

	return result;
}