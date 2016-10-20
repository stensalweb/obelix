/*
 * lexercfg.c - Copyright (c) 2016 Jan de Visser <jan@finiandarcy.com>
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

#include <ctype.h>
#include <stdio.h>

#include <exception.h>
#include <lexer.h>
#include "liblexer.h"

/* ------------------------------------------------------------------------ */

extern inline void _lexer_config_init(void);
static void        _lexer_config_free(lexer_config_t *);
static char *      _lexer_config_staticstring(lexer_config_t *);
static data_t *    _lexer_config_resolve(lexer_config_t *, char *);
static data_t *    _lexer_config_set(lexer_config_t *, char *, data_t *);
static data_t *    _lexer_config_mth_add_scanner(lexer_config_t *, char *, array_t *, dict_t *);
static data_t *    _lexer_config_mth_tokenize(lexer_config_t *, char *, array_t *, dict_t *);

static vtable_t _vtable_lexer_config[] = {
  { .id = FunctionFree,         .fnc = (void_t) _lexer_config_free },
  { .id = FunctionStaticString, .fnc = (void_t) _lexer_config_staticstring },
  { .id = FunctionResolve,      .fnc = (void_t) _lexer_config_resolve },
  { .id = FunctionSet,          .fnc = (void_t) _lexer_config_set },
  { .id = FunctionNone,         .fnc = NULL }
};

static methoddescr_t _methoddescr_lexer_config[] = {
  { .type = -1,     .name = "add",      .method = (method_t) _lexer_config_mth_add_scanner, .argtypes = { Any, NoType, NoType },      .minargs = 1, .varargs = 0 },
  { .type = -1,     .name = "tokenize", .method = (method_t) _lexer_config_mth_tokenize,    .argtypes = { InputStream, Any, NoType }, .minargs = 1, .varargs = 0 },
  { .type = NoType, .name = NULL,       .method = NULL,                .argtypes = { NoType, NoType, NoType }, .minargs = 0, .varargs = 0 }
};

int LexerConfig = -1;

/* -- L E X E R  C O N F I G --------------------------------------------- */

void _lexer_config_init(void) {
  if (LexerConfig < 0) {
    LexerConfig = typedescr_create_and_register(LexerConfig, "Lexer_config", _vtable_lexer_config, _methoddescr_lexer_config);
  }
}

void _lexer_config_free(lexer_config_t *config) {
  scanner_config_t *scanner, *prev;

  if (config) {
    scanner = config -> scanners;
    while (scanner) {
      prev = scanner -> prev;
      scanner_config_free(scanner);
      scanner = prev;
    }
  }
  data_free(config -> data);
}

char * _lexer_config_staticstring(lexer_config_t *config) {
  return "Lexer Configuration";
}

data_t * _lexer_config_resolve(lexer_config_t *config, char *name) {
  scanner_config_t *scanner;

  if (!strcmp(name, "buffersize")) {
    return int_to_data(config -> bufsize);
  } else {
    for (scanner = config -> scanners; scanner; scanner = scanner -> next) {
      if (!strcmp(data_typename(scanner), name)) {
        return (data_t *) scanner;
      }
    }
    return NULL;
  }
}

data_t * _lexer_config_set(lexer_config_t *config, char *name, data_t *value) {
  data_t *ret = NULL;

  if (!strcmp(name, "buffersize")) {
    if (!value) {
      lexer_config_set_bufsize(config, LEXER_BUFSIZE);
    } else if (data_is_int(value)) {
      lexer_config_set_bufsize(config, data_intval(value));
    } else {
      ret = data_exception(ErrorType,
                           "LexerConfig.buffersize expects 'int', not '%s'",
                           data_typedescr(value) -> type_name);
    }
  }
  return ret;
}

data_t * _lexer_config_mth_add_scanner(lexer_config_t *config, char *n, array_t *args, dict_t *kwargs) {
  data_t           *code;

  code = data_array_get(args, 0);
  return (data_t *) lexer_config_add_scanner(config, data_tostring(code));

}

data_t * _lexer_config_mth_tokenize(lexer_config_t *config, char *n, array_t *args, dict_t *kwargs) {
  lexer_t *lexer;
  array_t *tail;
  data_t  *ret;

  lexer = lexer_create(config, data_array_get(args, 0));
  tail = array_slice(args, 1, 0);
  ret = data_call((data_t *) lexer, tail, kwargs);
  lexer_free(lexer);
  array_free(tail);
  return ret;
}

