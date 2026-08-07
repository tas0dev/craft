/*
 * Copyright (C) 2026 tas0dev
 * This software is licensed under the GNU General Public License, version 3
 * only.
 *
 * Created by tas0dev
 */

#include "help.h"
#include "../cli.h"
#include <stdio.h>

void print_help(const int argc, char *argv[]) {
	(void)argc;

	char *bin = argv[0];
	printf(BLUE "usage: " RESET MAGENTA "%s" RESET "[" GREEN "-h" RESET
		    "," GREEN "help" RESET "]",
	       bin);
}