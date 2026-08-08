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
	if (strcmp(command, "help") == 0 || strcmp(command, "-h") == 0 ||
	    strcmp(command, "--help") == 0)
		return Help;
	if (strcmp(command, "version") == 0 || strcmp(command, "-v") == 0 ||
	    strcmp(command, "--version") == 0)
		return Version;
	if (strcmp(command, "build") == 0) return Build;
	if (strcmp(command, "clean") == 0) return Clean;
	if (strcmp(command, "run") == 0) return Run;
	if (strcmp(command, "targets") == 0) return Targets;
	if (strcmp(command, "install") == 0) return Install;
	return UnknownCommand;
}