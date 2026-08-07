/*
 * Copyright (C) 2026 tas0dev
 * This software is licensed under the GNU General Public License, version 3
 * only.
 *
 * Created by tas0dev
 */

#include "version.h"
#include "src/app.h"
#include <stdio.h>

void print_version(const int argc, char *argv[]) {
	(void)argc;
	(void)argv;

	printf("Craft version " VERSION "\n");
	printf("Build date: " BUILD "\n");
	printf("Author: " AUTHOR "\n");
}