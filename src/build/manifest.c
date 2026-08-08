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

static int load_string_array(const TomlTable *table,
			     const char *key,
			     char ***items,
			     size_t *count,
			     ManifestError *error) {
	const TomlValue *value = toml_table_get(table, key);

	if (!value) return 1;

	const TomlArray *array = toml_array(value);

	if (!array) {
		if (error) {
			error->line = 0;
			error->column = 0;
			error->message = "expected string array";
		}

		return 0;
	}

	const size_t length = toml_array_length(array);

	if (length == 0) return 1;

	char **result = calloc(length, sizeof(char *));

	if (!result) {
		fprintf(stderr, "Failed to allocate manifest array\n");
		return 0;
	}

	for (size_t i = 0; i < length; i++) {
		const TomlValue *item = toml_array_get(array, i);

		const char *string = toml_string(item);

		if (!string) {
			if (error) {
				error->line = 0;
				error->column = 0;
				error->message = "array must contain strings";
			}

			for (size_t j = 0; j < i; j++)
				free(result[j]);

			free(result);
			return 0;
		}

		result[i] = malloc(strlen(string) + 1);

		if (!result[i]) {
			for (size_t j = 0; j < i; j++)
				free(result[j]);

			free(result);

			fprintf(stderr, "Failed to allocate manifest string\n");

			return 0;
		}

		strcpy(result[i], string);
	}

	*items = result;
	*count = length;

	return 1;
}

static int load_target(BuildTarget *target,
		       const char *name,
		       const TomlTable *table,
		       ManifestError *error) {
	target->name = malloc(strlen(name) + 1);

	if (!target->name) {
		fprintf(stderr, "Failed to allocate target name\n");

		return 0;
	}

	strcpy(target->name, name);

	const TomlValue *type_value = toml_table_get(table, "type");

	const char *type = toml_string(type_value);

	if (!type || strcmp(type, "executable") == 0) {
		target->target_type = Executable;
	} else if (strcmp(type, "staticlib") == 0) {
		target->target_type = StaticLibrary;
	} else if (strcmp(type, "dynlib") == 0) {
		target->target_type = DynamicLibrary;
	} else {
		if (error) {
			error->line = 0;
			error->column = 0;
			error->message = "target.type must be executable, "
					 "staticlib or dynlib";
		}

		return 0;
	}

	if (!load_string_array(table, "source_dirs", &target->source_dirs,
			       &target->source_dir_count, error)) {
		return 0;
	}

	if (!target->source_dirs) {
		target->source_dir_count = 1;

		target->source_dirs = calloc(1, sizeof(char *));

		if (!target->source_dirs) return 0;

		target->source_dirs[0] = malloc(sizeof("src"));

		if (!target->source_dirs[0]) return 0;

		strcpy(target->source_dirs[0], "src");
	}

	if (!load_string_array(table, "include_dirs", &target->include_dirs,
			       &target->include_dir_count, error)) {
		return 0;
	}

	if (!target->include_dirs) {
		target->include_dir_count = 2;

		target->include_dirs = calloc(2, sizeof(char *));

		if (!target->include_dirs) return 0;

		target->include_dirs[0] = malloc(sizeof("src"));

		target->include_dirs[1] = malloc(sizeof("include"));

		if (!target->include_dirs[0] || !target->include_dirs[1]) {
			return 0;
		}

		strcpy(target->include_dirs[0], "src");

		strcpy(target->include_dirs[1], "include");
	}

	if (!load_string_array(table, "cflags", &target->cflags,
			       &target->cflags_count, error)) {
		return 0;
	}

	if (!load_string_array(table, "ldflags", &target->ldflags,
			       &target->ldflags_count, error)) {
		return 0;
	}

	if (!load_string_array(table, "dependencies", &target->dependencies,
			       &target->dependency_count, error)) {
		return 0;
	}

	const TomlValue *linker_script_value =
		toml_table_get(table, "linker_script");

	const char *linker_script = toml_string(linker_script_value);

	if (linker_script) {
		target->linker_script = malloc(strlen(linker_script) + 1);

		if (!target->linker_script) {
			fprintf(stderr, "Failed to allocate linker script\n");

			return 0;
		}

		strcpy(target->linker_script, linker_script);
	}

	return 1;
}

