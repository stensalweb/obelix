/*
 * scannercfg.c - Copyright (c) 2016 Jan de Visser <jan@finiandarcy.com>
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
#include <lexer.h>
#include <mutex.h>
#include <name.h>
#include <nvp.h>

/* ------------------------------------------------------------------------ */

static inline void        _scanner_config_init(void);
static scanner_config_t * _scanner_config_new(scanner_config_t *config, va_list);
static void               _scanner_config_free(scanner_config_t *);
static char *             _scanner_config_allocstring(scanner_config_t *);
static data_t *           _scanner_config_resolve(scanner_config_t *, char *);
static data_t *           _scanner_config_set(scanner_config_t *, char *, data_t *);
static data_t *           _scanner_config_call(scanner_config_t *, array_t *, dict_t *);
static scanner_config_t * _scanner_config_setvalue(scanner_config_t *, char *, data_t *);
static scanner_config_t * _scanner_config_setstring(scanner_config_t *, char *);

static vtable_t _vtable_scanner_config[] = {
  { .id = FunctionNew,          .fnc = (void_t) _scanner_config_new },
  { .id = FunctionFree,         .fnc = (void_t) _scanner_config_free },
  { .id = FunctionStaticString, .fnc = (void_t) _scanner_config_allocstring },
  { .id = FunctionResolve,      .fnc = (void_t) _scanner_config_resolve },
  { .id = FunctionSet,          .fnc = (void_t) _scanner_config_set },
  { .id = FunctionCall,         .fnc = (void_t) _scanner_config_call },
  { .id = FunctionNone,         .fnc = NULL }
};

int              ScannerConfig = -1;
static dict_t *  _scanners_configs;
static mutex_t * _scanner_config_mutex;

/* -- S C A N N E R  C O N F I G ----------------------------------------- */

void _scanner_config_init(void) {
  if (ScannerConfig < 0) {
    ScannerConfig = typedescr_create_and_register(ScannerConfig,
                                                  "scanner_config",
                                                  _vtable_scanner_config,
                                                  NULL);
    _scanners_configs = strdata_dict_create();
    _scanner_config_mutex = mutex_create();
  }
}

scanner_config_t * _scanner_config_new(scanner_config_t *config, va_list args) {
  lexer_config_t *lexer_config;

  lexer_config = va_arg(args, lexer_config_t *);
  config -> prev = config -> next = NULL;
  config -> lexer_config = lexer_config_copy(lexer_config);
  config -> match = typedescr_get_function(data_type(config), FunctionUsr1);
  config -> match_2nd_pass = typedescr_get_function(data_type(config), FunctionUsr2);
  config -> config = NULL;

  lexer_config -> num_scanners++;
  return config;
}

void _scanner_config_free(scanner_config_t *config) {
  if (config) {
    lexer_config_free(config -> lexer_config);
  }
}

char * _scanner_config_allocstring(scanner_config_t *config) {
  char *buf;

  asprintf(&buf, "Configuration for '%s' scanner", data_typename(config));
  return buf;
}

data_t * _scanner_config_resolve(scanner_config_t *config, char *name) {
  data_t *ret;

  if (!strcmp(name, "configuration")) {
    if (config -> config) {
      ret = (data_t *) str_copy_chars(dict_tostring(config -> config));
    } else {
      ret = data_null();
    }
  } else if (config -> config) {
    ret = data_copy(data_dict_get(config -> config, name));
  }
  return ret;
}

data_t * _scanner_config_set(scanner_config_t *config, char *name, data_t *value) {
  data_t  *ret = NULL;
  array_t *args;

  if (!strcmp(name, "configuration")) {
    ret = (data_t *) scanner_config_configure(config, value);
  } else {
    if (!config -> config) {
      config -> config = strdata_dict_create();
    }
    ret = (value) ? data_copy(value) : data_null();
    dict_put(config -> config, strdup(name), ret);
  }
  return ret;
}

scanner_config_t * _scanner_config_setvalue(scanner_config_t *config, char *name, data_t *value) {
  name_t *n;

  if (name) {
    n = name_create(1, name);
    data_set((data_t *) config, n, value);
    name_free(n);
  }
  return config;
}

scanner_config_t * _scanner_config_setstring(scanner_config_t *config, char *value) {
  char   *ptr;
  data_t *v;

  ptr = strpbrk(value, ":=");
  if (ptr) {
    *ptr = 0;
    v = (data_t *) str_copy_chars(ptr + 1);
  } else {
    v = data_true();
  }
  if (strcmp(value, "configuration")) {
    return _scanner_config_set(config, strdup(value), v);
  } else {
    return NULL;
  }
}

data_t * _scanner_config_call(scanner_config_t *config, array_t *args, dict_t *kwargs) {
  lexer_t *lexer;

  lexer = (lexer_t *) data_array_get(args, 0);
  return (data_t *) scanner_config_instantiate(config, lexer);
}

/* ------------------------------------------------------------------------ */

int scanner_config_typeid(void) {
  _scanner_config_init();
  return ScannerConfig;
}

typedescr_t * scanner_config_register(typedescr_t *def) {
  _scanner_config_init();
  mutex_lock(_scanner_config_mutex);
  dict_put(_scanners_configs, strdup(def -> type_name), def);
  mutex_unlock(_scanner_config_mutex);
  return def;
}

typedescr_t * scanner_config_get(char *code) {
  typedescr_t *ret;

  _scanner_config_init();
  mutex_lock(_scanner_config_mutex);
  ret = (typedescr_t *) dict_get(_scanners_configs, code);
  mutex_unlock(_scanner_config_mutex);
  return ret;
}

scanner_config_t * scanner_config_create(char *code, lexer_config_t *lexer_config) {
  typedescr_t      *type;

  type = scanner_config_get(code);
  return (type) ? data_create(type -> type, lexer_config) : NULL;
}

scanner_t * scanner_config_instantiate(scanner_config_t *config, lexer_t *lexer) {
  return (scanner_t *) data_create(Scanner, config, lexer);
}

scanner_config_t * scanner_config_configure(scanner_config_t *config, data_t *value) {
  nvp_t  *nvp;t
  char   *params;
  char   *param;
  char   *saveptr;

  if (data_type(value) == NVP) {
    nvp = data_as_nvp(value);
    _scanner_config_setvalue(config, data_tostring(nvp -> name), nvp -> value);
  } else {
    params = strdup(value ? data_tostring(value) : "");
    for (param = strtok_r(params, SCANNER_CONFIG_SEPARATORS, &saveptr);
         param;
         param = strtok_r(NULL, SCANNER_CONFIG_SEPARATORS, &saveptr)) {
      _scanner_config_setstring(config, param);
    }
    free(params);
  }
  return config;
}