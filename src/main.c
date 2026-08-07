/*
 * Copyright (C) 2026 tas0dev
 * This software is licensed under the GNU General Public License, version 3
 * only.
 *
 * Created by tas0dev
 */

#include "app.h"
#include "cli.h"
#include "commands/help.h"
#include <stdio.h>

int main(const int argc, char **argv) {
	if (argc == 1) print_help(argc, argv);

	if (match_commands(argv[1]) == Help) print_help(argc, argv);
	if (match_commands(argv[1]) == Unknown)
		printf(RED "Unknown command: " RESET "%s", argv[1]);
}
