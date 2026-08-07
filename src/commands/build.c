/*
 * Copyright (C) 2026 tas0dev
 * This software is licensed under the GNU General Public License, version 3
 * only.
 *
 * Created by tas0dev
 */

#include "build.h"
#include "app.h"
#include "build/manifest.h"
#include "build/project.h"
#include "build/source.h"
#include "src/cli.h"
#include <stdio.h>
#include <stdlib.h>
#ifndef _WIN32
#include <direct.h>
#define getcwd _getcwd
#else
#include <unistd.h>
#endif

int run_build(const int argc, char **argv) {
	(void)argc;
	(void)argv;

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
		fprintf(
			stderr,
			RED "Failed to load manifest" RESET);

		if (error.message) fprintf(stderr, ": %s", error.message);

		if (error.line != 0) {
			fprintf(stderr, " (%zu:%zu)", error.line, error.column);
		}

		fputc('\n', stderr);

		free(project_root);
		return 1;
	}

	SourceList *sources = source_collect(manifest, project_root);

	if (!sources) {
		fprintf(stderr, RED "Failed to collect sources\n" RESET);

		manifest_free(manifest);
		free(project_root);
		return 1;
	}

	printf("project: %s\n", manifest->name);
	printf("root: %s\n", project_root);

	printf("sources:\n");

	for (size_t i = 0; i < sources->count; i++)
		printf("\t%s\n", sources->files[i]);

	for (size_t i = 0; i < sources->count; i++) {
		printf("\t%s -> %s\n", sources->files[i].path,
		       sources->files[i].relative_path);
	}

	source_list_free(sources);
	manifest_free(manifest);
	free(project_root);

	return 0;
}