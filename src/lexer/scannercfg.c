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

#define _GNU_SOURCE
#include <stdio.h>

#include <function.h>
#include <lexer.h>
#include <mutex.h>
#include <nvp.h>
#include <resolve.h>

/* ------------------------------------------------------------------------ */

extern inline void        _scanner_config_init(void);
static scanner_config_t * _scanner_config_new(scanner_config_t *config, va_list);
static void               _scanner_config_free(scanner_config_t *);
static char *             _scanner_config_allocstring(scanner_config_t *);
static data_t *           _scanner_config_resolve(scanner_config_t *, char *);
static scanner_config_t * _scanner_config_set(scanner_config_t *, char *, data_t *);
static data_t *           _scanner_config_call(scanner_config_t *, array_t *, dict_t *);
static scanner_config_t * _scanner_config_setvalue(scanner_config_t *, char *, data_t *);
static scanner_config_t * _scanner_config_setstring(scanner_config_t *, char *);

static vtable_t _vtable_scanner_config[] = {
  { .id = FunctionNew,          .fnc = (void_t) _scanner_config_new },
  { .id = FunctionFree,         .fnc = (void_t) _scanner_config_free },
  { .id = FunctionAllocString,  .fnc = (void_t) _scanner_config_allocstring },
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
    _scanners_configs = dict_create(NULL);
    dict_set_key_type(_scanners_configs, type_str);
    dict_set_data_type(_scanners_configs, type_int);
    _scanners_configs = strdata_dict_create();
    _scanner_config_mutex = mutex_create();
    resolve_library("libobllexer");
  }
}

scanner_config_t * _scanner_config_new(scanner_config_t *config, va_list args) {
  lexer_config_t *lexer_config;

  lexer_config = va_arg(args, lexer_config_t *);
  config -> priority = 0;
  config -> prev = config -> next = NULL;
  config -> lexer_config = lexer_config_copy(lexer_config);
  config -> match = (matcher_t) typedescr_get_function(data_typedescr((data_t *) config), FunctionUsr1);
  config -> match_2nd_pass = (matcher_t) typedescr_get_function(data_typedescr((data_t *) config), FunctionUsr2);
  config -> config = NULL;

  mdebug(lexer, "Creating scanner config '%s'. match: %p match_2nd_pass %p",
         data_typename(config), config -> match, config -> match_2nd_pass);
  return config;
}

void _scanner_config_free(scanner_config_t *config) {
  if (config) {
    lexer_config_free(config -> lexer_config);
  }
}

char * _scanner_config_allocstring(scanner_config_t *config) {
  char      *buf;
  str_t     *configbuf;
  data_t * (*conffnc)(scanner_config_t *, array_t *);
  array_t   *cfg;

  configbuf = NULL;
  cfg = data_array_create(0);
  conffnc = (data_t * (*)(scanner_config_t *, array_t *)) data_get_function((data_t *) config, FunctionUsr4);
  if (conffnc) {
    if (!conffnc(config, cfg) && array_empty(cfg)) {
      configbuf = NULL;
    }
    configbuf = array_join(cfg, ";");
  }
  if (configbuf) {
    asprintf(&buf, "%s: %s", data_typename(config), str_chars(configbuf));
    free(configbuf);
  } else {
    asprintf(&buf, "%s", data_typename(config));
  }
  array_free(cfg);
  return buf;
}

data_t * _scanner_config_resolve(scanner_config_t *config, char *name) {
  data_t *ret;

  ret = NULL;
  if (!strcmp(name, PARAM_CONFIGURATION)) {
    if (config -> config) {
      ret = (data_t *) str_copy_chars(dict_tostring(config -> config));
    } else {
      ret = data_null();
    }
  } else if (!strcmp(name, PARAM_PRIORITY)) {
    return int_to_data(config -> priority);
  } else if (config -> config) {
    ret = data_copy(data_dict_get(config -> config, name));
  }
  return ret;
}

scanner_config_t * _scanner_config_set(scanner_config_t *config, char *name, data_t *value) {
  scanner_config_t  *ret = NULL;

  if (!strcmp(name, PARAM_CONFIGURATION)) {
    ret = scanner_config_configure(config, value);
  } else if (!strcmp(name, PARAM_PRIORITY)) {
    config -> priority = data_intval(value);
  } else {
    if (lexer_debug) {
      debug("Setting value '%s' for parameter '%s' on scanner config '%s'",
            data_tostring(value), name, data_typename(config));
    }
    if (!config -> config) {
      config -> config = strdata_dict_create();
    }
    ret = config;
    dict_put(config -> config, strdup(name), data_copy(value));
  }
  return ret;
}