static int load_targets(Manifest *manifest,
			const TomlTable *targets,
			ManifestError *error) {
	const TomlValue *type = toml_table_get(targets, "type");

	const TomlValue *source_dirs = toml_table_get(targets, "source_dirs");

	const TomlValue *include_dirs = toml_table_get(targets, "include_dirs");

	const TomlValue *cflags = toml_table_get(targets, "cflags");

	const TomlValue *ldflags = toml_table_get(targets, "ldflags");

	const TomlValue *linker_script =
		toml_table_get(targets, "linker_script");

	const int implicit_target = type || source_dirs || include_dirs ||
				    cflags || ldflags || linker_script;

	if (implicit_target) {
		manifest->target_count = 1;

		manifest->targets = calloc(1, sizeof(BuildTarget));

		if (!manifest->targets) {
			fprintf(stderr, "Failed to allocate target\n");

			return 0;
		}

		if (!load_target(&manifest->targets[0], manifest->name, targets,
				 error)) {
			return 0;
		}

		return 1;
	}

	manifest->target_count = toml_table_length(targets);

	if (manifest->target_count == 0) {
		if (error) {
			error->line = 0;
			error->column = 0;
			error->message = "at least one target is required";
		}

		return 0;
	}

	manifest->targets = calloc(manifest->target_count, sizeof(BuildTarget));

	if (!manifest->targets) {
		fprintf(stderr, "Failed to allocate targets\n");

		return 0;
	}

	for (size_t i = 0; i < manifest->target_count; i++) {
		const char *target_name = toml_table_key(targets, i);

		const TomlValue *target_value = toml_table_value(targets, i);

		if (!target_name || !target_value) {
			if (error) {
				error->line = 0;
				error->column = 0;
				error->message = "invalid target";
			}

			return 0;
		}

		const TomlTable *target_table = toml_table(target_value);

		if (!target_table) {
			if (error) {
				error->line = 0;
				error->column = 0;
				error->message = "target must be a table";
			}

			return 0;
		}

		if (!load_target(&manifest->targets[i], target_name,
				 target_table, error)) {
			return 0;
		}
	}

	return 1;
}

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

	manifest->name = malloc(strlen(name) + 1);

	if (!manifest->name) {
		fprintf(stderr, "Failed to allocate project name\n");

		manifest_free(manifest);
		toml_free(document);

		return NULL;
	}

	strcpy(manifest->name, name);

	const char *cc = toml_get_string(document, "toolchain.cc");

	if (!cc) cc = "cc";

	manifest->cc = malloc(strlen(cc) + 1);

	if (!manifest->cc) {
		fprintf(stderr, "Failed to allocate compiler name\n");

		manifest_free(manifest);
		toml_free(document);

		return NULL;
	}

	strcpy(manifest->cc, cc);

	const char *ld = toml_get_string(document, "toolchain.ld");

	if (!ld) ld = "cc";

	manifest->ld = malloc(strlen(ld) + 1);

	if (!manifest->ld) {
		fprintf(stderr, "Failed to allocate linker name\n");

		manifest_free(manifest);
		toml_free(document);

		return NULL;
	}

	strcpy(manifest->ld, ld);

	const TomlTable *targets = toml_get_table(document, "target");

	if (!targets) {
		if (error) {
			error->line = 0;
			error->column = 0;
			error->message = "target is required";
		}

		manifest_free(manifest);
		toml_free(document);

		return NULL;
	}

	if (!load_targets(manifest, targets, error)) {
		manifest_free(manifest);
		toml_free(document);

		return NULL;
	}

	toml_free(document);

	return manifest;
}

void manifest_free(Manifest *manifest) {
	if (!manifest) return;

	free(manifest->name);
	free(manifest->cc);
	free(manifest->ld);

	for (size_t i = 0; i < manifest->target_count; i++) {
		BuildTarget *target = &manifest->targets[i];

		free(target->name);
		free(target->linker_script);

		for (size_t j = 0; j < target->source_dir_count; j++) {
			free(target->source_dirs[j]);
		}

		free(target->source_dirs);

		for (size_t j = 0; j < target->include_dir_count; j++) {
			free(target->include_dirs[j]);
		}

		free(target->include_dirs);

		for (size_t j = 0; j < target->cflags_count; j++) {
			free(target->cflags[j]);
		}

		free(target->cflags);

		for (size_t j = 0; j < target->ldflags_count; j++) {
			free(target->ldflags[j]);
		}

		free(target->ldflags);

		for (size_t j = 0; j < target->dependency_count; j++) {
			free(target->dependencies[j]);
		}

		free(target->dependencies);
	}

	free(manifest->targets);
	free(manifest);
}

BuildTarget *manifest_find_target(const Manifest *manifest, const char *name) {
	if (!manifest || !name) return NULL;

	for (size_t i = 0; i < manifest->target_count; i++) {
		if (strcmp(manifest->targets[i].name, name) == 0)
			return &manifest->targets[i];
	}

	return NULL;
}