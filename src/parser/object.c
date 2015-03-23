/*
 * /obelix/src/object.c - Copyright (c) 2014 Jan de Visser <jan@finiandarcy.com>
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
#include <string.h>

#include <exception.h>
#include <object.h>
#include <script.h>

int res_debug = 0;

static void          _data_init_object(void) __attribute__((constructor));
static data_t *      _data_new_object(data_t *, va_list);
static data_t *      _data_copy_object(data_t *, data_t *);
static int           _data_cmp_object(data_t *, data_t *);
static char *        _data_tostring_object(data_t *);
static unsigned int  _data_hash_object(data_t *);
static data_t *      _data_call_object(data_t *, array_t *, dict_t *);
static data_t *      _data_resolve_object(data_t *, char *);
static data_t *      _data_set_object(data_t *, char *, data_t *);

static data_t *      _object_create(data_t *, char *, array_t *, dict_t *);
static data_t *      _object_new(data_t *, char *, array_t *, dict_t *);

static data_t *      _object_get(object_t *, char *);
static data_t *      _object_call_attribute(object_t *, char *, array_t *, dict_t *);
static object_t *    _object_set_all_reducer(entry_t *, object_t *);

  /* ----------------------------------------------------------------------- */

static vtable_t _vtable_object[] = {
  { .id = FunctionNew,      .fnc = (void_t) _data_new_object },
  { .id = FunctionCopy,     .fnc = (void_t) _data_copy_object },
  { .id = FunctionCmp,      .fnc = (void_t) _data_cmp_object },
  { .id = FunctionFree,     .fnc = (void_t) object_free },
  { .id = FunctionToString, .fnc = (void_t) _data_tostring_object },
  { .id = FunctionHash,     .fnc = (void_t) _data_hash_object },
  { .id = FunctionResolve,  .fnc = (void_t) _data_resolve_object },
  { .id = FunctionCall,     .fnc = (void_t) _data_call_object },
  { .id = FunctionSet,      .fnc = (void_t) _data_set_object },
  { .id = FunctionNone,     .fnc = NULL }
};

static typedescr_t _typedescr_object = {
  .type =      Object,
  .type_name = "object",
  .vtable =    _vtable_object
};

/* FIXME Add append, delete, head, tail, etc... */
static methoddescr_t _methoddescr_object[] = {
  { .type = Any,    .name = "object", .method = _object_create, .argtypes = { Any, Any, Any },          .minargs = 0, .varargs = 1 },
  { .type = Any,    .name = "new",    .method = _object_new,    .argtypes = { Any, Any, Any },          .minargs = 1, .varargs = 1 },
  { .type = NoType, .name = NULL,     .method = NULL,            .argtypes = { NoType, NoType, NoType }, .minargs = 0, .varargs = 0 },
};

/* ----------------------------------------------------------------------- */

void _data_init_object(void) {
  typedescr_register(&_typedescr_object);  
  typedescr_register_methods(_methoddescr_object);
}

data_t * _data_new_object(data_t *ret, va_list arg) {
  object_t *object;

  object = va_arg(arg, object_t *);
  ret -> ptrval = object_copy(object);
  return ret;
}

data_t * _data_copy_object(data_t *target, data_t *src) {
  target -> ptrval = object_copy(data_objectval(src));
  return target;
}

int _data_cmp_object(data_t *d1, data_t *d2) {
  return object_cmp((object_t *) d1 -> ptrval, (object_t *) d2 -> ptrval);
}

char * _data_tostring_object(data_t *d) {
  return object_tostring((object_t *) d -> ptrval);
}

unsigned int _data_hash_object(data_t *d) {
  return object_hash((object_t *) d -> ptrval);
}

data_t * _data_call_object(data_t *self, array_t *args, dict_t *kwargs) {
  return object_call(data_objectval(self), args, kwargs);
}

data_t * _data_resolve_object(data_t *data, char *name) {
  return object_resolve(data_objectval(data), name);
}

data_t * _data_set_object(data_t *data, char *name, data_t *value) {
  return object_set(data_objectval(data), name, value);
}

data_t * data_create_object(object_t *object) {
  return data_create(Object, object);
}

/* ----------------------------------------------------------------------- */

data_t * _object_create(data_t *self, char *name, array_t *args, dict_t *kwargs) {
  data_t   *ret;
  object_t *obj;

  obj = object_create(NULL);
  dict_reduce(kwargs, (reduce_t) _object_set_all_reducer, obj);
  ret = data_create(Object, obj);
  return ret;
}

data_t * _object_new(data_t *self, char *fncname, array_t *args, dict_t *kwargs) {
  data_t   *ret;
  object_t *obj;
  name_t   *name = NULL;
  data_t   *n;
  script_t *script = NULL;
  array_t  *shifted;
  
  n = data_copy(data_array_get(args, 0));
  if (data_is_name(n)) {
    name = name_copy(data_nameval(n));
  } else if (data_type(n) == String) {
    name = name_parse(data_tostring(n));
  }
  if (name) {
    data_free(n);
    n = data_resolve(self, name);
  }
  if (!data_is_error(n)) {
    if (data_is_object(n)) {
      script = data_objectval(n) -> script;
    } else if (data_is_closure(n)) {
      script = data_closureval(n) -> script;
    } else if (data_is_script(n)) {
      script = data_scriptval(n);
    }
    if (script) {
      shifted = array_slice(args, 1, 0);
      ret = script_execute(script, shifted, kwargs);
      array_free(shifted);
    }
  } else {
    ret = data_copy(n);
  }
  name_free(name);
  data_free(n);
  return ret;
}

