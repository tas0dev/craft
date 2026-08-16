/*
 * Copyright (C) 2026 tas0dev
 * This software is licensed under the GNU General Public License, version 3
 * only.
 *
 * Created by tas0dev
 */

#ifndef CRAFT_TEST_H
#define CRAFT_TEST_H

#include <stddef.h>

typedef void (*CraftTestFunction)(void);

typedef struct {
	const char *name;
	CraftTestFunction function;
} CraftTestCase;

int craft_test_run_all(const CraftTestCase *tests, size_t count);

void craft_test_fail(const char *file, int line, const char *expression);

/// Define test
///
/// Arguments:
/// - name: test name
#define test(name) static void name(void)

#define test_case(name) {#name, name}

#define assert_true(expression)                                           \
	do {                                                              \
		if (!(expression)) {                                      \
			craft_test_fail(__FILE__, __LINE__, #expression); \
			return;                                           \
		}                                                         \
	} while (0)

#define assert_false(expression) assert_true(!(expression))

#define assert_eq(left, right) assert_true((left) == (right))

#define assert_ne(left, right) assert_true((left) != (right))

#define tests(...)                                                           \
	int main(void) {                                                     \
		const CraftTestCase tests[] = {__VA_ARGS__};                 \
		return craft_test_run_all(tests,                             \
					  sizeof(tests) / sizeof(tests[0])); \
	}

#endif