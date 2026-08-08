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
			   const char *project_root) {
	char *target_root = path_join(project_root, "target");

	if (!target_root) return NULL;

#ifdef _WIN32
	const size_t name_length = strlen(target->name) + strlen(".exe") + 1;

	char *name = malloc(name_length);

	if (!name) {
		fprintf(stderr, "Failed to allocate artifact name\n");

		free(target_root);
		return NULL;
	}

	snprintf(name, name_length, "%s.exe", target->name);
#else
	char *name = malloc(strlen(target->name) + 1);

	if (!name) {
		fprintf(stderr, "Failed to allocate artifact name\n");

		free(target_root);
		return NULL;
	}

	strcpy(name, target->name);
#endif

	char *path = path_join(target_root, name);

	free(name);
	free(target_root);

	return path;
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
		fprintf(stderr, RED "Failed to get current directory\n" RESET);

		return 1;
	}

	char *project_root = project_find_root(cwd);

	if (!project_root) {
		fprintf(stderr, RED "Could not find " MANIFEST_FILE "\n" RESET);

		return 1;
	}

	ManifestError error = {0};

	Manifest *manifest = manifest_load(project_root, &error);

	if (!manifest) {
		fprintf(stderr, RED "Failed to load manifest" RESET);

		if (error.message) fprintf(stderr, ": %s", error.message);

		fputc('\n', stderr);

		free(project_root);
		return 1;
	}

	BuildTarget *target = NULL;

	if (argc >= 3) {
		target = manifest_find_target(manifest, argv[2]);

		if (!target) {
			fprintf(stderr, RED "Unknown target: " RESET "%s\n",
				argv[2]);

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

	char *build_argv[] = {argv[0], "build", target->name, NULL};

	if (run_build(3, build_argv) != 0) {
		manifest_free(manifest);
		free(project_root);

		return 1;
	}

	char *artifact = artifact_path(target, project_root);

	if (!artifact) {
		manifest_free(manifest);
		free(project_root);

		return 1;
	}

	const char *run_argv[] = {artifact, NULL};

	const int result = process_run(artifact, run_argv);

	if (result == -1) {
		fprintf(stderr, RED "Failed to run %s\n" RESET, target->name);

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