/*
 * Copyright (C) 2026 tas0dev
 * This software is licensed under the GNU General Public License, version 3
 * only.
 *
 * Created by tas0dev
 */

#include "profile.h"

const char *build_profile_name(BuildProfile profile) {
	switch (profile) {
	case Release: return "release";
	case Debug: return "debug";
	}

	return "debug";
}

const char *build_profile_optimization(BuildProfile profile) {
	switch (profile) {
	case Debug: return "-O0";
	case Release: return "-O3";
	}

	return "-O0";
}
