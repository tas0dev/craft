/*
 * Copyright (C) 2026 tas0dev
 * This software is licensed under the GNU General Public License, version 3
 * only.
 *
 * Created by tas0dev
 */

#ifndef CRAFT_TOML_INTERNAL_H
#define CRAFT_TOML_INTERNAL_H

#include "toml.h"

struct TomlArray {
	TomlValue *items;
	size_t length;
	size_t capacity;
};

typedef struct {
	char *key;
	TomlValue *value;
} TomlEntry;

struct TomlTable {
	TomlEntry *entries;
	size_t length;
	size_t capacity;
};

struct TomlValue {
	TomlType type;
	struct {
		char *string;
		int64_t integer;
		bool boolean;
		TomlArray array;
		TomlTable table;
	} as;
};

struct TomlDocument {
	TomlTable root;
};

void toml_value_destroy(TomlValue *value);
TomlValue *toml_table_insert(TomlTable *table, const char *key);
TomlValue *toml_table_find_mut(TomlTable *table, const char *key);

#endif
