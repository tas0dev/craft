/*
 * Copyright (C) 2026 tas0dev
 * This software is licensed under the GNU General Public License, version 3
 * only.
 *
 * Created by tas0dev
 */

#include "app.h"
#include "commands/build.h"
#include "commands/clean.h"
#include "commands/help.h"
#include "commands/install.h"
#include "commands/run.h"
#include "commands/targets.h"
#include "commands/test.h"
#include "commands/unknown.h"
#include "commands/version.h"

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
	case Targets: run_targets(argc, argv); break;
	case Install: run_install(argc, argv); break;
	case Test: run_tests(argc, argv); break;
	case UnknownCommand: print_unknown(argc, argv); break;
	}
	return 0;
}
