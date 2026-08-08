/*
 * Copyright (C) 2026 tas0dev
 * This software is licensed under the GNU General Public License, version 3
 * only.
 *
 * Created by tas0dev
 */

#ifndef CRAFT_COMPILER_H
#define CRAFT_COMPILER_H
#include "profile.h"
#include "source.h"

typedef struct {
	char **files;
	size_t count;
} ObjectList;

ObjectList *compile_sources(const Manifest *manifest,
			    const BuildTarget *target,
			    const SourceList *sources,
			    const char *project_root,
			    BuildProfile profile);

void object_list_free(ObjectList *objects);
#endif // CRAFT_COMPILER_H
