/*
 * scanner.c - Copyright (c) 2016 Jan de Visser <jan@finiandarcy.com>
 *
 * This file is part of Obelix.
 *
 * Obelix is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Obelix is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Obelix.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>

#include "liblexer.h"

/* ------------------------------------------------------------------------ */

extern inline void      _scanner_init(void);
static scanner_t *      _scanner_new(scanner_t *, va_list);
static void             _scanner_free(scanner_t *);
static char *           _scanner_allocstring(scanner_t *);
static data_t *         _scanner_resolve(scanner_t *, char *);
static data_t *         _scanner_set(scanner_t *, char *, data_t *);

static vtable_t _vtable_Scanner[] = {
  { .id = FunctionNew,          .fnc = (void_t) _scanner_new },
  { .id = FunctionFree,         .fnc = (void_t) _scanner_free },
  { .id = FunctionStaticString, .fnc = (void_t) _scanner_allocstring },
  { .id = FunctionResolve,      .fnc = (void_t) _scanner_resolve },
  { .id = FunctionSet,          .fnc = (void_t) _scanner_set },
  { .id = FunctionNone,         .fnc = NULL }
};

int Scanner = -1;

/* -- S C A N N E R  ----------------------------------------------------- */

void _scanner_init(void) {
  if (Scanner < 0) {
    typedescr_register(Scanner, scanner_t);
  }
}

scanner_t * _scanner_new(scanner_t *scanner, va_list args) {
  scanner_config_t *config;
  lexer_t          *lexer;
  scanner_t        *last = NULL;
  scanner_t        *next;

  config = va_arg(args, scanner_config_t *);
  lexer = va_arg(args, lexer_t *);
  for (next = lexer -> scanners; next; next = next -> next) {
    last = next;
  }
  scanner -> next = NULL;
  scanner -> prev = last;
  if (last) {
    last -> next = scanner;
  } else {
    lexer -> scanners = scanner;
  }
  scanner -> config = scanner_config_copy(config);
  scanner -> lexer = lexer;
  scanner -> state = 0;
  scanner -> data = NULL;
  debug(lexer, "Created scanner of type '%s'. match: %p", data_typename(scanner -> config), scanner -> config -> match);
  return scanner;
}

void _scanner_free(scanner_t *scanner) {
  void (*destroy_scanner)(scanner_t *);

  if (scanner) {
    destroy_scanner = (void (*)(scanner_t *)) data_get_function((data_t *) scanner -> config, FunctionDestroyScanner);
    if (destroy_scanner && scanner -> data) {
      destroy_scanner(scanner -> data);
    } else {
      free(scanner -> data);
    }
    scanner_config_free(scanner -> config);
  }
}

char * _scanner_allocstring(scanner_t *scanner) {
  char *buf;

  asprintf(&buf, "'%s' scanner", data_typename(scanner -> config));
  return buf;
}

data_t * _scanner_resolve(scanner_t *scanner, char *name) {
  return NULL;
}

data_t * _scanner_set(scanner_t *scanner, char *name, data_t *value) {
  return NULL;
}

scanner_t * scanner_create(scanner_config_t *config, lexer_t *lexer) {
  scanner_t *ret;

  _scanner_init();
  mdebug(lexer, "Creating scanner of type '%s'", data_typename(config));
  ret = (scanner_t *) data_create(Scanner, config, lexer);
  return ret;
}

scanner_t * scanner_reconfigure(scanner_t *scanner, char *param, data_t *value) {
  scanner_t * (*reconfigfnc)(scanner_t *, char *, data_t *);
  reconfigfnc = (scanner_t * (*)(scanner_t *, char *, data_t *))
    data_get_function((data_t *) scanner -> config, FunctionReconfigScanner);
  return (reconfigfnc) ? reconfigfnc(scanner, param, value) : NULL;
}
