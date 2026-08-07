/*
 * Copyright (C) 2026 tas0dev
 * This software is licensed under the GNU General Public License, version 3
 * only.
 *
 * Created by tas0dev
 */

#ifndef CRAFT_TOML_H
#define CRAFT_TOML_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct TomlDocument TomlDocument;
typedef struct TomlValue TomlValue;
typedef struct TomlArray TomlArray;
typedef struct TomlTable TomlTable;

typedef enum {
	TOML_NULL,
	TOML_STRING,
	TOML_INTEGER,
	TOML_BOOLEAN,
	TOML_ARRAY,
	TOML_TABLE
} TomlType;

typedef struct {
	size_t line;
	size_t column;
	const char *message;
} TomlError;

TomlDocument *toml_parse(const char *source, TomlError *error);
TomlDocument *toml_parse_file(const char *path, TomlError *error);
void toml_free(TomlDocument *document);

const TomlValue *toml_get(const TomlDocument *document, const char *path);

TomlType toml_type(const TomlValue *value);
const char *toml_string(const TomlValue *value);
int64_t toml_integer(const TomlValue *value);
bool toml_boolean(const TomlValue *value);
const TomlArray *toml_array(const TomlValue *value);
const TomlTable *toml_table(const TomlValue *value);

size_t toml_array_length(const TomlArray *array);
const TomlValue *toml_array_get(const TomlArray *array, size_t index);

size_t toml_table_length(const TomlTable *table);
const char *toml_table_key(const TomlTable *table, size_t index);
const TomlValue *toml_table_value(const TomlTable *table, size_t index);
const TomlValue *toml_table_get(const TomlTable *table, const char *key);

const char *toml_get_string(const TomlDocument *document, const char *path);
bool toml_get_integer(const TomlDocument *document,
		      const char *path,
		      int64_t *value);
bool toml_get_boolean(const TomlDocument *document,
		      const char *path,
		      bool *value);
const TomlArray *toml_get_array(const TomlDocument *document, const char *path);
const TomlTable *toml_get_table(const TomlDocument *document, const char *path);

#endif
