/*
 * Copyright (C) 2026 tas0dev
 * This software is licensed under the GNU General Public License, version 3
 * only.
 *
 * Created by tas0dev
 */

#include "lexer.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char lexer_peek(const Lexer *lexer) {
	return lexer->source[lexer->position];
}

static char lexer_advance(Lexer *lexer) {
	char c = lexer->source[lexer->position++];

	if (c == '\n') {
		lexer->line++;
		lexer->column = 1;
	} else {
		lexer->column++;
	}

	return c;
}

static char *dup_string(const char *text) {
	if (!text) return NULL;

	const size_t length = strlen(text);
	char *copy = malloc(length + 1);

	if (!copy) return NULL;

	memcpy(copy, text, length + 1);
	return copy;
}

static char *copy_range(const char *source, size_t start, size_t end) {
	const size_t length = end - start;
	char *text = malloc(length + 1);

	if (!text) return NULL;

	memcpy(text, source + start, length);
	text[length] = '\0';
	return text;
}

static Token
make_token(TokenType type, char *text, size_t line, size_t column) {
	Token token = {
		.type = type,
		.text = text,
		.line = line,
		.column = column,
	};
	return token;
}

static Token make_simple(TokenType type, size_t line, size_t column) {
	return make_token(type, NULL, line, column);
}

static void skip_spaces_and_comments(Lexer *lexer) {
	for (;;) {
		char c = lexer_peek(lexer);

		while (c == ' ' || c == '\t' || c == '\r') {
			lexer_advance(lexer);
			c = lexer_peek(lexer);
		}

		if (c != '#') return;

		while (lexer_peek(lexer) != '\0' && lexer_peek(lexer) != '\n')
			lexer_advance(lexer);
	}
}

static Token lex_string(Lexer *lexer, size_t line, size_t column) {
	lexer_advance(lexer);

	size_t capacity = 32;
	size_t length = 0;
	char *buffer = malloc(capacity);

	if (!buffer) return make_token(TOKEN_ERROR, NULL, line, column);

	while (lexer_peek(lexer) != '\0' && lexer_peek(lexer) != '"') {
		char c = lexer_advance(lexer);

		if (c == '\n') {
			free(buffer);
			return make_token(TOKEN_ERROR,
					  dup_string("newline in string"), line,
					  column);
		}

		if (c == '\\') {
			char escaped = lexer_peek(lexer);

			if (escaped == '\0') {
				free(buffer);
				return make_token(
					TOKEN_ERROR,
					dup_string(
						"unterminated escape sequence"),
					line, column);
			}

			lexer_advance(lexer);

			switch (escaped) {
			case '"': c = '"'; break;
			case '\\': c = '\\'; break;
			case 'n': c = '\n'; break;
			case 'r': c = '\r'; break;
			case 't': c = '\t'; break;
			default:
				free(buffer);
				return make_token(
					TOKEN_ERROR,
					dup_string(
						"unsupported escape sequence"),
					line, column);
			}
		}

		if (length + 1 >= capacity) {
			capacity *= 2;
			char *new_buffer = realloc(buffer, capacity);

			if (!new_buffer) {
				free(buffer);
				return make_token(TOKEN_ERROR, NULL, line,
						  column);
			}

			buffer = new_buffer;
		}

		buffer[length++] = c;
	}

	if (lexer_peek(lexer) != '"') {
		free(buffer);
		return make_token(TOKEN_ERROR,
				  dup_string("unterminated string"), line,
				  column);
	}

	lexer_advance(lexer);
	buffer[length] = '\0';

	return make_token(TOKEN_STRING, buffer, line, column);
}

static int is_identifier_start(char c) {
	return isalpha((unsigned char)c) || c == '_' || c == '-';
}

static int is_identifier_part(char c) {
	return isalnum((unsigned char)c) || c == '_' || c == '-';
}

static Token lex_identifier(Lexer *lexer, size_t line, size_t column) {
	size_t start = lexer->position;

	while (is_identifier_part(lexer_peek(lexer)))
		lexer_advance(lexer);

	char *text = copy_range(lexer->source, start, lexer->position);

	if (!text) return make_token(TOKEN_ERROR, NULL, line, column);

	if (strcmp(text, "true") == 0) {
		free(text);
		return make_simple(TOKEN_TRUE, line, column);
	}

	if (strcmp(text, "false") == 0) {
		free(text);
		return make_simple(TOKEN_FALSE, line, column);
	}

	return make_token(TOKEN_IDENTIFIER, text, line, column);
}

static Token lex_integer(Lexer *lexer, size_t line, size_t column) {
	size_t start = lexer->position;

	if (lexer_peek(lexer) == '+' || lexer_peek(lexer) == '-')
		lexer_advance(lexer);

	if (!isdigit((unsigned char)lexer_peek(lexer)))
		return make_token(TOKEN_ERROR, dup_string("expected integer"),
				  line, column);

	while (isdigit((unsigned char)lexer_peek(lexer)) ||
	       lexer_peek(lexer) == '_')
		lexer_advance(lexer);

	char *text = copy_range(lexer->source, start, lexer->position);

	if (!text) return make_token(TOKEN_ERROR, NULL, line, column);

	return make_token(TOKEN_INTEGER, text, line, column);
}

void lexer_init(Lexer *lexer, const char *source) {
	lexer->source = source;
	lexer->position = 0;
	lexer->line = 1;
	lexer->column = 1;
}

Token lexer_next(Lexer *lexer) {
	skip_spaces_and_comments(lexer);

	const size_t line = lexer->line;
	const size_t column = lexer->column;
	const char c = lexer_peek(lexer);

	if (c == '\0') return make_simple(TOKEN_EOF, line, column);

	if (c == '\n') {
		lexer_advance(lexer);
		return make_simple(TOKEN_NEWLINE, line, column);
	}

	switch (c) {
	case '[':
		lexer_advance(lexer);
		return make_simple(TOKEN_LBRACKET, line, column);
	case ']':
		lexer_advance(lexer);
		return make_simple(TOKEN_RBRACKET, line, column);
	case '=':
		lexer_advance(lexer);
		return make_simple(TOKEN_EQUAL, line, column);
	case ',':
		lexer_advance(lexer);
		return make_simple(TOKEN_COMMA, line, column);
	case '.':
		lexer_advance(lexer);
		return make_simple(TOKEN_DOT, line, column);
	case '"': return lex_string(lexer, line, column);
	default: break;
	}

	if (isdigit((unsigned char)c) ||
	    ((c == '+' || c == '-') &&
	     isdigit((unsigned char)lexer->source[lexer->position + 1])))
		return lex_integer(lexer, line, column);

	if (is_identifier_start(c)) return lex_identifier(lexer, line, column);

	lexer_advance(lexer);

	char message[64];
	snprintf(message, sizeof(message), "unexpected character '%c'", c);
	return make_token(TOKEN_ERROR, dup_string(message), line, column);
}

void token_free(Token *token) {
	free(token->text);
	token->text = NULL;
}
