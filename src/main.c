/*
 * Copyright (C) 2026 tas0dev
 * This software is licensed under the GNU General Public License, version 3
 * only.
 *
 * Created by tas0dev
 */

#include "app.h"
#include "cli.h"
#include "commands/build.h"
#include "commands/clean.h"
#include "commands/help.h"
#include "commands/run.h"
#include "commands/version.h"
#include <stdio.h>

int main(const int argc, char **argv) {
	if (argc == 1) {
		print_help(argc, argv);
		return 0;
	}

	const enum Commands command = match_commands(argv[1]);

	switch (command) {
	case Help: print_help(argc, argv); break;
	case Version: print_version(argc, argv); break;
	case Build: run_build(argc, argv); break;
	case Clean: run_clean(argc, argv); break;
	case Run: run_run(argc, argv); break;
	default:
		printf(RED "Unknown command: " RESET "%s\n", argv[1]);
		return 1;
	}

	return 0;
}
