/*
 * /obelix/src/parser/loader.c - Copyright (c) 2015 Jan de Visser <jan@finiandarcy.com>
 *
 * This file is part of obelix.
 *
 * obelix is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * obelix is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with obelix.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include <stdio.h>

#include <data.h>
#include <exception.h>
#include <file.h>
#include <fsentry.h>
#include <grammarparser.h>
#include <loader.h>
#include <namespace.h>
#include <thread.h>

extern grammar_t *      build_grammar(void);

static void             _scriptloader_init(void) __attribute__((constructor));
static file_t *         _scriptloader_open_file(scriptloader_t *, char *, module_t *);
static data_t *         _scriptloader_open_reader(scriptloader_t *, module_t *);
static data_t *         _scriptloader_get_object(scriptloader_t *, int, ...);
static data_t *         _scriptloader_set_value(scriptloader_t *, data_t *, char *, data_t *);
static data_t *         _scriptloader_import_sys(scriptloader_t *, array_t *);
static void             _scriptloader_free(scriptloader_t *);
static char *           _scriptloader_allocstring(scriptloader_t *);
static data_t *         _scriptloader_call(scriptloader_t *, array_t *, dict_t *);
static data_t *         _scriptloader_resolve(scriptloader_t *, char *);

static code_label_t obelix_option_labels[] = {
  { .code = ObelixOptionList,  .label = "ObelixOptionList" },
  { .code = ObelixOptionTrace, .label = "ObelixOptionTrace" },
  { .code = ObelixOptionLAST,  .label = NULL }
};

int ScriptLoader = -1;

static vtable_t _vtable_scriptloader[] = {
  { .id = FunctionFree,        .fnc = (void_t) _scriptloader_free },
  { .id = FunctionAllocString, .fnc = (void_t) _scriptloader_allocstring },
  { .id = FunctionCall,        .fnc = (void_t) _scriptloader_call },
  { .id = FunctionResolve,     .fnc = (void_t) _scriptloader_resolve },
  { .id = FunctionNone,        .fnc = NULL }
};

/* ------------------------------------------------------------------------ */

void _scriptloader_init(void) {
  ScriptLoader = typedescr_create_and_register(ScriptLoader, "loader", 
                                               _vtable_scriptloader, NULL);
}

/* -- S C R I P T L O A D E R   D A T A   F U N C T I O N S --------------- */

void _scriptloader_free(scriptloader_t *loader) {
  if (loader) {
    free(loader -> system_dir);
    data_free(loader -> load_path);
    array_free(loader -> options);
    grammar_free(loader -> grammar);
    parser_free(loader -> parser);
    ns_free(loader -> ns);
  }
}

char *_scriptloader_allocstring(scriptloader_t *loader) {
  char *buf;
  
  asprintf(&buf, "Loader(%s)", loader -> system_dir);
  return buf;
}

data_t *_scriptloader_call(scriptloader_t *loader, array_t *args, dict_t *kwargs) {
  name_t  *name = data_as_name(data_array_get(args, 0));
  array_t *args_shifted = array_slice(args, 1, -1);
  data_t  *ret;
  
  ret = scriptloader_run(loader, name, args_shifted, kwargs);
  free(args_shifted);
  return ret;
}

data_t * _scriptloader_resolve(scriptloader_t *loader, char *name) {
  if (!strcmp(name, "list")) {
    return data_create(Bool, scriptloader_get_option(loader, ObelixOptionList));
  } else if (!strcmp(name, "trace")) {
    return data_create(Bool, scriptloader_get_option(loader, ObelixOptionTrace));
  } else if (!strcmp(name, "loadpath")) {
    return data_copy((data_t *) loader -> load_path);
  } else if (!strcmp(name, "systempath")) {
    return data_create(String, loader -> system_dir);
  } else if (!strcmp(name, "grammar")) {
    return data_copy((data_t *) loader -> grammar);
  } else if (!strcmp(name, "parser")) {
    return data_copy((data_t *) loader -> parser);
  } else if (!strcmp(name, "namespace")) {
    return data_copy((data_t *) loader -> ns);
  } else {
    return NULL;
  }
}

