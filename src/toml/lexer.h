/*
 * Copyright (C) 2026 tas0dev
 * This software is licensed under the GNU General Public License, version 3
 * only.
 *
 * Created by tas0dev
 */

#ifndef CRAFT_TOML_LEXER_H
#define CRAFT_TOML_LEXER_H

#include <stddef.h>

typedef enum {
	TOKEN_EOF,
	TOKEN_NEWLINE,
	TOKEN_IDENTIFIER,
	TOKEN_STRING,
	TOKEN_INTEGER,
	TOKEN_TRUE,
	TOKEN_FALSE,
	TOKEN_LBRACKET,
	TOKEN_RBRACKET,
	TOKEN_EQUAL,
	TOKEN_COMMA,
	TOKEN_DOT,
	TOKEN_ERROR
} TokenType;

typedef struct {
	TokenType type;
	char *text;
	size_t line;
	size_t column;
} Token;

typedef struct {
	const char *source;
	size_t position;
	size_t line;
	size_t column;
} Lexer;

void lexer_init(Lexer *lexer, const char *source);
Token lexer_next(Lexer *lexer);
void token_free(Token *token);

#endif
