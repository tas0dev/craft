/*
 * Copyright (C) 2026 tas0dev
 * This software is licensed under the GNU General Public License, version 3
 * only.
 *
 * Created by tas0dev
 */

#include "unknown.h"

#include "cli.h"
#include "help.h"

#include <stdio.h>

int print_unknown(const int argc, char **argv) {
	(void)argc;
	fprintf(stderr, RED "unknown command: " RESET "%s\n", argv[1]);

	print_help(argc, argv);
}