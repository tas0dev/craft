/*
 * Copyright (C) 2026 tas0dev
 * This software is licensed under the GNU General Public License, version 3
 * only.
 *
 * Created by tas0dev
 */

#include "parser.h"
#include "internal.h"
#include "lexer.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
	Lexer lexer;
	Token current;
	TomlDocument *document;
	TomlError *error;
	TomlTable *current_table;
	int failed;
} Parser;

static TomlValue parse_array(Parser *parser);

static char *dup_string(const char *text) {
	if (!text) return NULL;

	const size_t length = strlen(text);
	char *copy = malloc(length + 1);

	if (!copy) return NULL;

	memcpy(copy, text, length + 1);
	return copy;
}

static void parser_set_error(Parser *parser,
			     size_t line,
			     size_t column,
			     const char *message) {
	if (parser->failed) return;

	parser->failed = 1;

	if (parser->error) {
		parser->error->line = line;
		parser->error->column = column;
		parser->error->message = message ? message : "parse error";
	}
}

static void parser_advance(Parser *parser) {
	token_free(&parser->current);
	parser->current = lexer_next(&parser->lexer);

	if (parser->current.type == TOKEN_ERROR)
		parser_set_error(parser, parser->current.line,
				 parser->current.column, "invalid token");
}

static int parser_expect(Parser *parser, TokenType type, const char *message) {
	if (parser->failed) return 0;

	if (parser->current.type != type) {
		parser_set_error(parser, parser->current.line,
				 parser->current.column, message);
		return 0;
	}

	return 1;
}

static char *parse_key_path(Parser *parser) {
	size_t capacity = 32;
	size_t length = 0;
	char *path = malloc(capacity);

	if (!path) {
		parser_set_error(parser, parser->current.line,
				 parser->current.column, "out of memory");
		return NULL;
	}

	for (;;) {
		if (!parser_expect(parser, TOKEN_IDENTIFIER, "expected key")) {
			free(path);
			return NULL;
		}

		const size_t part_length = strlen(parser->current.text);

		if (length + part_length + 2 > capacity) {
			while (length + part_length + 2 > capacity)
				capacity *= 2;

			char *new_path = realloc(path, capacity);

			if (!new_path) {
				free(path);
				parser_set_error(parser, parser->current.line,
						 parser->current.column,
						 "out of memory");
				return NULL;
			}

			path = new_path;
		}

		memcpy(path + length, parser->current.text, part_length);
		length += part_length;
		parser_advance(parser);

		if (parser->current.type != TOKEN_DOT) break;

		path[length++] = '.';
		parser_advance(parser);
	}

	path[length] = '\0';
	return path;
}

static TomlValue *
table_get_or_create_table(Parser *parser, TomlTable *table, const char *key) {
	TomlValue *value = toml_table_find_mut(table, key);

	if (!value) {
		value = toml_table_insert(table, key);

		if (!value) {
			parser_set_error(parser, parser->current.line,
					 parser->current.column,
					 "out of memory");
			return NULL;
		}

		value->type = TOML_TABLE;
	}

	if (value->type != TOML_TABLE) {
		parser_set_error(parser, parser->current.line,
				 parser->current.column,
				 "key is already defined as a non-table");
		return NULL;
	}

	return value;
}

static TomlTable *resolve_table_path(Parser *parser, const char *path) {
	TomlTable *table = &parser->document->root;
	const char *start = path;

	for (;;) {
		const char *dot = strchr(start, '.');
		size_t length = dot ? (size_t)(dot - start) : strlen(start);
		char *part = malloc(length + 1);

		if (!part) {
			parser_set_error(parser, parser->current.line,
					 parser->current.column,
					 "out of memory");
			return NULL;
		}

		memcpy(part, start, length);
		part[length] = '\0';

		TomlValue *value =
			table_get_or_create_table(parser, table, part);
		free(part);

		if (!value) return NULL;

		table = &value->as.table;

		if (!dot) return table;

		start = dot + 1;
	}
}

