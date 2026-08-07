/*
 * Copyright (C) 2026 tas0dev
 * This software is licensed under the GNU General Public License, version 3
 * only.
 *
 * Created by tas0dev
 */

#include "app.h"
#include <string.h>

enum Commands match_commands(const char *command) {
	if (strcmp(command, "help") == 0 || strcmp(command, "-h") == 0)
		return Help;
	return Unknown;
}