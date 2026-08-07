/*
 * Copyright (C) 2026 tas0dev
 * This software is licensed under the GNU General Public License, version 3
 * only.
 *
 * Created by tas0dev
 */

#ifndef CRAFT_APP_H
#define CRAFT_APP_H

#define VERSION "0.1"
#define AUTHOR "tas0dev"
#define BUILD __DATE__

enum Commands {
	Help,
	Version,
	Unknown
};

enum Commands match_commands(const char *command);

#endif // CRAFT_APP_H
