/*
 * Copyright (C) 2026 tas0dev
 * This software is licensed under the GNU General Public License, version 3
 * only.
 *
 * Created by tas0dev
 */

#include "manifest.h"
#include "app.h"
#include "toml/toml.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Manifest *manifest_load(const char *path, ManifestError *error) {
	TomlError toml_error;

	if (error) {
		error->line = 0;
		error->column = 0;
		error->message = NULL;
	}

	const size_t length = strlen(path) + 1 + strlen(MANIFEST_FILE) + 1;

	char *manifest_file = malloc(length);

	if (!manifest_file) {
		fprintf(stderr, "Failed to allocate manifest path\n");
		return NULL;
	}

	snprintf(manifest_file, length, "%s/%s", path, MANIFEST_FILE);

	TomlDocument *document = toml_parse_file(manifest_file, &toml_error);

	free(manifest_file);

	if (!document) {
		if (error) {
			error->line = toml_error.line;
			error->column = toml_error.column;
			error->message = toml_error.message;
		}

		return NULL;
	}

	const char *name = toml_get_string(document, "project.name");

	if (!name) {
		if (error) {
			error->line = 0;
			error->column = 0;
			error->message = "project.name is required";
		}

		toml_free(document);
		return NULL;
	}

	Manifest *manifest = calloc(1, sizeof(*manifest));

	if (!manifest) {
		fprintf(stderr, "Failed to allocate manifest\n");
		toml_free(document);
		return NULL;
	}

	manifest->name =
		malloc(strlen(name) + 1);

	if (!manifest->name) {
		fprintf(stderr, "Failed to allocate project name\n");
		manifest_free(manifest);
		toml_free(document);
		return NULL;
	}

	strcpy(manifest->name, name);

	const char *target_type = toml_get_string(document, "target.type");

	if (!target_type) {
		manifest->target_type = Executable;
	} else if (strcmp(target_type, "executable") == 0) {
		manifest->target_type = Executable;
	} else if (strcmp(target_type, "staticlib") == 0) {
		manifest->target_type = StaticLibrary;
	} else if (strcmp(target_type, "dynlib") == 0) {
		manifest->target_type = DynamicLibrary;
	} else {
		if (error) {
			error->line = 0;
			error->column = 0;
			error->message = "target.type must be executable, "
					 "staticlib or dynlib";
		}

		manifest_free(manifest);
		toml_free(document);
		return NULL;
	}

	const char *cc = toml_get_string(document, "toolchain.cc");

	if (!cc)
		cc = "cc";

	manifest->cc =
		malloc(strlen(cc) + 1);

	if (!manifest->cc) {
		fprintf(stderr, "Failed to allocate compiler name\n");
		manifest_free(manifest);
		toml_free(document);
		return NULL;
	}

	strcpy(manifest->cc, cc);

	const char *ld =
		toml_get_string(document, "toolchain.ld");

	if (!ld)
		ld = "cc";

	manifest->ld =
		malloc(strlen(ld) + 1);

	if (!manifest->ld) {
		fprintf(stderr, "Failed to allocate linker name\n");
		manifest_free(manifest);
		toml_free(document);
		return NULL;
	}

	strcpy(manifest->ld, ld);

	const TomlArray *source_dirs =
		toml_get_array(
			document,
			"target.source_dirs"
		);

	if (source_dirs) {
		manifest->source_dir_count =
			toml_array_length(source_dirs);

		manifest->source_dirs = calloc(
			manifest->source_dir_count, sizeof(char *));

		if (!manifest->source_dirs) {
			fprintf(stderr, "Failed to allocate source directories\n");
			manifest_free(manifest);
			toml_free(document);
			return NULL;
		}

		for (
			size_t i = 0;
			i < manifest->source_dir_count;
			i++
		) {
			const TomlValue *value = toml_array_get(source_dirs, i);

			const char *source_dir = toml_string(value);

			if (!source_dir) {
				if (error) {
					error->line = 0;
					error->column = 0;
					error->message =
						"target.source_dirs must contain strings";
				}

				manifest_free(manifest);
				toml_free(document);
				return NULL;
			}

			manifest->source_dirs[i] =
				malloc(strlen(source_dir) + 1);

			if (!manifest->source_dirs[i]) {
				fprintf(stderr, "Failed to allocate source directory\n");
				manifest_free(manifest);
				toml_free(document);
				return NULL;
			}

			strcpy(
				manifest->source_dirs[i],
				source_dir
			);
		}
	} else {
		manifest->source_dir_count = 1;

		manifest->source_dirs =
			calloc(1, sizeof(char *));

		if (!manifest->source_dirs) {
			fprintf(stderr,
				"Failed to allocate source directories\n");
			manifest_free(manifest);
			toml_free(document);
			return NULL;
		}

		manifest->source_dirs[0] =
			malloc(sizeof("src"));

		if (!manifest->source_dirs[0]) {
			fprintf(stderr, "Failed to allocate default source "
					"directory\n");
			manifest_free(manifest);
			toml_free(document);
			return NULL;
		}

		strcpy(
			manifest->source_dirs[0],
			"src"
		);
	}

	const TomlArray *include_dirs =
		toml_get_array(
			document,
			"target.include_dirs");

	if (include_dirs) {
		manifest->include_dir_count =
			toml_array_length(include_dirs);

		manifest->include_dirs =
			calloc(manifest->include_dir_count, sizeof(char *));

		if (!manifest->include_dirs) {
			fprintf(stderr,
				"Failed to allocate include directories\n");
			manifest_free(manifest);
			toml_free(document);
			return NULL;
		}

		for (
			size_t i = 0;
			i < manifest->include_dir_count;
			i++
		) {
			const TomlValue *value =
				toml_array_get(include_dirs, i);

			const char *include_dir =
				toml_string(value);

			if (!include_dir) {
				if (error) {
					error->line = 0;
					error->column = 0;
					error->message =
						"target.include_dirs must contain strings";
				}

				manifest_free(manifest);
				toml_free(document);
				return NULL;
			}

			manifest->include_dirs[i] =
				malloc(strlen(include_dir) + 1);

			if (!manifest->include_dirs[i]) {
				fprintf(stderr, "Failed to allocate include "
						"directory\n");
				manifest_free(manifest);
				toml_free(document);
				return NULL;
			}

			strcpy(
				manifest->include_dirs[i],
				include_dir
			);
		}
	} else {
		manifest->include_dir_count = 2;

		manifest->include_dirs =
			calloc(2, sizeof(char *));

		if (!manifest->include_dirs) {
			fprintf(stderr, "Failed to allocate include directories\n");
			manifest_free(manifest);
			toml_free(document);
			return NULL;
		}

		manifest->include_dirs[0] =
			malloc(sizeof("src"));

		manifest->include_dirs[1] =
			malloc(sizeof("include"));

		if (
			!manifest->include_dirs[0] ||
			!manifest->include_dirs[1]) {
			fprintf(stderr, "Failed to allocate default include "
					"directories\n");
			manifest_free(manifest);
			toml_free(document);
			return NULL;
		}

		strcpy(
			manifest->include_dirs[0],
			"src");

		strcpy(manifest->include_dirs[1],
			"include"
		);
	}

	const TomlArray *cflags =
		toml_get_array(document, "target.cflags");

	if (cflags) {
		manifest->cflags_count = toml_array_length(cflags);

		manifest->cflags = calloc(
			manifest->cflags_count, sizeof(char *));

		if (!manifest->cflags) {
			fprintf(stderr, "Failed to allocate cflags\n");
			manifest_free(manifest);
			toml_free(document);
			return NULL;
		}

		for (
			size_t i = 0;
			i < manifest->cflags_count;
			i++) {
			const TomlValue *value = toml_array_get(cflags, i);

			const char *cflag =
				toml_string(value);

			if (!cflag) {
				if (error) {
					error->line = 0;
					error->column = 0;
					error->message =
						"target.cflags must "
							 "contain strings";
				}

				manifest_free(manifest);
				toml_free(document);
				return NULL;
			}

			manifest->cflags[i] =
				malloc(strlen(cflag) + 1);

			if (!manifest->cflags[i]) {
				fprintf(stderr, "Failed to allocate cflag\n");
				manifest_free(manifest);
				toml_free(document);
				return NULL;
			}

			strcpy(
				manifest->cflags[i],
				cflag
			);
		}
	}

	const TomlArray *ldflags = toml_get_array(document, "target.ldflags");

	if (ldflags) {
		manifest->ldflags_count = toml_array_length(ldflags);

		manifest->ldflags =
			calloc(manifest->ldflags_count, sizeof(char *));

		if (!manifest->ldflags) {
			fprintf(stderr, "Failed to allocate ldflags\n");
			manifest_free(manifest);
			toml_free(document);
			return NULL;
		}

		for (
			size_t i = 0;
			i < manifest->ldflags_count;
			i++
		) {
			const TomlValue *value = toml_array_get(ldflags, i);

			const char *ldflag = toml_string(value);

			if (!ldflag) {
				if (error) {
					error->line = 0;
					error->column = 0;
					error->message =
						"target.ldflags must contain strings";
				}

				manifest_free(manifest);
				toml_free(document);
				return NULL;
			}

			manifest->ldflags[i] =
				malloc(strlen(ldflag) + 1);

			if (!manifest->ldflags[i]) {
				fprintf(stderr, "Failed to allocate ldflag\n");
				manifest_free(manifest);
				toml_free(document);
				return NULL;
			}

			strcpy(
				manifest->ldflags[i],
				ldflag
			);
		}
	}

	const char *linker_script =
		toml_get_string(document, "target.linker_script"
		);

	if (linker_script) {
		manifest->linker_script = malloc(strlen(linker_script) + 1);

		if (!manifest->linker_script) {
			fprintf(stderr, "Failed to allocate linker script\n");
			manifest_free(manifest);
			toml_free(document);
			return NULL;
		}

		strcpy(manifest->linker_script,
			linker_script
		);
	}

	toml_free(document);

	return manifest;
}

void manifest_free(Manifest *manifest) {
	if (!manifest) return;

	free(manifest->name);
	free(manifest->cc);
	free(manifest->ld);
	free(manifest->linker_script);

	for (
		size_t i = 0;
		i < manifest->source_dir_count;
		i++
	) {
		free(manifest->source_dirs[i]);
	}

	free(manifest->source_dirs);

	for (
		size_t i = 0;
		i < manifest->include_dir_count; i++) {
		free(manifest->include_dirs[i]);
	}

	free(manifest->include_dirs);

	for (size_t i = 0;
		i < manifest->cflags_count;
		i++
	) {
		free(manifest->cflags[i]);
	}

	free(manifest->cflags);

	for (size_t i = 0; i < manifest->ldflags_count;
		i++
	) {
		free(manifest->ldflags[i]);
	}

	free(manifest->ldflags);

	free(manifest);
}