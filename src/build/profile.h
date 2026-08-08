/*
 * Copyright (C) 2026 tas0dev
 * This software is licensed under the GNU General Public License, version 3
 * only.
 *
 * Created by tas0dev
 */

#ifndef CRAFT_PROFILE_H
#define CRAFT_PROFILE_H

typedef enum {
	Release,
	Debug,
} BuildProfile;

const char *build_profile_name(BuildProfile profile);
const char *build_profile_optimization(BuildProfile profile);

#endif // CRAFT_PROFILE_H