data_t * _scriptloader_set(scriptloader_t *loader, char *name, data_t *value) {
  if (!strcmp(name, "list")) {
    scriptloader_set_option(loader, ObelixOptionList, data_intval(value));
    return value;
  } else if (!strcmp(name, "trace")) {
    scriptloader_set_option(loader, ObelixOptionTrace, data_intval(value));
    return value;
  }
  return data_exception(ErrorType,
                        "Cannot set '%s' on objects of type '%s'",
                        name, data_typename(loader));
}

/* ------------------------------------------------------------------------ */

static file_t * _scriptloader_open_file(scriptloader_t *loader, 
                                        char *basedir, 
                                        module_t *mod) {
  char      *fname;
  char      *ptr;
  fsentry_t *e;
  fsentry_t *init;
  file_t    *ret;
  char      *name;
  char      *buf;
  name_t    *n = mod -> name;

  assert(*(basedir + (strlen(basedir) - 1)) == '/');
  buf = strdup(name_tostring_sep(n, "/"));
  name = buf;
  if (script_debug) {
    debug("_scriptloader_open_file('%s', '%s')", basedir, name);
  }
  while (name && ((*name == '/') || (*name == '.'))) {
    name++;
  }
  fname = new(strlen(basedir) + strlen(name) + 5);
  sprintf(fname, "%s%s", basedir, name);
  for (ptr = strchr(fname + strlen(basedir), '.'); ptr; ptr = strchr(ptr, '.')) {
    *ptr = '/';
  }
  e = fsentry_create(fname);
  init = NULL;
  if (fsentry_isdir(e)) {
    init = fsentry_getentry(e, "__init__.obl");
    fsentry_free(e);
    if (fsentry_exists(init)) {
      e = init;
    } else {
      e = NULL;
      fsentry_free(init);
    }
  } else {
    strcat(fname, ".obl");
    e = fsentry_create(fname);
  }
  if ((e != NULL) && fsentry_isfile(e) && fsentry_canread(e)) {
    ret = fsentry_open(e);
    mod -> source = data_create(String, e -> name);
    assert(ret -> fh > 0);
  } else {
    ret = NULL;
  }
  free(buf);
  fsentry_free(e);
  free(fname);
  return ret;
}

data_t * _scriptloader_open_reader(scriptloader_t *loader, module_t *mod) {
  file_t *text = NULL;
  name_t *name = mod -> name;
  data_t *path_entry;
  int     ix;

  assert(loader);
  assert(name);
  if (script_debug) {
    debug("_scriptloader_open_reader('%s')", name_tostring(name));
  }
  for (ix = 0; !text && (ix < data_list_size(loader -> load_path)); ix++) {
    path_entry = data_list_get(loader -> load_path, ix);
    text = _scriptloader_open_file(loader, data_tostring(path_entry), mod);
  }
  return (data_t *) text;
}

static data_t * _scriptloader_get_object(scriptloader_t *loader, int count, ...) {
  va_list   args;
  int       ix;
  object_t *obj;
  module_t *mod;
  data_t   *data = NULL;
  name_t   *name;
  
  va_start(args, count);
  name = name_create(0);
  for (ix = 0; ix < count; ix++) {
    name_extend(name, va_arg(args, char *));
  }
  va_end(args);
  data = ns_get(loader -> ns, name);
  if (data_is_module(data)) {
    mod = data_as_module(data);
    data = data_create(Object, mod -> obj);
  }
  return data;
}

static data_t * _scriptloader_set_value(scriptloader_t *loader, data_t *obj,
                                        char *name, data_t *value) {
  name_t *n;
  data_t *ret;
  
  n = name_create(1, name);
  ret = data_set(obj, n, value);
  if (!(ret && data_is_exception(ret))) {
    data_free(value);
    ret = NULL;
  }
  name_free(n);
  return ret;
}

