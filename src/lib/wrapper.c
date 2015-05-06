/*
 * /obelix/src/types/wrapper.c - Copyright (c) 2015 Jan de Visser <jan@de-visser.net>
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

#include <exception.h>
#include <logging.h>
#include <wrapper.h>

static void         _wrapper_init(void) __attribute__((constructor));
static int          _wrapper_is(typedescr_t *, int);
static data_t *     _wrapper_new(data_t *, va_list);
static void         _wrapper_free(data_t *);
static data_t *     _wrapper_copy(data_t *, data_t *);
static data_t *     _wrapper_parse(typedescr_t *, char *);
static int          _wrapper_cmp(data_t *, data_t *);
static unsigned int _wrapper_hash(data_t *);
static char *       _wrapper_tostring(data_t *);
static data_t *     _wrapper_call(data_t *, array_t *, dict_t *);
static data_t *     _wrapper_resolve(data_t *, char *);
static data_t *     _wrapper_set(data_t *, char *, data_t *);
static int          _wrapper_len(data_t *);
static data_t *     _wrapper_iter(data_t *);
static data_t *     _wrapper_next(data_t *);
static data_t *     _wrapper_has_next(data_t *);

static vtable_t _vtable_wrapper[] = {
  { .id = FunctionIs,       .fnc = (void_t) _wrapper_is },
  { .id = FunctionNew,      .fnc = (void_t) _wrapper_new },
  { .id = FunctionCopy,     .fnc = (void_t) _wrapper_copy },
  { .id = FunctionParse,    .fnc = (void_t) _wrapper_parse },
  { .id = FunctionCmp,      .fnc = (void_t) _wrapper_cmp },
  { .id = FunctionHash,     .fnc = (void_t) _wrapper_hash },
  { .id = FunctionFreeData, .fnc = (void_t) _wrapper_free },
  { .id = FunctionToString, .fnc = (void_t) _wrapper_tostring },
  { .id = FunctionResolve,  .fnc = (void_t) _wrapper_resolve },
  { .id = FunctionCall,     .fnc = (void_t) _wrapper_call },
  { .id = FunctionSet,      .fnc = (void_t) _wrapper_set },
  { .id = FunctionLen,      .fnc = (void_t) _wrapper_len },
  { .id = FunctionIter,     .fnc = (void_t) _wrapper_iter },
  { .id = FunctionNext,     .fnc = (void_t) _wrapper_next },
  { .id = FunctionHasNext,  .fnc = (void_t) _wrapper_has_next },
  { .id = FunctionNone,     .fnc = NULL }
};

int wrapper_debug = 0;
int Wrapper = -1;

static typedescr_t typedescr_wrapper = {
  .type       = -1,
  .type_name  = "wrapper",
  .vtable     = _vtable_wrapper
};

#define wrapper_function(type, fnc_id) (((type) -> ptr) \
  ? vtable_get((vtable_t *) (type) -> ptr, (fnc_id)) \
  : NULL)
  
/* -- W R A P P E R  S T A T I C  F U N C T I O N S ----------------------- */

void _wrapper_init(void) {
  logging_register_category("wrapper", &wrapper_debug);
  Wrapper = typedescr_register(&typedescr_wrapper);
}

int _wrapper_is(typedescr_t *descr, int type) {
  return (type > Interface) 
    ? vtable_implements((vtable_t *) type -> ptr, type) 
    : FALSE;
}

data_t * _wrapper_new(data_t *ret, va_list arg) {
  typedescr_t *type = data_typedescr(ret);
  void_t       fnc;
  void        *src;
  
  if (wrapper_debug) {
    debug("wrapper_new(%s)", type -> type_name);
  }
  fnc = wrapper_function(type, FunctionFactory);
  if (fnc) {
    if (wrapper_debug) {
      debug("wrapper(%s) - FunctionFactory", type -> type_name);
    }
    ret -> ptrval = ((vcreate_t) fnc)(arg);
  } else {
    src = va_arg(arg, void *);
    fnc = wrapper_function(type, FunctionCopy);
    if (fnc) {
      if (wrapper_debug) {
        debug("wrapper(%s) - FunctionCopy", type -> type_name);
      }
      ret -> ptrval = ((voidptrvoidptr_t) fnc)(src);
    } else {
      info("wrapper(%s) - Direct pointer assignment", type -> type_name);
      ret -> ptrval = src;
    }
  }
  return ret;
}

