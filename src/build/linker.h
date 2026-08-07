/*
 * Copyright (C) 2026 tas0dev
 * This software is licensed under the GNU General Public License, version 3
 * only.
 *
 * Created by tas0dev
 */

#ifndef CRAFT_LINKER_H
#define CRAFT_LINKER_H
#include "compiler.h"
#include "manifest.h"

int link_objects(const Manifest *manifest,
		 const ObjectList *objects,
		 const char *project_root);

#endif // CRAFT_LINKER_H
