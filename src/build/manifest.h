/*
 * Copyright (C) 2026 tas0dev
 * This software is licensed under the GNU General Public License, version 3
 * only.
 *
 * Created by tas0dev
 */

#ifndef CRAFT_MANIFEST_H
#define CRAFT_MANIFEST_H
#include <stddef.h>

typedef enum {
	Executable,
	StaticLibrary,
	DynamicLibrary,
} TargetType;

typedef struct {
	char *name;
	char **source_dirs;
	size_t source_dir_count;
	char **include_dirs;
	size_t include_dir_count;
	char **cflags;
	size_t cflags_count;
	char **ldflags;
	size_t ldflags_count;
	char **dependencies;
	size_t dependency_count;
	char *linker_script;
	TargetType target_type;
} BuildTarget;

typedef struct {
	char *name;
	char *cc;
	char *ld;
	BuildTarget *targets;
	size_t target_count;
} Manifest;

typedef struct {
	size_t line;
	size_t column;
	const char *message;
} ManifestError;

Manifest *manifest_load(const char *path, ManifestError *error);
void manifest_free(Manifest *manifest);
BuildTarget *manifest_find_target(const Manifest *manifest, const char *name);

#endif // CRAFT_MANIFEST_H