static data_t * _scriptloader_import_sys(scriptloader_t *loader, 
                                         array_t *user_path) {
  name_t *name;
  data_t *sys;
  data_t *val;
  data_t *ret;
  
  name = name_create(1, "sys");
  sys = scriptloader_import(loader, name);
  name_free(name);
  if (!data_is_exception(sys)) {
    scriptloader_extend_loadpath(loader, user_path);
    ret = _scriptloader_set_value(loader, sys, "path", 
                                  data_copy(loader -> load_path));
    data_free(sys);
  } else {
    ret = sys;
  }
  return ret;
}

/* -- S C R I P T L O A D E R _ T   P U B L I C   F U N C T I O N S ------- */

scriptloader_t * scriptloader_create(char *sys_dir, array_t *user_path, 
                                     char *grammarpath) {
  scriptloader_t   *ret;
  grammar_parser_t *gp;
  data_t           *file;
  data_t           *root;
  char             *userdir;
  int               ix;
  int               len;
  array_t          *a;

  if (script_debug) {
    debug("Creating script loader");
  }
  ret = data_new(ScriptLoader, scriptloader_t);
  if (!sys_dir) {
    sys_dir = getenv("OBELIX_SYS_PATH");
  }
  if (!sys_dir) {
    sys_dir = OBELIX_DATADIR;
  }
  len = strlen(sys_dir);
  ret -> system_dir = (char *) new (len + ((*(sys_dir + (len - 1)) != '/') ? 2 : 1));
  strcpy(ret -> system_dir, sys_dir);
  if (*(ret -> system_dir + (strlen(ret -> system_dir) - 1)) != '/') {
    strcat(ret -> system_dir, "/");
  }
  
  if (!user_path || !array_size(user_path)) {
    if (user_path) {
      array_free(user_path);
    }
    user_path = array_split(getenv("OBELIX_USER_PATH"), ":");
  }
  if (!user_path || !array_size(user_path)) {
    if (!user_path) {
      str_array_create(1);
    }
    array_push(user_path, strdup("./"));
  }

  if (script_debug) {
    debug("system dir: %s", ret -> system_dir);
    debug("user path: %s", array_tostring(user_path));
  }

  if (!grammarpath) {
    if (script_debug) {
      debug("Using stock, compiled-in grammar");
    }
    ret -> grammar = build_grammar();
  } else {
    if (script_debug) {
      debug("grammar file: %s", grammarpath);
    }
    file = data_create(File, grammarpath);
    assert(file_isopen(data_as_file(file)));
    gp = grammar_parser_create(file);
    ret -> grammar = grammar_parser_parse(gp);
    assert(ret -> grammar);
    grammar_parser_free(gp);
    data_free(file);
  }

  if (script_debug) {
    debug("  Loaded grammar");
  }
  ret -> options = data_array_create((int) ObelixOptionLAST);
  for (ix = 0; ix < (int) ObelixOptionLAST; ix++) {
    scriptloader_set_option(ret, ix, 0L);
  }

  ret -> parser = parser_create(ret -> grammar);
  if (script_debug) {
    debug("  Created parser");
  }

  ret -> load_path = data_create(List, 1, data_create(String, ret -> system_dir));
  ret -> ns = ns_create("loader", ret, (import_t) scriptloader_load);
  root = ns_import(ret -> ns, NULL);
  if (!data_is_module(root)) {
    error("Error initializing loader scope: %s", data_tostring(root));
    ns_free(ret -> ns);
    ret -> ns = NULL;
  } else {
    if (script_debug) {
      debug("  Created loader namespace");
    }
    _scriptloader_import_sys(ret, user_path);
  }
  data_free(root);
  if (!ret -> ns) {
    error("Could not initialize loader root namespace");
    scriptloader_free(ret);
    ret = NULL;
  } else {
    data_thread_set_kernel((data_t *) ret);
  }
  if (script_debug) {
    debug("scriptloader created");
  }
  return ret;
}

scriptloader_t * scriptloader_get(void) {
  return (scriptloader_t *) data_thread_kernel();
}

scriptloader_t * scriptloader_set_option(scriptloader_t *loader, obelix_option_t option, long value) {
  data_t *opt = data_create(Int, value);
  
  array_set(loader -> options, (int) option, opt);
  return loader;
}

