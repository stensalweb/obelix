/*
 * /obelix/src/parser/namespace.c - Copyright (c) 2015 Jan de Visser <jan@finiandarcy.com>
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

#include <stdio.h>

#include <array.h>
#include <exception.h>
#include <namespace.h>

static void          _data_init_module(void) __attribute__((constructor));
static data_t *      _data_new_module(data_t *, va_list);
static data_t *      _data_copy_module(data_t *, data_t *);
static int           _data_cmp_module(data_t *, data_t *);
static char *        _data_tostring_module(data_t *);
static unsigned int  _data_hash_module(data_t *);
static data_t *      _data_resolve_module(data_t *, char *);
static data_t *      _data_call_module(data_t *, array_t *, dict_t *);

static namespace_t * _ns_create(void);
static module_t *    _ns_add(namespace_t *, name_t *);

int ns_debug = 0;

vtable_t _vtable_module[] = {
  { .id = MethodNew,      .fnc = (void_t) _data_new_module },
  { .id = MethodCopy,     .fnc = (void_t) _data_copy_module },
  { .id = MethodCmp,      .fnc = (void_t) _data_cmp_module },
  { .id = MethodFree,     .fnc = (void_t) mod_free },
  { .id = MethodToString, .fnc = (void_t) _data_tostring_module },
  { .id = MethodHash,     .fnc = (void_t) _data_hash_module },
  { .id = MethodResolve,  .fnc = (void_t) _data_resolve_module },
  { .id = MethodCall,     .fnc = (void_t) _data_call_module },
  { .id = MethodNone,     .fnc = NULL }
};

static typedescr_t typedescr_module = {
  .type       = Module,
  .type_name  = "module",
  .vtable     = _vtable_module
};

/*
 * Module data functions
 */

void _data_init_module(void) {
  typedescr_register(&typedescr_module);
}

data_t * _data_new_module(data_t *ret, va_list arg) {
  module_t *module;

  module = va_arg(arg, module_t *);
  ret -> ptrval = module;
  ((module_t *) ret -> ptrval) -> refs++;
  return ret;
}

data_t * _data_copy_module(data_t *target, data_t *src) {
  target -> ptrval = src -> ptrval;
  ((module_t *) target -> ptrval) -> refs++;
  return target;
}

int _data_cmp_module(data_t *d1, data_t *d2) {
  module_t *s1;
  module_t *s2;

  s1 = (module_t *) d1 -> ptrval;
  s2 = (module_t *) d2 -> ptrval;
  return strcmp(s1 -> name, s2 -> name);
}

char * _data_tostring_module(data_t *d) {
  return mod_tostring((module_t *) d -> ptrval);
}

unsigned int _data_hash_module(data_t *data) {
  return object_hash(data_moduleval(data) -> obj);
}

data_t * _data_resolve_module(data_t *mod, char *name) {
  return mod_resolve(data_moduleval(mod), name);
}

data_t * _data_call_module(data_t *mod, array_t *args, dict_t *kwargs) {
  module_t *m = data_moduleval(mod);
  
  return object_call(m -> obj, args, kwargs);
}

data_t * data_create_module(module_t *module) {
  return data_create(Module, module);
}

/* ------------------------------------------------------------------------ */

module_t * mod_create(name_t *name) {
  module_t   *ret;
  
  ret = NEW(module_t);
  if (ns_debug) {
    debug("  Creating module '%s'", name);
  }
  ret -> name = strdup(name_tostring(name));
  ret -> contents = strdata_dict_create();
  ret -> obj = object_create(NULL);
  ret -> refs = 0;
  return ret;
}

void mod_free(module_t *mod) {
  if (mod) {
    mod -> refs--;
    if (mod -> refs <= 0) {
      dict_free(mod -> contents);
      object_free(mod -> obj);
      free(mod -> name);
      free(mod);
    }
  }
}

module_t * mod_copy(module_t *module) {
  module -> refs++;
  return module;
}

char * mod_tostring(module_t *module) {
  free(module -> str);
  
  module -> str = (char *) new(snprintf(NULL, 0, "<<module %s>>", module -> name));
  sprintf(module -> str, "<<module %s>>", module -> name);
  return module -> str;
}

int mod_has_module(module_t *mod, char *name) {
  return dict_has_key(mod -> contents, name);
}

data_t * mod_get_module(module_t *mod, char *name) {
  return (data_t *) dict_get(mod -> contents, name);
}