void _wrapper_free(data_t *data) {
  typedescr_t *type = data_typedescr(data);
  void_t       fnc;
  
  if (wrapper_debug) {
    debug("wrapper_free(%s)", type -> type_name);
  }
  fnc = wrapper_function(type, FunctionFree);
  if (fnc) {
    if (wrapper_debug) {
      debug("wrapper(%s) - FunctionFree", type -> type_name);
    }
    ((free_t) fnc)(data -> ptrval);
  } else {
    warning("No free method defined for wrapper type '%s'", type -> type_name);
  }
}

data_t * _wrapper_copy(data_t *target, data_t *src) {
  typedescr_t *type = data_typedescr(src);
  void_t       fnc;
  
  if (wrapper_debug) {
    debug("wrapper_copy(%s)", type -> type_name);
  }
  fnc = wrapper_function(type, FunctionCopy);
  if (fnc) {
    if (wrapper_debug) {
      debug("wrapper(%s) - FunctionCopy", type -> type_name);
    }
    target -> ptrval = ((voidptrvoidptr_t) fnc)(src -> ptrval);
  } else {
    target -> ptrval = src -> ptrval;
  }
  return target;
}

int _wrapper_cmp(data_t *d1, data_t *d2) {
  typedescr_t *type = data_typedescr(d1);
  void_t       fnc;
  
  if (wrapper_debug) {
    debug("wrapper_cmp(%s)", type -> type_name);
  }
  fnc = wrapper_function(type, FunctionCmp);
  if (fnc) {
    return ((cmp_t) fnc)(d1 -> ptrval, d2 -> ptrval);
  } else {
    return (int) ((unsigned long) d1 -> ptrval) - ((unsigned long) d2 -> ptrval);
  }
}

unsigned int _wrapper_hash(data_t *data) {
  typedescr_t *type = data_typedescr(data);
  void_t       fnc;
  
  if (wrapper_debug) {
    debug("wrapper_hash(%s)", type -> type_name);
  }
  fnc = wrapper_function(type, FunctionHash);
  if (fnc) {
    return ((hash_t) fnc)(data -> ptrval);
  } else {
    return hashptr(data -> ptrval);
  }
}

char * _wrapper_tostring(data_t *data) {
  typedescr_t *type = data_typedescr(data);
  void_t       fnc = wrapper_function(type, FunctionToString);
  
  if (wrapper_debug) {
    debug("wrapper_tostring(%s)", type -> type_name);
  }
  if (fnc) {
    return ((tostring_t) fnc)(data -> ptrval);
  } else {
    return (char *) data -> ptrval;
  }
}

data_t * _wrapper_parse(typedescr_t *type, char *str) {
  void_t  fnc = wrapper_function(type, FunctionParse);
  data_t *ret;
  
  if (wrapper_debug) {
    debug("wrapper_parse(%s)", type -> type_name);
  }
  if (fnc) {
    ret = data_create_noinit(type -> type);
    ret -> ptrval = ((parse_t) fnc)(str);
    return ret;
  } else {
    return data_exception(ErrorInternalError,
                          "No 'parse' method defined for wrapper type '%s'", 
                          type -> type_name);
  }
}

data_t * _wrapper_call(data_t *self, array_t *params, dict_t *kwargs) {
  typedescr_t *type = data_typedescr(self);
  void_t       fnc;
  
  if (wrapper_debug) {
    debug("wrapper_call(%s)", type -> type_name);
  }
  fnc = wrapper_function(type, FunctionCall);
  if (!fnc) {
    return data_exception(ErrorInternalError,
                      "No 'call' method defined for wrapper type '%s'", 
                      type -> type_name);
  } else {
    return ((call_t) fnc)(self -> ptrval, params, kwargs);
  }
}

