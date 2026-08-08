/*
 * Copyright (C) 2026 tas0dev
 * This software is licensed under the GNU General Public License, version 3
 * only.
 *
 * Created by tas0dev
 */

#include "commands/install.h"
#include "app.h"
#include "build/manifest.h"
#include "build/profile.h"
#include "build/project.h"
#include "cli.h"
#include "commands/build.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define getcwd _getcwd
#else
#include <sys/stat.h>
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

static int parse_install_arguments(const int argc,
				   char **argv,
				   const char **target_name,
				   BuildProfile *profile) {
	*target_name = NULL;
	*profile = Release;

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

static BuildTarget *find_install_target(const Manifest *manifest) {
	BuildTarget *result = NULL;

	for (size_t i = 0; i < manifest->target_count; i++) {
		BuildTarget *target = &manifest->targets[i];

		if (target->target_type != Executable) continue;

		if (result) {
			fprintf(stderr,
				RED "Multiple executable targets found. " RESET
				    "Specify a target with "
				    "'craft install <target>'\n");

			return NULL;
		}

		result = target;
	}

	if (!result) {
		fprintf(stderr, RED "No executable target found\n" RESET);
	}

	return result;
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
	const size_t name_length =
		strlen(target->name) +
		strlen(".exe") + 1;

	char *name = malloc(name_length);

	if (!name) {
		fprintf(stderr, "Failed to allocate artifact name\n");

		free(profile_root);
		return NULL;
	}

	snprintf(name, name_length, "%s.exe", target->name
	);
#else
	char *name = malloc(strlen(target->name) + 1);

	if (!name) {
		fprintf(stderr, "Failed to allocate artifact name\n");

		free(profile_root);
		return NULL;
	}

	strcpy(name, target->name);
#endif

	char *path = path_join(profile_root,
			name
		);

	free(name);
	free(profile_root);

	return path;
}

#ifndef _WIN32

static int create_directory(const char *path) {
	if (mkdir(path, 0755) == 0) return 1;

	return errno == EEXIST;
}

static char *get_install_directory(void) {
	const char *home = getenv("HOME");

	if (!home) {
		fprintf(stderr, "HOME is not set\n");

		return NULL;
	}

	char *local = path_join(home, ".local");

	if (!local) return NULL;

	if (!create_directory(local)) {
		fprintf(stderr, "Failed to create %s: %s\n", local,
			strerror(errno));

		free(local);
		return NULL;
	}

	char *bin = path_join(local, "bin");

	free(local);

	if (!bin) return NULL;

	if (!create_directory(bin)) {
		fprintf(stderr, "Failed to create %s: %s\n", bin,
			strerror(errno));

		free(bin);
		return NULL;
	}

	return bin;
}

#else

static char *get_install_directory(void) {
	const char *local_app_data = getenv("LOCALAPPDATA");

	if (!local_app_data) {
		fprintf(stderr,
			"LOCALAPPDATA is not set\n");

		return NULL;
	}

	char *microsoft = path_join(local_app_data,
			"Microsoft");

	if (!microsoft) return NULL;

	char *windows_apps = path_join(
			microsoft,
			"WindowsApps");

	free(microsoft);

	return windows_apps;
}

#endif

static char *get_install_path(const BuildTarget *target) {
	char *directory = get_install_directory();

	if (!directory) return NULL;

#ifdef _WIN32
	const size_t name_length = strlen(target->name) + strlen(".exe") + 1;

	char *name = malloc(name_length);

	if (!name) {
		fprintf(stderr, "Failed to allocate install name\n");

		free(directory);
		return NULL;
	}

	snprintf(name, name_length, "%s.exe", target->name);
#else
	char *name = malloc(strlen(target->name) + 1);

	if (!name) {
		fprintf(stderr, "Failed to allocate install name\n");

		free(directory);
		return NULL;
	}

	strcpy(name, target->name);
#endif

	char *path = path_join(directory, name);

	free(name);
	free(directory);

	return path;
}

static int copy_file(const char *source, const char *destination) {
	FILE *input = fopen(source, "rb");

	if (!input) {
		fprintf(stderr, "Failed to open %s: %s\n", source,
			strerror(errno));

		return 0;
	}

	FILE *output = fopen(destination, "wb");

	if (!output) {
		fprintf(stderr, "Failed to open %s: %s\n", destination,
			strerror(errno));

		fclose(input);
		return 0;
	}

	char buffer[16384];

	size_t size;

	while (
		(size = fread(buffer, 1, sizeof(buffer),
			input
		)) != 0
	) {
		if (fwrite(buffer, 1, size, output) != size
		) {
			fprintf(stderr,
				"Failed to write %s\n",
				destination);

			fclose(output);
			fclose(input);

			return 0;
		}
	}

	if (ferror(input)) {
		fprintf(stderr, "Failed to read %s\n", source);

		fclose(output);
		fclose(input);

		return 0;
	}

	fclose(output);
	fclose(input);

#ifndef _WIN32
	if (chmod(destination, 0755) != 0) {
		fprintf(stderr,
			"Failed to make %s executable: %s\n",
			destination,
			strerror(errno));

		return 0;
	}
#endif

	return 1;
}

int run_install(const int argc, char **argv) {
	const char *target_name = NULL;
	BuildProfile profile = Release;

	if (!parse_install_arguments(argc, argv, &target_name, &profile)) {
		return 1;
	}

	char cwd[4096];

	if (!getcwd(cwd, sizeof(cwd))) {
		fprintf(stderr,
			RED "Failed to get current directory\n" RESET);

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

	BuildTarget *target = NULL;

	if (target_name) {
		target = manifest_find_target(manifest, target_name);

		if (!target) {
			fprintf(stderr, RED "Unknown target: " RESET "%s\n",
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
		target = find_install_target(manifest);

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

	char *source = artifact_path(target, project_root, profile);

	if (!source) {
		manifest_free(manifest);
		free(project_root);

		return 1;
	}

	char *destination = get_install_path(target);

	if (!destination) {
		free(source);
		manifest_free(manifest);
		free(project_root);

		return 1;
	}

	if (!copy_file(source, destination)) {
		free(destination);
		free(source);
		manifest_free(manifest);
		free(project_root);

		return 1;
	}

	printf(GREEN "Installed " RESET "%s " GREEN "to " RESET "%s\n",
	       target->name, destination);

	free(destination);
	free(source);
	manifest_free(manifest);
	free(project_root);

	return 0;
}