long scriptloader_get_option(scriptloader_t *loader, obelix_option_t option) {
  data_t *opt;
  
  opt = data_array_get(loader -> options, (int) option);
  return data_intval(opt);
}

scriptloader_t * scriptloader_add_loadpath(scriptloader_t *loader, char *pathentry) {
  char *sanitized_entry;
  int   len;
  
  len = strlen(pathentry);
  sanitized_entry = (char *) new (len + ((*(pathentry + (len - 1)) != '/') ? 2 : 1));
  strcpy(sanitized_entry, pathentry);
  if (*(sanitized_entry + (strlen(sanitized_entry) - 1)) != '/') {
    strcat(sanitized_entry, "/");
  }
  data_list_push(loader -> load_path, data_create(String, sanitized_entry));
  free(sanitized_entry);
  return loader;
}

scriptloader_t * scriptloader_extend_loadpath(scriptloader_t *loader, array_t *path) {
  int ix;
  
  for (ix = 0; ix < array_size(path); ix++) {
    scriptloader_add_loadpath(loader, str_array_get(path, ix));
  }
  return loader;
}

data_t * scriptloader_load_fromreader(scriptloader_t *loader, module_t *mod, data_t *reader) {
  data_t   *ret = NULL;
  script_t *script;
  char     *name;
  
  if (script_debug) {
    debug("scriptloader_load_fromreader('%s')", name_tostring(mod -> name));
  }
  parser_clear(loader -> parser);
  parser_set(loader -> parser, "module", data_create(Module, mod));
  name = strdup((name_size(mod -> name)) ? name_tostring(mod -> name) : "__root__");
  parser_set(loader -> parser, "name", data_create(String, name));
  parser_set(loader -> parser, "options", data_create_list(loader -> options));
  ret = parser_parse(loader -> parser, reader);
  if (!ret) {
    ret = data_copy(parser_get(loader -> parser, "script"));
  }
  return ret;
}

data_t * scriptloader_import(scriptloader_t *loader, name_t *name) {
  return ns_import(loader -> ns, name);
}

data_t * scriptloader_load(scriptloader_t *loader, module_t *mod) {
  data_t *rdr;
  data_t *ret;
  char   *script_name;
  name_t *name = mod -> name;

  assert(loader);
  assert(name);
  script_name = strdup((name && name_size(mod -> name)) ? name_tostring(mod -> name) : "__root__");
  if (script_debug) {
    debug("scriptloader_load('%s')", script_name);
  }
  if (mod -> state == ModStateLoading) {
    rdr = _scriptloader_open_reader(loader, mod);
    ret = (rdr)
            ? scriptloader_load_fromreader(loader, mod, rdr)
            : data_exception(ErrorName, "Could not load '%s'", script_name);
    data_free(rdr);
  } else {
    debug("Module '%s' is already active. Skipped.", 
          name_tostring(mod -> name));
  }
  free(script_name);
  return ret;
}

data_t * scriptloader_run(scriptloader_t *loader, name_t *name, array_t *args, dict_t *kwargs) {
  data_t   *data;
  object_t *obj;
  data_t   *sys;
  
  sys = _scriptloader_get_object(loader, 1, "sys");
  if (sys && !data_is_exception(sys)) {
    _scriptloader_set_value(loader, sys, "argv", data_create_list(args));
    if (scriptloader_get_option(loader, ObelixOptionTrace)) {
      logging_enable("trace");
    }
    data_free(sys);
    data = ns_execute(loader -> ns, name, args, kwargs);
    if (obj = data_as_object(data)) {
      data = data_copy(obj -> retval);
      object_free(obj);
    } else if (!data_is_exception(data)) {
      data = data_exception(ErrorInternalError,
                            "ns_execute '%s' returned '%s', a %s",
                            name_tostring(name),
                            data_tostring(data),
                            data_typename(data));
    }
    logging_disable("trace");
  } else {
    data = (sys) ? sys : data_exception(ErrorName, "Could not resolve module 'sys'");
  }
  return data;
}