module_t * mod_add_module(module_t *mod, name_t *name) {
  module_t *ret = mod_create(name);
  
  dict_put(mod -> contents, strdup(name_last(name)), data_create(Module, ret));
  return ret;
}

data_t * mod_set(module_t *mod, script_t *script) {
  data_t *data;
  
  data = script_create_object(script, NULL, NULL);
  if (data_is_object(data)) {
    object_free(mod -> obj);
    mod -> obj = object_copy(data_objectval(data));
    if (ns_debug) {
      debug("  module '%s' initialized", mod -> name);
    }
  } else {
    error("ERROR initializing module '%s': %s", mod -> name, data_tostring(data));
  }
  return data;
}

object_t * mod_get(module_t *mod) {
  return (mod -> obj) ? object_copy(mod -> obj) : NULL;
}

data_t * mod_resolve(module_t *mod, char *name) {
  data_t *ret;
  
  /* 
   * You can shadow subpackages by explicitely having an attribute in the 
   * object.
   */
  ret = object_resolve(mod -> obj, name);
  if (!ret) {
    ret = mod_get_module(mod, name);
  }
}

/* ------------------------------------------------------------------------ */

namespace_t * _ns_create(void) {
  namespace_t *ret;

  ret = NEW(namespace_t);
  ret -> root = data_create(Module, mod_create(name_create(0)));
  ret -> import_ctx = NULL;
  ret -> up = NULL;
  return ret;
}

module_t * _ns_add(namespace_t *ns, name_t *name) {
  name_t   *current = name_create(0);
  int       ix;
  char     *n;
  module_t *node;
  module_t *sub;
  
  node = data_moduleval(ns -> root);
  for (ix = 0; ix < name_size(name); ix++) {
    n = name_get(name, ix);
    name_extend(current, n);
    sub = data_moduleval(mod_get_module(node, n));
    if (!sub) {
      sub = mod_add_module(node, name_copy(current));
    }
    node = sub;
  }
  name_free(current);
  return node;
}

/* ------------------------------------------------------------------------ */

namespace_t * ns_create(namespace_t *up) {
  namespace_t *ret;

  assert(up);
  if (ns_debug) {
    debug("  Creating subordinate namespace");
  }
  ret = _ns_create();
  ret -> import_ctx = NULL;
  ret -> up = up;
  return ret;
}

namespace_t * ns_create_root(void *importer, import_t import_fnc) {
  namespace_t *ret;

  assert(importer && import_fnc);
  if (ns_debug) {
    debug("  Creating root namespace");
  }
  ret = _ns_create();
  ret -> import_ctx = importer;
  ret -> import_fnc = import_fnc;
  return ret;
}

void ns_free(namespace_t *ns) {
  if (ns) {
    data_free(ns -> root);
    free(ns);
  }
}

data_t * ns_import(namespace_t *ns, name_t *name) {
  char     *n;
  data_t   *data;
  data_t   *mod;
  data_t   *script;
  module_t *module;

  if (ns_debug) {
    debug("  Importing module '%s'", name_tostring(name));
  }
  mod = ns_get(ns, name);
  if (mod) {
    if (ns_debug) {
      debug("  Module '%s' already imported", n);
    }
    data = mod;
  } else {
    if (!ns -> import_ctx) {
      if (ns_debug) {
        debug("  Module '%s' not found - delegating to higher level namespace", n);
      }
      data = ns_import(ns -> up, name);
    } else {
      if (ns_debug) {
        debug("  Module '%s' not found - delegating to loader", n);
      }
      script = ns -> import_fnc(ns -> import_ctx, name);
      if (data_is_script(script)) {
        module = _ns_add(ns, name);
        data = mod_set(module, data_scriptval(script));

        if (data_is_module(data)) {
          data = data_create(Module, module);
        }
      } else {
        data = script; /* !data_is_script(script) => Error */
      }
    }
  }
  return data;
}

data_t * ns_get(namespace_t *ns, name_t *name) {
  int       ix;
  char     *n;
  module_t *node;
  
  node = data_moduleval(ns -> root);
  for (ix = 0; ix < name_size(name); ix++) {
    n = name_get(name, ix);
    node = data_moduleval(mod_get_module(node, n));
    if (!node) {
      return data_error(ErrorName,
                        "Import '%s' not found in namespace", name);
    }
  }
  return data_create(Module, node);
}

data_t * ns_resolve(namespace_t *ns, char *name) {
  return mod_resolve(data_moduleval(ns -> root), name);
}
