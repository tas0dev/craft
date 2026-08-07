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

	if (!manifest_file) return NULL;

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
		toml_free(document);
		return NULL;
	}

	manifest->name = malloc(strlen(name) + 1);

	if (!manifest->name) {
		manifest_free(manifest);
		toml_free(document);
		return NULL;
	}

	strcpy(manifest->name, name);

	const TomlArray *source_dirs =
		toml_get_array(document, "target.source_dirs");

	if (source_dirs) {
		manifest->source_dir_count = toml_array_length(source_dirs);

		manifest->source_dirs =
			calloc(manifest->source_dir_count, sizeof(char *));

		if (!manifest->source_dirs) {
			manifest_free(manifest);
			toml_free(document);
			return NULL;
		}

		for (size_t i = 0; i < manifest->source_dir_count; i++) {
			const TomlValue *value = toml_array_get(source_dirs, i);

			const char *source_dir = toml_string(value);

			if (!source_dir) {
				if (error) {
					error->line = 0;
					error->column = 0;
					error->message = "target.source_dirs "
							 "must contain strings";
				}

				manifest_free(manifest);
				toml_free(document);
				return NULL;
			}

			manifest->source_dirs[i] =
				malloc(strlen(source_dir) + 1);

			if (!manifest->source_dirs[i]) {
				manifest_free(manifest);
				toml_free(document);
				return NULL;
			}

			strcpy(manifest->source_dirs[i], source_dir);
		}
	} else {
		manifest->source_dir_count = 1;

		manifest->source_dirs = calloc(1, sizeof(char *));

		if (!manifest->source_dirs) {
			manifest_free(manifest);
			toml_free(document);
			return NULL;
		}

		manifest->source_dirs[0] = malloc(sizeof("src"));

		if (!manifest->source_dirs[0]) {
			manifest_free(manifest);
			toml_free(document);
			return NULL;
		}

		strcpy(manifest->source_dirs[0], "src");
	}

	const TomlArray *include_dirs =
		toml_get_array(document, "target.include_dirs");

	if (include_dirs) {
		manifest->include_dir_count = toml_array_length(include_dirs);

		manifest->include_dirs =
			calloc(manifest->include_dir_count, sizeof(char *));

		if (!manifest->include_dirs) {
			manifest_free(manifest);
			toml_free(document);
			return NULL;
		}

		for (size_t i = 0; i < manifest->include_dir_count; i++) {
			const TomlValue *value =
				toml_array_get(include_dirs, i);

			const char *include_dir = toml_string(value);

			if (!include_dir) {
				if (error) {
					error->line = 0;
					error->column = 0;
					error->message = "target.include_dirs "
							 "must contain strings";
				}

				manifest_free(manifest);
				toml_free(document);
				return NULL;
			}

			manifest->include_dirs[i] =
				malloc(strlen(include_dir) + 1);

			if (!manifest->include_dirs[i]) {
				manifest_free(manifest);
				toml_free(document);
				return NULL;
			}

			strcpy(manifest->include_dirs[i], include_dir);
		}
	} else {
		manifest->include_dir_count = 2;

		manifest->include_dirs = calloc(2, sizeof(char *));

		if (!manifest->include_dirs) {
			manifest_free(manifest);
			toml_free(document);
			return NULL;
		}

		manifest->include_dirs[0] = malloc(sizeof("src"));

		manifest->include_dirs[1] = malloc(sizeof("include"));

		if (!manifest->include_dirs[0] || !manifest->include_dirs[1]) {
			manifest_free(manifest);
			toml_free(document);
			return NULL;
		}

		strcpy(manifest->include_dirs[0], "src");

		strcpy(manifest->include_dirs[1], "include");
	}

	toml_free(document);

	return manifest;
}

void manifest_free(Manifest *manifest) {
	if (!manifest) return;

	free(manifest->name);

	for (size_t i = 0; i < manifest->source_dir_count; i++)
		free(manifest->source_dirs[i]);

	free(manifest->source_dirs);

	for (size_t i = 0; i < manifest->include_dir_count; i++)
		free(manifest->include_dirs[i]);

	free(manifest->include_dirs);

	for (size_t i = 0; i < manifest->cflags_count; i++)
		free(manifest->cflags[i]);

	free(manifest->cflags);

	free(manifest);
}