static int parse_integer_text(const char *text, int64_t *value) {
	const size_t length = strlen(text);
	char *clean = malloc(length + 1);

	if (!clean) return 0;

	size_t out = 0;

	for (size_t i = 0; i < length; i++) {
		if (text[i] != '_') clean[out++] = text[i];
	}

	clean[out] = '\0';

	errno = 0;
	char *end = NULL;
	const long long parsed = strtoll(clean, &end, 10);
	const int ok = errno == 0 && end && *end == '\0';

	free(clean);

	if (!ok) return 0;

	*value = parsed;
	return 1;
}

static TomlValue toml_value_init(const TomlType type) {
	TomlValue value = {0};
	value.type = type;

	return value;
}

static TomlValue parse_value(Parser *parser) {
	TomlValue value = toml_value_init(TOML_NULL);

	if (parser->failed) return value;

	switch (parser->current.type) {
	case TOKEN_STRING:
		value.type = TOML_STRING;
		value.as.string = dup_string(parser->current.text);

		if (!value.as.string) {
			parser_set_error(parser, parser->current.line,
					 parser->current.column,
					 "out of memory");
			return value;
		}

		parser_advance(parser);
		return value;

	case TOKEN_INTEGER:
		value.type = TOML_INTEGER;

		if (!parse_integer_text(parser->current.text,
					&value.as.integer)) {
			parser_set_error(parser, parser->current.line,
					 parser->current.column,
					 "invalid integer");
			value.type = TOML_NULL;
			return value;
		}

		parser_advance(parser);
		return value;

	case TOKEN_TRUE:
		value.type = TOML_BOOLEAN;
		value.as.boolean = true;
		parser_advance(parser);
		return value;

	case TOKEN_FALSE:
		value.type = TOML_BOOLEAN;
		value.as.boolean = false;
		parser_advance(parser);
		return value;

	case TOKEN_LBRACKET: return parse_array(parser);

	default:
		parser_set_error(parser, parser->current.line,
				 parser->current.column, "expected value");
		return value;
	}
}

static TomlValue parse_array(Parser *parser) {
	TomlValue value = toml_value_init(TOML_ARRAY);

	parser_advance(parser);

	while (!parser->failed) {
		while (parser->current.type == TOKEN_NEWLINE)
			parser_advance(parser);

		if (parser->current.type == TOKEN_RBRACKET) {
			parser_advance(parser);
			return value;
		}

		TomlValue item = parse_value(parser);

		if (parser->failed) {
			toml_value_destroy(&item);
			toml_value_destroy(&value);

			TomlValue null_value = {0};
			null_value.type = TOML_NULL;
			return null_value;
		}

		if (value.as.array.length == value.as.array.capacity) {
			size_t capacity = value.as.array.capacity
						  ? value.as.array.capacity * 2
						  : 4;

			TomlValue *items = realloc(value.as.array.items,
						   capacity * sizeof(*items));

			if (!items) {
				toml_value_destroy(&item);
				toml_value_destroy(&value);
				parser_set_error(parser, parser->current.line,
						 parser->current.column,
						 "out of memory");

				TomlValue null_value = {0};
				null_value.type = TOML_NULL;
				return null_value;
			}

			value.as.array.items = items;
			value.as.array.capacity = capacity;
		}

		value.as.array.items[value.as.array.length++] = item;

		while (parser->current.type == TOKEN_NEWLINE)
			parser_advance(parser);

		if (parser->current.type == TOKEN_COMMA) {
			parser_advance(parser);
			continue;
		}

		if (parser->current.type != TOKEN_RBRACKET) {
			parser_set_error(parser, parser->current.line,
					 parser->current.column,
					 "expected ',' or ']'");

			toml_value_destroy(&value);

			TomlValue null_value = {0};
			null_value.type = TOML_NULL;
			return null_value;
		}
	}

	toml_value_destroy(&value);

	TomlValue null_value = {0};
	null_value.type = TOML_NULL;
	return null_value;
}

