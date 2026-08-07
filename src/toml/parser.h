/*
 * Copyright (C) 2026 tas0dev
 * This software is licensed under the GNU General Public License, version 3
 * only.
 *
 * Created by tas0dev
 */

#ifndef CRAFT_TOML_PARSER_H
#define CRAFT_TOML_PARSER_H

#include "toml.h"

TomlDocument *parser_parse(const char *source, TomlError *error);

#endif
