/*
 * Copyright (C) 2026 tas0dev
 * This software is licensed under the GNU General Public License, version 3
 * only.
 *
 * Created by tas0dev
 */

#include "toml.h"
#include "internal.h"
#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dup_string(const char *text) {
	if (!text) return NULL;

	const size_t length = strlen(text);
	char *copy = malloc(length + 1);

	if (!copy) return NULL;

	memcpy(copy, text, length + 1);
	return copy;
}

void toml_value_destroy(TomlValue *value) {
	if (!value) return;

	switch (value->type) {
	case TOML_STRING: free(value->as.string); break;

	case TOML_ARRAY:
		for (size_t i = 0; i < value->as.array.length; i++)
			toml_value_destroy(&value->as.array.items[i]);

		free(value->as.array.items);
		break;

	case TOML_TABLE:
		for (size_t i = 0; i < value->as.table.length; i++) {
			free(value->as.table.entries[i].key);
			toml_value_destroy(value->as.table.entries[i].value);
			free(value->as.table.entries[i].value);
		}

		free(value->as.table.entries);
		break;

	default: break;
	}

	memset(value, 0, sizeof(*value));
	value->type = TOML_NULL;
}

TomlValue *toml_table_find_mut(TomlTable *table, const char *key) {
	if (!table || !key) return NULL;

	for (size_t i = 0; i < table->length; i++) {
		if (strcmp(table->entries[i].key, key) == 0)
			return table->entries[i].value;
	}

	return NULL;
}

TomlValue *toml_table_insert(TomlTable *table, const char *key) {
	if (!table || !key) return NULL;

	if (table->length == table->capacity) {
		size_t capacity = table->capacity ? table->capacity * 2 : 8;
		TomlEntry *entries =
			realloc(table->entries, capacity * sizeof(*entries));

		if (!entries) return NULL;

		table->entries = entries;
		table->capacity = capacity;
	}

	char *key_copy = dup_string(key);
	TomlValue *value = calloc(1, sizeof(*value));

	if (!key_copy || !value) {
		free(key_copy);
		free(value);
		return NULL;
	}

	value->type = TOML_NULL;

	table->entries[table->length].key = key_copy;
	table->entries[table->length].value = value;
	table->length++;

	return value;
}

TomlDocument *toml_parse(const char *source, TomlError *error) {
	return parser_parse(source, error);
}

TomlDocument *toml_parse_file(const char *path, TomlError *error) {
	if (error) {
		error->line = 0;
		error->column = 0;
		error->message = NULL;
	}

	if (!path) {
		if (error) {
			error->line = 1;
			error->column = 1;
			error->message = "path is null";
		}
		return NULL;
	}

	FILE *file = fopen(path, "rb");

	if (!file) {
		if (error) {
			error->line = 1;
			error->column = 1;
			error->message = "failed to open file";
		}
		return NULL;
	}

	if (fseek(file, 0, SEEK_END) != 0) {
		fclose(file);
		if (error) {
			error->line = 1;
			error->column = 1;
			error->message = "failed to seek file";
		}
		return NULL;
	}

	long length = ftell(file);

	if (length < 0) {
		fclose(file);
		if (error) {
			error->line = 1;
			error->column = 1;
			error->message = "failed to determine file size";
		}
		return NULL;
	}

	rewind(file);

	char *source = malloc((size_t)length + 1);

	if (!source) {
		fclose(file);
		if (error) {
			error->line = 1;
			error->column = 1;
			error->message = "out of memory";
		}
		return NULL;
	}

	const size_t read_length = fread(source, 1, (size_t)length, file);
	fclose(file);

	if (read_length != (size_t)length) {
		free(source);
		if (error) {
			error->line = 1;
			error->column = 1;
			error->message = "failed to read file";
		}
		return NULL;
	}

	source[length] = '\0';

	TomlDocument *document = toml_parse(source, error);
	free(source);
	return document;
}

void toml_free(TomlDocument *document) {
	if (!document) return;

	for (size_t i = 0; i < document->root.length; i++) {
		free(document->root.entries[i].key);
		toml_value_destroy(document->root.entries[i].value);
		free(document->root.entries[i].value);
	}

	free(document->root.entries);
	free(document);
}

const TomlValue *toml_table_get(const TomlTable *table, const char *key) {
	if (!table || !key) return NULL;

	for (size_t i = 0; i < table->length; i++) {
		if (strcmp(table->entries[i].key, key) == 0)
			return table->entries[i].value;
	}

	return NULL;
}

const TomlValue *toml_get(const TomlDocument *document, const char *path) {
	if (!document || !path || !*path) return NULL;

	const TomlTable *table = &document->root;
	const char *start = path;

	for (;;) {
		const char *dot = strchr(start, '.');
		size_t length = dot ? (size_t)(dot - start) : strlen(start);

		if (length == 0) return NULL;

		char *part = malloc(length + 1);

		if (!part) return NULL;

		memcpy(part, start, length);
		part[length] = '\0';

		const TomlValue *value = toml_table_get(table, part);
		free(part);

		if (!value) return NULL;

		if (!dot) return value;

		if (value->type != TOML_TABLE) return NULL;

		table = &value->as.table;
		start = dot + 1;
	}
}

TomlType toml_type(const TomlValue *value) {
	return value ? value->type : TOML_NULL;
}

const char *toml_string(const TomlValue *value) {
	return value && value->type == TOML_STRING ? value->as.string : NULL;
}

int64_t toml_integer(const TomlValue *value) {
	return value && value->type == TOML_INTEGER ? value->as.integer : 0;
}

bool toml_boolean(const TomlValue *value) {
	return value && value->type == TOML_BOOLEAN ? value->as.boolean : false;
}

const TomlArray *toml_array(const TomlValue *value) {
	return value && value->type == TOML_ARRAY ? &value->as.array : NULL;
}

const TomlTable *toml_table(const TomlValue *value) {
	return value && value->type == TOML_TABLE ? &value->as.table : NULL;
}

size_t toml_array_length(const TomlArray *array) {
	return array ? array->length : 0;
}

const TomlValue *toml_array_get(const TomlArray *array, size_t index) {
	if (!array || index >= array->length) return NULL;

	return &array->items[index];
}

size_t toml_table_length(const TomlTable *table) {
	return table ? table->length : 0;
}

const char *toml_table_key(const TomlTable *table, size_t index) {
	if (!table || index >= table->length) return NULL;

	return table->entries[index].key;
}

const TomlValue *toml_table_value(const TomlTable *table, size_t index) {
	if (!table || index >= table->length) return NULL;

	return table->entries[index].value;
}

const char *toml_get_string(const TomlDocument *document, const char *path) {
	return toml_string(toml_get(document, path));
}

bool toml_get_integer(const TomlDocument *document,
		      const char *path,
		      int64_t *value) {
	const TomlValue *toml_value = toml_get(document, path);

	if (!toml_value || toml_value->type != TOML_INTEGER || !value)
		return false;

	*value = toml_value->as.integer;
	return true;
}

bool toml_get_boolean(const TomlDocument *document,
		      const char *path,
		      bool *value) {
	const TomlValue *toml_value = toml_get(document, path);

	if (!toml_value || toml_value->type != TOML_BOOLEAN || !value)
		return false;

	*value = toml_value->as.boolean;
	return true;
}

const TomlArray *toml_get_array(const TomlDocument *document,
				const char *path) {
	return toml_array(toml_get(document, path));
}

const TomlTable *toml_get_table(const TomlDocument *document,
				const char *path) {
	return toml_table(toml_get(document, path));
}
