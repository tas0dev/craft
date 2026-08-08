/*
 * Copyright (C) 2026 tas0dev
 * This software is licensed under the GNU General Public License, version 3
 * only.
 *
 * Created by tas0dev
 */

#include <stdio.h>

int main(const int argc, char **argv) {
	for (int i = 0; i < argc; i++)
		printf("%s\n", argv[i]);

	return 0;
}