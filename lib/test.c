/*
 * Copyright (C) 2026 tas0dev
 * This software is licensed under the GNU General Public License, version 3
 * only.
 *
 * Created by tas0dev
 */

#include "test.h"
#include "common.h"
#include <stdio.h>

static int current_test_failed = 0;

void craft_test_fail(const char *file, const int line, const char *expression) {
	current_test_failed = 1;

	fprintf(stderr, RED "\nassertion failed: %s\n" RESET "  at %s:%d\n",
		expression, file, line);
}

int craft_test_run_all(const CraftTestCase *tests, const size_t count) {
	size_t passed = 0;
	size_t failed = 0;

	printf(GREEN "running %zu tests\n" RESET, count);

	for (size_t i = 0; i < count; i++) {
		const CraftTestCase *test = &tests[i];

		current_test_failed = 0;

		test->function();

		if (current_test_failed) {
			printf(RED "test %s ... FAILED\n" RESET, test->name);
			failed++;
		} else {
			printf(RED "test %s ... ok\n" RESET, test->name);
			passed++;
		}
	}

	printf(BLUE "\ntest result: " RESET GREEN "%s\n\t%zu passed" RESET RED
		    "\n\t%zu failed\n" RESET,
	       failed == 0 ? "ok" : "FAILED", passed, failed);

	return failed == 0 ? 0 : 1;
}