scanner_config_t * _scanner_config_setvalue(scanner_config_t *config, char *name, data_t *value) {
  name_t *n;

  if (name) {
    n = name_create(1, name);
    mdebug(lexer, "_scanner_config_setvalue %s[%s] := %s",
                   data_tostring((data_t *) config),
                   name, data_tostring(value));
    data_set((data_t *) config, n, value);
    name_free(n);
  }
  return config;
}

scanner_config_t * _scanner_config_setstring(scanner_config_t *config, char *value) {
  char             *ptr;
  char             *name;
  data_t           *v;
  scanner_config_t *ret = NULL;

  if (!value || !*value) {
    return NULL;
  }
  value = strdup(value);
  mdebug(lexer, "Setting config string '%s' on scanner config '%s'",
                value, data_typename(config));
  ptr = strchr(value, '=');
  if (ptr) {
    *ptr = 0;
    ptr = strtrim(ptr + 1);
    v = (data_t *) str_copy_chars(ptr);
  } else {
    v = data_true();
  }
  name = strtrim(value);

  if (strcmp(name, "configuration")) {
    ret = _scanner_config_setvalue(config, name, v);
  }
  free(value);
  return ret;
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
  dict_put(_scanners_configs, strdup(def -> type_name), (void *) ((long) def -> type));
  mutex_unlock(_scanner_config_mutex);
  return def;
}

typedescr_t * scanner_config_load(char *code, char *regfnc_name) {
  function_t  *fnc;
  char        *fncname;
  typedescr_t *ret = NULL;
  create_t     regfnc;

  _scanner_config_init();
  mutex_lock(_scanner_config_mutex);
  if (!dict_has_key(_scanners_configs, code)) {
    if (regfnc_name) {
      fncname = regfnc_name;
    } else {
      asprintf(&fncname, "%s_register", code);
    }
    fnc = function_create(fncname, NULL);
    if (fnc -> fnc) {
      regfnc = (create_t) fnc -> fnc;
      ret = regfnc();
      typedescr_assign_inheritance(ret -> type, ScannerConfig);
      scanner_config_register(ret);
    }
    function_free(fnc);
    if (fncname != regfnc_name) {
      free(fncname);
    }
  } else {
    ret = scanner_config_get(code);
  }
  mutex_unlock(_scanner_config_mutex);
  return ret;
}

typedescr_t * scanner_config_get(char *code) {
  typedescr_t *ret;
  long         type;

  _scanner_config_init();
  mutex_lock(_scanner_config_mutex);
  type = (long) dict_get(_scanners_configs, code);
  ret = typedescr_get(type);
  mutex_unlock(_scanner_config_mutex);
  return ret;
}

scanner_config_t * scanner_config_create(char *code, lexer_config_t *lexer_config) {
  typedescr_t      *type;

  type = scanner_config_get(code);
  return (type)
           ? ((scanner_config_t *) data_create(type -> type, lexer_config))
           : NULL;
}

scanner_t * scanner_config_instantiate(scanner_config_t *config, lexer_t *lexer) {
  return scanner_create(config, lexer);
}

scanner_config_t * scanner_config_configure(scanner_config_t *config, data_t *value) {
  nvp_t  *nvp;
  char   *params;
  char   *param;
  char   *sepptr;

  mdebug(lexer, "Configuring scanner '%s' with value '%s'", data_typename(config), data_tostring(value));
  if (value) {
    if (data_type(value) == NVP) {
      nvp = data_as_nvp(value);
      _scanner_config_setvalue(config, data_tostring(nvp -> name), nvp -> value);
    } else if (value != data_null()) {
      params = strdup(data_tostring(value));
      param = params;
      for (sepptr = strchr(param, ';');
           sepptr;
           sepptr = strchr(param, ';')) {
        *sepptr = 0;
        _scanner_config_setstring(config, param);
        param = sepptr + 1;
      }
      if (param && *param) {
        _scanner_config_setstring(config, param);
      }
      free(params);
    }
  }
  mdebug(lexer, "Configured scanner '%s'", data_typename(config));
  return config;
}