/* ------------------------------------------------------------------------ */

scanner_config_t * _lexer_config_add_scanner(lexer_config_t *config, char *code) {
  scanner_config_t *scanner;
  scanner_config_t *next;
  scanner_config_t *prev;
  int               priority;

  scanner = scanner_config_create(code, config);
  if (scanner) {
    priority = scanner -> priority;
    for (next = config -> scanners, prev = NULL;
         next && next -> priority >= priority;
         next = next -> next) {
      prev = next;
    };
    scanner -> next = next;
    scanner -> prev = prev;
    if (next) {
      scanner -> prev = next -> prev;
      next -> prev = scanner;
    }
    if (prev) {
      prev -> next = scanner;
    } else {
      config -> scanners = scanner;
    }
    config -> num_scanners++;
  }
  mdebug(lexer, "Created scanner config '%s' match: %p", scanner_config_tostring(scanner), scanner -> match);
  return scanner;
}

/* ------------------------------------------------------------------------ */

lexer_config_t * lexer_config_create(void) {
  lexer_config_t *ret;

  _lexer_config_init();
  ret = data_new(LexerConfig, lexer_config_t);
  ret -> bufsize = LEXER_BUFSIZE;
  ret -> scanners = NULL;
  ret -> num_scanners = 0;
  return ret;
}

scanner_config_t * lexer_config_add_scanner(lexer_config_t *config, char *code_config) {
  char             *copy;
  char             *ptr = NULL;
  char             *code;
  char             *param = NULL;
  data_t           *param_data;
  scanner_config_t *scanner = NULL;

  copy = strdup(code_config);
  ptr = strchr(copy, ':');
  if (ptr) {
    *ptr = 0;
    param = ptr + 1;
    if (!*param) {
      param = NULL;
    } else {
      param = strtrim(param);
    }
  }
  code = strtrim(code);
  if (*code) {
    scanner = lexer_config_get_scanner(config, code);
    if (!scanner) {
      scanner = _lexer_config_add_scanner(config, code);
    }
    if (scanner && param) {
      param_data = (data_t *) str_wrap(param);
      scanner_config_configure(scanner, param_data);
      data_free(param_data);
    }
  }
  free(copy);
  return scanner;
}

scanner_config_t * lexer_config_get_scanner(lexer_config_t *config, char *code) {
  scanner_config_t *scanner = config -> scanners;

  for (scanner = config -> scanners;
       scanner && strcmp(data_typename((data_t *) scanner), code);
       scanner = scanner -> next);
  return scanner;
}

data_t * lexer_config_set(lexer_config_t *config, char *code, data_t *param) {
  scanner_config_t *scanner;

  scanner = lexer_config_get_scanner(config, code);
  if (!scanner) {
    return data_exception(ErrorParameterValue, "No scanner with code '%s' found", code);
  }
  return scanner_config_configure(scanner, param)
         ? (data_t *) config
         : data_exception(ErrorParameterValue,
                          "Could not set parameter '%s' on scanner with code '%s'",
                          data_tostring(param), code);
}

int lexer_config_get_bufsize(lexer_config_t *config) {
  return config -> bufsize;
}

lexer_config_t * lexer_config_set_bufsize(lexer_config_t *config, int bufsize) {
  config -> bufsize = bufsize;
  return config;
}

lexer_config_t * lexer_config_tokenize(lexer_config_t *config, reduce_t tokenizer, data_t *stream) {
  lexer_t *lexer;

  lexer = lexer_create(config, stream);
  lexer_tokenize(lexer, tokenizer, config);
  lexer_free(lexer);
  return config;
}

lexer_config_t * lexer_config_dump(lexer_config_t *config) {
  scanner_config_t        *scanner;

  printf("  lexer_config = lexer_config_create();\n");
  printf("  lexer_config_set_bufsize(lexer_config, %d)\n",
         lexer_config_get_bufsize(config));
  for (scanner = config -> scanners; scanner; scanner = scanner -> next) {
    printf("  scanner_config = lexer_config_add_scanner(lexer_config, \"%s\")\n",
           scanner_config_tostring(scanner));
    scanner_config_dump(scanner);
  }
  return config;
}