/* ----------------------------------------------------------------------- */

data_t * _object_get(object_t *object, char *name) {
  data_t *ret;

  ret = (data_t *) dict_get(object -> variables, name);
  if (ret) {
    ret = data_copy(ret);
  }
  return ret;  
}

data_t * _object_call_attribute(object_t *object, char *name, array_t *args, dict_t *kwargs) {
  data_t *func;
  data_t *ret = NULL;
  
  func = _object_get(object, name);
  if (data_is_callable(func)) {
    ret = data_call(func, args, kwargs);
  }
  data_free(func);
  return ret;
}

object_t * _object_set_all_reducer(entry_t *e, object_t *object) {
  object_set(object, (char *) e -> key, (data_t *) e -> value);
  return object;
}

/* ----------------------------------------------------------------------- */

object_t * object_create(script_t *script) {
  object_t   *ret;
  str_t      *s;

  ret = NEW(object_t);
  ret -> refs = 1;
  ret -> script = script;
  ret -> constructing = FALSE;
  ret -> ptr = NULL;
  ret -> str = NULL;
  ret -> debugstr = NULL;
  ret -> variables = strdata_dict_create();
  return ret;
}

object_t* object_copy(object_t* object) {
  object -> refs++;
  return object;
}

void object_free(object_t *object) {
  if (object) {
    object -> refs--;
    if (object -> refs <= 0) {
      data_free(_object_call_attribute(object, "__finalize__", NULL, NULL));
      dict_free(object -> variables);
      free(object -> str);
      free(object -> debugstr);
    }
  }
}

data_t * object_get(object_t *object, char *name) {
  data_t *ret;

  ret = _object_get(object, name);
  if (!ret) {
    ret = data_error(ErrorName,
                     "Object '%s' has no attribute '%s'",
                     object_debugstr(object),
                     name);
  }
  return ret;
}

data_t * object_set(object_t *object, char *name, data_t *value) {
  str_t     *s;
  closure_t *closure;
  
  if (data_is_script(value)) {
    closure = script_create_closure(data_scriptval(value), data_create(Object, object), NULL);
    value = data_create(Closure, closure);
  }  
  dict_put(object -> variables, strdup(name), data_copy(value));
  if (res_debug) {
    s = dict_tostr(object -> variables);
    debug("   object_set('%s') -> variables = %s", 
          object -> script ? script_tostring(object -> script) : "anon", 
          str_chars(s));
    str_free(s);
  }
  return value;
}

int object_has(object_t *object, char *name) {
  int ret;
  ret = dict_has_key(object -> variables, name);
  if (res_debug) {
    debug("   object_has('%s', '%s'): %d", object_debugstr(object), name, ret);
  }
  return ret;
}

data_t * object_call(object_t *object, array_t *args, dict_t *kwargs) {
  data_t *ret;

  ret = _object_call_attribute(object, "__call__", args, kwargs);
  if (!ret || data_is_error(ret)) {
    data_free(ret);
    ret = data_error(ErrorNotCallable, "Object '%s' is not callable", 
                     object_tostring(object));
  }
  return ret;
}

char * object_tostring(object_t *object) {
  data_t  *data;

  data = _object_call_attribute(object, "__str__", NULL, NULL);
  if (!data) {
    object -> str = strdup(object_debugstr(object));
  } else {
    object -> str = strdup(data_charval(data));
  }
  data_free(data);
  return object -> str;
}

char * object_debugstr(object_t *object) {
  if (!object -> debugstr) {
    if (object -> script) {
      asprintf(&(object -> debugstr), "<%s object at %p>",
               script_tostring(object -> script), object);
    } else {
      asprintf(&(object -> debugstr), "<anon object at %p>", object);
    }
  }
  return object -> debugstr;
}

unsigned int object_hash(object_t *object) {
  data_t       *data;
  unsigned int  ret;

  data = _object_call_attribute(object, "__hash__", NULL, NULL);
  ret = (data && (data_type(data) == Int)) ? data_intval(data) : hashptr(object);
  data_free(data);
  return ret;
}

int object_cmp(object_t *o1, object_t *o2) {
  data_t  *data;
  array_t *args;
  int      ret;

  args = data_array_create(1);
  array_set(args, 0, data_create(Object, o2));
  data = _object_call_attribute(o1, "__cmp__", args, NULL);
  ret = (data && !data_is_error(data))
      ? data_intval(data)
      : (long) o1 - (long) o2;
  data_free(data);
  array_free(args);
  return ret;
}

data_t * object_resolve(object_t *object, char *name) {
  return (data_t *) dict_get(object -> variables, name);
}