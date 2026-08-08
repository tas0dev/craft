/*
 * Copyright (C) 2026 tas0dev
 * This software is licensed under the GNU General Public License, version 3
 * only.
 *
 * Created by tas0dev
 */

#include "commands/clean.h"
#include "app.h"
#include "build/manifest.h"
#include "build/profile.h"
#include "build/project.h"
#include "cli.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define getcwd _getcwd
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

static int parse_clean_arguments(const int argc,
				 char **argv,
				 const char **target_name,
				 BuildProfile *profile,
				 int *profile_specified) {
	*target_name = NULL;
	*profile = Debug;
	*profile_specified = 0;

	for (int i = 2; i < argc; i++) {
		if (strcmp(argv[i], "--debug") == 0) {
			if (*profile_specified) {
				fprintf(stderr, RED
					"Multiple profiles specified\n" RESET);

				return 0;
			}

			*profile = Debug;
			*profile_specified = 1;

			continue;
		}

		if (strcmp(argv[i], "--release") == 0) {
			if (*profile_specified) {
				fprintf(stderr, RED
					"Multiple profiles specified\n" RESET);

				return 0;
			}

			*profile = Release;
			*profile_specified = 1;

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

static int remove_file_if_exists(const char *path) {
#ifdef _WIN32
	if (DeleteFileA(path)) return 1;

	const DWORD error = GetLastError();

	if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
		return 1;
	}

	fprintf(stderr, "Failed to remove file: %s\n", path);

	return 0;
#else
	if (remove(path) == 0) return 1;

	if (errno == ENOENT) return 1;

	fprintf(stderr, "Failed to remove file: %s\n", path);

	return 0;
#endif
}

static char *artifact_name(const BuildTarget *target) {
	char *name = NULL;

	switch (target->target_type) {
	case Executable:
#ifdef _WIN32
	{
		const size_t length = strlen(target->name) + strlen(".exe") + 1;

		name = malloc(length);

		if (name) { snprintf(name, length, "%s.exe", target->name); }
	}
#else
		name = malloc(strlen(target->name) + 1);

		if (name) strcpy(name, target->name);
#endif
	break;

	case StaticLibrary: {
		const size_t length =
			strlen("lib") + strlen(target->name) + strlen(".a") + 1;

		name = malloc(length);

		if (name) { snprintf(name, length, "lib%s.a", target->name); }

		break;
	}

	case DynamicLibrary:
#ifdef _WIN32
	{
		const size_t length = strlen(target->name) + strlen(".dll") + 1;

		name = malloc(length);

		if (name) { snprintf(name, length, "%s.dll", target->name); }
	}
#else
	{
		const size_t length = strlen("lib") + strlen(target->name) +
				      strlen(".so") + 1;

		name = malloc(length);

		if (name) { snprintf(name, length, "lib%s.so", target->name); }
	}
#endif
	break;
	}

	if (!name) {
		fprintf(stderr, "Failed to allocate artifact name\n");

		return NULL;
	}

	return name;
}

#ifdef _WIN32

static int remove_directory_recursive(const char *path) {
	char *search_path = path_join(path, "*");

	if (!search_path) return 0;

	WIN32_FIND_DATAA entry;

	HANDLE handle = FindFirstFileA(search_path, &entry);

	free(search_path);

	if (handle == INVALID_HANDLE_VALUE) {
		const DWORD error = GetLastError();

		if (error == ERROR_FILE_NOT_FOUND ||
		    error == ERROR_PATH_NOT_FOUND) {
			return 1;
		}

		fprintf(stderr, "Failed to open directory: %s\n", path);

		return 0;
	}

	do {
		if (strcmp(entry.cFileName, ".") == 0 ||
		    strcmp(entry.cFileName, "..") == 0) {
			continue;
		}

		char *entry_path = path_join(path, entry.cFileName);

		if (!entry_path) {
			FindClose(handle);
			return 0;
		}

		if (entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			if (!remove_directory_recursive(entry_path)) {
				free(entry_path);
				FindClose(handle);
				return 0;
			}
		} else {
			if (!DeleteFileA(entry_path)) {
				fprintf(stderr, "Failed to remove file: %s\n",
					entry_path);

				free(entry_path);
				FindClose(handle);
				return 0;
			}
		}

		free(entry_path);

	} while (FindNextFileA(handle, &entry));

	FindClose(handle);

	if (!RemoveDirectoryA(path)) {
		fprintf(stderr, "Failed to remove directory: %s\n", path);

		return 0;
	}

	return 1;
}

#else

static int remove_directory_recursive(const char *path) {
	DIR *directory = opendir(path);

	if (!directory) return 1;

	struct dirent *entry;

	while ((entry = readdir(directory))) {
		if (strcmp(entry->d_name, ".") == 0 ||
		    strcmp(entry->d_name, "..") == 0) {
			continue;
		}

		char *entry_path = path_join(path, entry->d_name);

		if (!entry_path) {
			closedir(directory);
			return 0;
		}

		struct stat status;

		if (stat(entry_path, &status) != 0) {
			fprintf(stderr, "Failed to stat: %s\n", entry_path);

			free(entry_path);
			closedir(directory);
			return 0;
		}

		if (S_ISDIR(status.st_mode)) {
			if (!remove_directory_recursive(entry_path)) {
				free(entry_path);
				closedir(directory);
				return 0;
			}
		} else {
			if (remove(entry_path) != 0) {
				fprintf(stderr, "Failed to remove file: %s\n",
					entry_path);

				free(entry_path);
				closedir(directory);
				return 0;
			}
		}

		free(entry_path);
	}

	closedir(directory);

	if (rmdir(path) != 0) {
		fprintf(stderr, "Failed to remove directory: %s\n", path);

		return 0;
	}

	return 1;
}

#endif

static int clean_target_profile(const BuildTarget *target,
				const char *project_root,
				const BuildProfile profile) {
	char *target_root =
		path_join(project_root, "target");

	if (!target_root) return 0;

	char *profile_root =
		path_join(target_root, build_profile_name(profile));

	free(target_root);

	if (!profile_root) return 0;

	char *build_root = path_join(profile_root, "build");

	if (!build_root) {
		free(profile_root);
		return 0;
	}

	char *target_build = path_join(build_root, target->name);

	free(build_root);

	if (!target_build) {
		free(profile_root);
		return 0;
	}

	if (!remove_directory_recursive(target_build)) {
		free(target_build);
		free(profile_root);

		return 0;
	}

	free(target_build);

	char *name = artifact_name(target);

	if (!name) {
		free(profile_root);
		return 0;
	}

	char *artifact = path_join(profile_root, name);

	free(name);
	free(profile_root);

	if (!artifact) return 0;

	if (!remove_file_if_exists(artifact)) {
		free(artifact);
		return 0;
	}

	const size_t command_length = strlen(artifact) + strlen(".cmd") + 1;

	char *command_path = malloc(command_length);

	if (!command_path) {
		fprintf(stderr, "Failed to allocate command path\n");

		free(artifact);
		return 0;
	}

	snprintf(command_path, command_length, "%s.cmd", artifact);

	free(artifact);

	if (!remove_file_if_exists(command_path)) {
		free(command_path);
		return 0;
	}

	free(command_path);

	return 1;
}

int run_clean(const int argc, char **argv) {
	const char *target_name = NULL;

	BuildProfile profile = Debug;
	int profile_specified = 0;

	if (!parse_clean_arguments(
		    argc,
		    argv,
		    &target_name,
		    &profile,
		    &profile_specified
	    )) {
		return 1;
	}

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

	if (!target_name) {
		char *target_path = path_join(project_root,
				"target"
			);

		if (!target_path) {
			free(project_root);
			return 1;
		}

		char *clean_path = target_path;

		if (profile_specified) {
			clean_path = path_join(
					target_path,
					       build_profile_name(profile));

			free(target_path);

			if (!clean_path) {
				free(project_root);
				return 1;
			}
		}

		if (!remove_directory_recursive(clean_path)) {
			free(clean_path);
			free(project_root);

			return 1;
		}

		if (profile_specified) {
			printf("Cleaned %s profile\n",
			       build_profile_name(profile));
		} else {
			printf("Cleaned target\n");
		}

		free(clean_path);
		free(project_root);

		return 0;
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

	BuildTarget *target = manifest_find_target(manifest, target_name);

	if (!target) {
		fprintf(stderr, RED "Unknown target: " RESET "%s\n",
			target_name);

		manifest_free(manifest);
		free(project_root);

		return 1;
	}

	if (profile_specified) {
		if (!clean_target_profile(target, project_root, profile)) {
			manifest_free(manifest);
			free(project_root);

			return 1;
		}

		printf("Cleaned %s (%s)\n", target->name,
		       build_profile_name(profile));
	} else {
		if (!clean_target_profile(target, project_root, Debug)) {
			manifest_free(manifest);
			free(project_root);

			return 1;
		}

		if (!clean_target_profile(target, project_root, Release)) {
			manifest_free(manifest);
			free(project_root);

			return 1;
		}

		printf("Cleaned %s\n", target->name);
	}

	manifest_free(manifest);
	free(project_root);

	return 0;
}