static int assign_value(Parser *parser,
			TomlTable *table,
			const char *path,
			TomlValue value) {
	const char *start = path;
	const char *dot = strchr(start, '.');

	while (dot) {
		const size_t length = (size_t)(dot - start);
		char *part = malloc(length + 1);

		if (!part) {
			toml_value_destroy(&value);
			parser_set_error(parser, parser->current.line,
					 parser->current.column,
					 "out of memory");
			return 0;
		}

		memcpy(part, start, length);
		part[length] = '\0';

		TomlValue *table_value =
			table_get_or_create_table(parser, table, part);
		free(part);

		if (!table_value) {
			toml_value_destroy(&value);
			return 0;
		}

		table = &table_value->as.table;
		start = dot + 1;
		dot = strchr(start, '.');
	}

	if (toml_table_find_mut(table, start)) {
		toml_value_destroy(&value);
		parser_set_error(parser, parser->current.line,
				 parser->current.column, "duplicate key");
		return 0;
	}

	TomlValue *slot = toml_table_insert(table, start);

	if (!slot) {
		toml_value_destroy(&value);
		parser_set_error(parser, parser->current.line,
				 parser->current.column, "out of memory");
		return 0;
	}

	*slot = value;
	return 1;
}

static void parse_table_header(Parser *parser) {
	const size_t line = parser->current.line;
	const size_t column = parser->current.column;

	parser_advance(parser);

	if (parser->current.type == TOKEN_LBRACKET) {
		parser_set_error(parser, line, column,
				 "array of tables is not supported");
		return;
	}

	char *path = parse_key_path(parser);

	if (!path) return;

	if (!parser_expect(parser, TOKEN_RBRACKET, "expected ']'")) {
		free(path);
		return;
	}

	parser_advance(parser);
	parser->current_table = resolve_table_path(parser, path);
	free(path);

	if (!parser->current_table) return;

	if (parser->current.type != TOKEN_NEWLINE &&
	    parser->current.type != TOKEN_EOF)
		parser_set_error(parser, parser->current.line,
				 parser->current.column,
				 "expected end of line");
}

static void parse_assignment(Parser *parser) {
	char *path = parse_key_path(parser);

	if (!path) return;

	if (!parser_expect(parser, TOKEN_EQUAL, "expected '='")) {
		free(path);
		return;
	}

	parser_advance(parser);
	TomlValue value = parse_value(parser);

	if (parser->failed) {
		free(path);
		toml_value_destroy(&value);
		return;
	}

	if (!assign_value(parser, parser->current_table, path, value)) {
		free(path);
		return;
	}

	free(path);

	if (parser->current.type != TOKEN_NEWLINE &&
	    parser->current.type != TOKEN_EOF)
		parser_set_error(parser, parser->current.line,
				 parser->current.column,
				 "expected end of line");
}

TomlDocument *parser_parse(const char *source, TomlError *error) {
	if (error) {
		error->line = 0;
		error->column = 0;
		error->message = NULL;
	}

	if (!source) {
		if (error) {
			error->line = 1;
			error->column = 1;
			error->message = "source is null";
		}
		return NULL;
	}

	TomlDocument *document = calloc(1, sizeof(*document));

	if (!document) {
		if (error) {
			error->line = 1;
			error->column = 1;
			error->message = "out of memory";
		}
		return NULL;
	}

	Parser parser = {
		.document = document,
		.error = error,
		.current_table = &document->root,
	};

	lexer_init(&parser.lexer, source);
	parser.current = lexer_next(&parser.lexer);

	if (parser.current.type == TOKEN_ERROR)
		parser_set_error(&parser, parser.current.line,
				 parser.current.column, "invalid token");

	while (!parser.failed && parser.current.type != TOKEN_EOF) {
		if (parser.current.type == TOKEN_NEWLINE) {
			parser_advance(&parser);
			continue;
		}

		if (parser.current.type == TOKEN_LBRACKET)
			parse_table_header(&parser);
		else
			parse_assignment(&parser);

		if (!parser.failed && parser.current.type == TOKEN_NEWLINE)
			parser_advance(&parser);
	}

	token_free(&parser.current);

	if (parser.failed) {
		toml_free(document);
		return NULL;
	}

	return document;
}
