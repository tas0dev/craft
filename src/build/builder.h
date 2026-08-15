/*
 * Copyright (C) 2026 tas0dev
 * This software is licensed under the GNU General Public License, version 3
 * only.
 *
 * Created by tas0dev
 */

#ifndef CRAFT_BUILDER_H
#define CRAFT_BUILDER_H
#include "manifest.h"
#include "profile.h"

int build_target(const Manifest *manifest,
		 const BuildTarget *target,
		 const char *project_root,
		 const BuildProfile *profile,
		 int verbose,
		 unsigned long long jobs);

int build_target_recursive(const Manifest *manifest,
			   const BuildTarget *target,
			   const char *project_root,
			   unsigned char *states,
			   const BuildProfile *profile,
			   int verbose,
			   unsigned long long jobs);

char *artifact_path(const BuildTarget *target,
		    const char *project_root,
		    BuildProfile profile);

#endif // CRAFT_BUILDER_H
