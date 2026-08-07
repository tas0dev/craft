/*
 * Copyright (C) 2026 tas0dev
 * This software is licensed under the GNU General Public License, version 3
 * only.
 *
 * Created by tas0dev
 */

#include "build.h"
#include "build/manifest.h"
#include "src/cli.h"
#include <stdio.h>
#ifndef _WIN32
#include <direct.h>
#define getcwd _getcwd
#else
#include <unistd.h>
#endif

static void print_manifest(const Manifest *manifest) {
	printf("project: %s\n", manifest->name);
	printf("source dirs:\n");

	for (size_t i = 0; i < manifest->source_dir_count; i++) {
		printf("\t%s\n", manifest->source_dirs[i]);
	}
}

int run_build(const int argc, char **argv) {
	(void)argc;
	(void)argv;

	char cwd[4096];

	if (!getcwd(cwd, sizeof(cwd))) {
		fprintf(stderr, RED "Failed to get current directory" RESET);

		return 1;
	}

	ManifestError error = {0};
	Manifest *manifest = manifest_load(cwd, &error);

	if (!manifest) {
		fprintf(stderr, RED "Failed to load manifest" RESET);

		if (error.message) fprintf(stderr, ": %s", error.message);
		if (error.line != 0)
			fprintf(stderr, " (%zu:%zu", error.line, error.column);

		fputc('\n', stderr);

		return 1;
	}

	print_manifest(manifest);
	manifest_free(manifest);

	return 0;
}