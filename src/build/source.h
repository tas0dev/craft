/*
 * Copyright (C) 2026 tas0dev
 * This software is licensed under the GNU General Public License, version 3
 * only.
 *
 * Created by tas0dev
 */

#ifndef CRAFT_SOURCES_H
#define CRAFT_SOURCES_H
#include "manifest.h"
#include <stddef.h>

typedef struct {
	char **files;
	size_t count;
} SourceList;

SourceList *source_collect(const Manifest *manifest, const char *project_root);
void source_list_free(SourceList *list);

#endif // CRAFT_SOURCES_H