data_t * _wrapper_resolve(data_t *data, char *name) {
  typedescr_t *type = data_typedescr(data);
  void_t       fnc;
  data_t      *ret;
  
  fnc = wrapper_function(type, FunctionResolve);
  if (!fnc) {
    return data_exception(ErrorInternalError,
                      "No 'resolve' method defined for wrapper type '%s'", 
                      type -> type_name);
  } else {
    ret = ((resolve_name_t) fnc)(data -> ptrval, name);
    if (wrapper_debug) {
      debug("wrapper_resolve(%s, %s) = %s", 
            type -> type_name, name, data_tostring(ret));
    }
    return ret;
  }
}

data_t * _wrapper_set(data_t *data, char *name, data_t *value) {
  typedescr_t *type = data_typedescr(data);
  void_t       fnc;
  
  if (wrapper_debug) {
    debug("wrapper_set(%s)", type -> type_name);
  }
  fnc = wrapper_function(type, FunctionSet);
  if (!fnc) {
    return data_exception(ErrorInternalError,
                      "No 'set' method defined for wrapper type '%s'", 
                      type -> type_name);
  } else {
    return ((setvalue_t) fnc)(data -> ptrval, name, value);
  }
}

int _wrapper_len(data_t *data) {
  typedescr_t *type = data_typedescr(data);
  int        (*fnc)(void *);
  
  if (wrapper_debug) {
    debug("_wrapper_len(%s)", type -> type_name);
  }
  fnc = (int (*)(void *)) wrapper_function(type, FunctionHash);
  if (fnc) {
    return fnc(data -> ptrval);
  } else {
    return -1;
  }
}

data_t * _wrapper_iter(data_t *data) {
  typedescr_t *type = data_typedescr(data);
  data_t     (*fnc)(void *);
  
  if (wrapper_debug) {
    debug("_wrapper_iter(%s)", type -> type_name);
  }
  fnc = (data_t * (*)(void *)) wrapper_function(type, FunctionIter);
  if (fnc) {
    return fnc(data -> ptrval);
  } else {
    return data_exception(ErrorInternalError,
                          "No 'iter' method defined for wrapper type '%s'", 
                          type -> type_name);
  }
}

data_t * _wrapper_next(data_t *data) {
  typedescr_t *type = data_typedescr(data);
  data_t     (*fnc)(void *);
  
  if (wrapper_debug) {
    debug("_wrapper_next(%s)", type -> type_name);
  }
  fnc = (data_t * (*)(void *)) wrapper_function(type, FunctionNext);
  if (fnc) {
    return fnc(data -> ptrval);
  } else {
    return data_exception(ErrorInternalError,
                          "No 'next' method defined for wrapper type '%s'", 
                          type -> type_name);
  }
}

data_t * _wrapper_has_next(data_t *data) {
  typedescr_t  *type = data_typedescr(data);
  int         (*fnc)(void *);
  
  if (wrapper_debug) {
    debug("_wrapper_has_next(%s)", type -> type_name);
  }
  fnc = (int (*)(void *)) wrapper_function(type, FunctionHasNext);
  if (fnc) {
    return data_create(Bool, fnc(data -> ptrval));
  } else {
    return data_exception(ErrorInternalError,
                          "No 'has_next' method defined for wrapper type '%s'", 
                          type -> type_name);
  }
}

/* -- W R A P P E R  P U B L I C  F U N C T I O N S ----------------------- */

int wrapper_register(int type, char *name, vtable_t *vtable) {
  typedescr_t  descr;

  descr.type = type;
  descr.type_name = name;
  descr.ptr = vtable_build(vtable);
  descr.vtable = _vtable_wrapper;
  descr.inherits_size = 0;
  descr.inherits = NULL;
  descr.promote_to = 0;
  descr.methods = NULL;
  descr.hash = 0;
  descr.str = NULL;
  descr.count = 0;
  return typedescr_register(&descr);
}

int wrapper_register_with_overrides(int type, char *name, vtable_t *vtable, vtable_t *overrides) {
  int          ret = wrapper_register(type, name, vtable);
  typedescr_t *td = typedescr_get(ret);
  int          ix;
  
  if (overrides) {
    for (ix = 0; overrides[ix].id != FunctionNone; ix++) {
      td -> vtable[overrides[ix].id].fnc = overrides[ix].fnc;
    }
  }
  return ret;
}



