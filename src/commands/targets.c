/*
 * Copyright (C) 2026 tas0dev
 * This software is licensed under the GNU General Public License, version 3
 * only.
 *
 * Created by tas0dev
 */

#include "commands/targets.h"
#include "app.h"
#include "build/manifest.h"
#include "build/project.h"
#include "cli.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <direct.h>
#define getcwd _getcwd
#else
#include <unistd.h>
#endif

static const char *target_type_name(const TargetType type) {
	switch (type) {
	case Executable: return "executable";

	case StaticLibrary: return "staticlib";

	case DynamicLibrary: return "dynlib";
	}

	return "unknown";
}

int run_targets(const int argc, char **argv) {
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
		fprintf(stderr, RED "Failed to load manifest" RESET);

		if (error.message) fprintf(stderr, ": %s", error.message);

		if (error.line != 0) {
			fprintf(stderr, " (%zu:%zu)", error.line, error.column);
		}

		fputc('\n', stderr);

		free(project_root);
		return 1;
	}

	printf(BLUE "Targets:\n" RESET);

	for (size_t i = 0; i < manifest->target_count; i++) {
		const BuildTarget *target = &manifest->targets[i];

		printf("\t" GREEN "%s" RESET "\t\t%s\n", target->name,
		       target_type_name(target->target_type));
	}

	manifest_free(manifest);
	free(project_root);

	return 0;
}