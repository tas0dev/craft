/*
 * Copyright (C) 2026 tas0dev
 * This software is licensed under the GNU General Public License, version 3
 * only.
 *
 * Created by tas0dev
 */

#include "help.h"
#include "cli.h"
#include <stdio.h>

void print_help(const int argc, char *argv[]) {
	(void)argc;
	(void)argv;

	printf(BLUE "Usage:\n" RESET "\t" MAGENTA "craft" RESET
		    " <command> [arguments]\n"
		    "\n" BLUE "Commands:\n" RESET "\t" GREEN "build" RESET
		    "\t[target]\tBuild the project or a target\n"
		    "\t" GREEN "run" RESET
		    "\t[target]\tBuild and run an executable target\n"
		    "\t" GREEN "clean" RESET "\t\t\tRemove build artifacts\n"
		    "\t" GREEN "version" RESET "\t\t\tPrint Craft version\n"
		    "\t" GREEN "help" RESET "\t\t\tPrint this help\n"
		    "\n" BLUE "Options:\n" RESET "\t" GREEN "-h" RESET
		    "\t\t\tPrint this help\n"
		    "\t" GREEN "-v" RESET "\t\t\tPrint Craft version\n");

	fflush(stdout);
}