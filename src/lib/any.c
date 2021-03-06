/*
 * /obelix/src/types/any.c - Copyright (c) 2015 Jan de Visser <jan@de-visser.net>
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

#include "libcore.h"
#include <data.h>
#include <exception.h>
#include <range.h>
#include <str.h>

/* ------------------------------------------------------------------------ */

extern int      data_debug;

static void_t _type_initializers[] = {
  str_init,
  int_init,
  float_init,
  datalist_init,
  exception_init,
  ptr_init,
  file_init,
  mutex_init,
  name_init,
  thread_init,
  nvp_init,
  NULL
};

static data_t *    _any_cmp(data_t *, char *, arguments_t *);
static data_t *    _any_not(data_t *, char *, arguments_t *);
static data_t *    _any_and(data_t *, char *, arguments_t *);
static data_t *    _any_or(data_t *, char *, arguments_t *);
static str_t *     _any_tostring(data_t *, char *, arguments_t *);
static data_t *    _any_hash(data_t *, char *, arguments_t *);
static data_t *    _any_len(data_t *, char *, arguments_t *);
static data_t *    _any_type(data_t *, char *, arguments_t *);
static data_t *    _any_hasattr(data_t *, char *, arguments_t *);
static data_t *    _any_getattr(data_t *, char *, arguments_t *);
static data_t *    _any_setattr(data_t *, char *, arguments_t *);
static data_t *    _any_callable(data_t *, char *, arguments_t *);
static data_t *    _any_iterable(data_t *, char *, arguments_t *);
static data_t *    _any_iterator(data_t *, char *, arguments_t *);
static data_t *    _any_iter(data_t *, char *, arguments_t *);
static data_t *    _any_next(data_t *, char *, arguments_t *);
static data_t *    _any_has_next(data_t *, char *, arguments_t *);
static data_t *    _any_reduce(data_t *, char *, arguments_t *);
static data_t *    _any_visit(data_t *, char *, arguments_t *);
static data_t *    _any_format(data_t *, char *, arguments_t *);
static data_t *    _any_query(data_t *, char *, arguments_t *);
static data_t *    _range_create(data_t *, char *, arguments_t *);
static data_t *    _set_loglevel(data_t *, char *, arguments_t *);
static data_t *    _set_logfile(data_t *, char *, arguments_t *);
#ifndef NDEBUG
static data_t *    _enable_debug(data_t *, char *, arguments_t *);
#endif /* !NDEBUG */

extern data_t *    _mutex_create(data_t *, char *, arguments_t *);


/* ------------------------------------------------------------------------ */

static methoddescr_t _methoddescr_interfaces[] = {
  { .type = Any,           .name = ">" ,           .method = (method_t) _any_cmp,           .argtypes = { Any, NoType, NoType },      .minargs = 1, .varargs = 0 },
  { .type = Any,           .name = "<" ,           .method = (method_t) _any_cmp,           .argtypes = { Any, NoType, NoType },      .minargs = 1, .varargs = 0 },
  { .type = Any,           .name = ">=" ,          .method = (method_t) _any_cmp,           .argtypes = { Any, NoType, NoType },      .minargs = 1, .varargs = 0 },
  { .type = Any,           .name = "<=" ,          .method = (method_t) _any_cmp,           .argtypes = { Any, NoType, NoType },      .minargs = 1, .varargs = 0 },
  { .type = Any,           .name = "==" ,          .method = (method_t) _any_cmp,           .argtypes = { Any, NoType, NoType },      .minargs = 1, .varargs = 0 },
  { .type = Any,           .name = "!=" ,          .method = (method_t) _any_cmp,           .argtypes = { Any, NoType, NoType },      .minargs = 1, .varargs = 0 },
  { .type = Any,           .name = "not",          .method = (method_t) _any_not,           .argtypes = { Any, NoType, NoType },      .minargs = 1, .varargs = 0 },
  { .type = Any,           .name = "and",          .method = (method_t) _any_and,           .argtypes = { Any, NoType, NoType },      .minargs = 1, .varargs = 1 },
  { .type = Any,           .name = "&&",           .method = (method_t) _any_and,           .argtypes = { Any, NoType, NoType },      .minargs = 1, .varargs = 1 },
  { .type = Any,           .name = "or",           .method = (method_t) _any_or,            .argtypes = { Any, NoType, NoType },      .minargs = 1, .varargs = 1 },
  { .type = Any,           .name = "||",           .method = (method_t) _any_or,            .argtypes = { Any, NoType, NoType },      .minargs = 1, .varargs = 1 },
  { .type = Any,           .name = "$",            .method = (method_t) _any_tostring,      .argtypes = { Any, NoType, NoType },      .minargs = 1, .varargs = 1 },
  { .type = Any,           .name = "hash",         .method = (method_t) _any_hash,          .argtypes = { NoType, NoType, NoType },   .minargs = 0, .varargs = 0 },
  { .type = Any,           .name = "len",          .method = (method_t) _any_len,           .argtypes = { NoType, NoType, NoType },   .minargs = 0, .varargs = 0 },
  { .type = Any,           .name = "size",         .method = (method_t) _any_len,           .argtypes = { NoType, NoType, NoType },   .minargs = 0, .varargs = 0 },
  { .type = Any,           .name = "type",         .method = (method_t) _any_type,          .argtypes = { Any, NoType, NoType },      .minargs = 0, .varargs = 0, .maxargs = 1 },
  { .type = Any,           .name = "hasattr",      .method = (method_t) _any_hasattr,       .argtypes = { String, NoType, NoType },   .minargs = 1, .varargs = 0 },
  { .type = Any,           .name = "getattr",      .method = (method_t) _any_getattr,       .argtypes = { String, NoType, NoType },   .minargs = 1, .varargs = 0 },
  { .type = Any,           .name = "setattr",      .method = (method_t) _any_setattr,       .argtypes = { String, Any, NoType },      .minargs = 2, .varargs = 0 },
  { .type = Any,           .name = "callable",     .method = (method_t) _any_callable,      .argtypes = { NoType, NoType, NoType },   .minargs = 0, .varargs = 1 },
  { .type = Any,           .name = "iterable",     .method = (method_t) _any_iterable,      .argtypes = { NoType, NoType, NoType },   .minargs = 0, .varargs = 1 },
  { .type = Any,           .name = "iterator",     .method = (method_t) _any_iterator,      .argtypes = { NoType, NoType, NoType },   .minargs = 0, .varargs = 1 },
  { .type = Iterable,      .name = "iter",         .method = (method_t) _any_iter,          .argtypes = { NoType, NoType, NoType },   .minargs = 0, .varargs = 1 },
  { .type = Iterator,      .name = "next",         .method = (method_t) _any_next,          .argtypes = { NoType, NoType, NoType },   .minargs = 0, .varargs = 1 },
  { .type = Iterator,      .name = "hasnext",      .method = (method_t) _any_has_next,      .argtypes = { NoType, NoType, NoType },   .minargs = 0, .varargs = 1 },
  { .type = Iterable,      .name = "reduce",       .method = (method_t) _any_reduce,        .argtypes = { Callable, Any, NoType },    .minargs = 1, .varargs = 1, .maxargs = 2 },
  { .type = Iterable,      .name = "visit",        .method = (method_t) _any_visit,         .argtypes = { Callable, NoType, NoType }, .minargs = 1, .varargs = 0 },
  { .type = Any,           .name = "format",       .method = (method_t) _any_format,        .argtypes = { Any, NoType, NoType },      .minargs = 0, .varargs = 1 },
  { .type = Connector,     .name = "query",        .method = (method_t) _any_query,         .argtypes = { String, Any, Any },         .minargs = 1, .varargs = 1 },
  { .type = Incrementable, .name = "~",            .method = (method_t) _range_create,      .argtypes = { Incrementable, Any, Any },  .minargs = 1, .varargs = 0 },
  { .type = Any,           .name = "range",        .method = (method_t) _range_create,      .argtypes = { Incrementable, Incrementable, Any },  .minargs = 1, .varargs = 0 },
  { .type = Any,           .name = "mutex",        .method = (method_t) _mutex_create,      .argtypes = { Any, Any, Any },            .minargs = 0, .varargs = 0 },
  { .type = Any,           .name = "set_loglevel", .method = (method_t) _set_loglevel,      .argtypes = { Any, Any, Any },            .minargs = 1, .varargs = 0 },
  { .type = Any,           .name = "set_logfile",  .method = (method_t) _set_logfile,       .argtypes = { Any, Any, Any },            .minargs = 1, .varargs = 0 },
#ifndef NDEBUG
  { .type = Any,           .name = "enable_debug", .method = (method_t) _enable_debug,      .argtypes = { String, Any, Any },         .minargs = 1, .varargs = 0 },
#endif /* !NDEBUG */
  { .type = NoType,        .name = NULL,           .method = NULL,                          .argtypes = { NoType, NoType, NoType },   .minargs = 0, .varargs = 0 }
};

/* ------------------------------------------------------------------------ */

void any_init(void) {
  int ix;

  for (ix = 0; _type_initializers[ix]; ix++) {
    _type_initializers[ix]();
  }
  typedescr_register_methods(Any, _methoddescr_interfaces);
}

/* ------------------------------------------------------------------------ */

data_t * _any_cmp(data_t *self, char *name, arguments_t *args) {
  data_t *other = arguments_get_arg(args, 0);
  int_t  *ret;
  int     cmp = data_cmp(self, other);

  if (!strcmp(name, "==")) {
    ret = bool_get(cmp == 0);
  } else if (!strcmp(name, "!=")) {
    ret = bool_get(cmp != 0);
  } else if (!strcmp(name, ">")) {
    ret = bool_get(cmp > 0);
  } else if (!strcmp(name, ">=")) {
    ret = bool_get(cmp >= 0);
  } else if (!strcmp(name, "<")) {
    ret = bool_get(cmp < 0);
  } else if (!strcmp(name, "<=")) {
    ret = bool_get(cmp <= 0);
  } else {
    assert(0);
    ret = (int_t *) data_false();
  }
  return (data_t *) ret;
}

data_t * _any_not(data_t *self, char *name, arguments_t *args) {
  data_t *asbool = data_cast(self, Bool);
  data_t *ret;

  (void) name;
  (void) args;

  if (!asbool) {
    ret = data_exception(ErrorSyntax,
                         "not(): Cannot convert value '%s' of type '%s' to boolean",
                         data_tostring(self),
                         data_typename(self));
  } else {
    ret = int_as_bool(data_intval(asbool) == 0);
    data_free(asbool);
  }
  return ret;
}

data_t * _any_and(data_t *self, char *name, arguments_t *args) {
  data_t *asbool;
  int     boolval;
  int     ix;

  (void) name;

  for (ix = -1; ix < arguments_args_size(args); ix++) {
    asbool = data_cast((ix < 0) ? self : arguments_get_arg(args, ix), Bool);
    if (!asbool) {
      return data_exception(ErrorSyntax,
                        "and(): Cannot convert value '%s' of type '%s' to boolean",
                        arguments_arg_tostring(args, ix),
                        data_typename(arguments_get_arg(args, ix)));
    }
    boolval = data_intval(asbool);
    data_free(asbool);
    if (!boolval) {
      return data_false();
    }
  }
  return data_true();
}

data_t * _any_or(data_t *self, char *name, arguments_t *args) {
  data_t *asbool;
  data_t *data;
  int     boolval;
  int     ix;

  (void) name;

  for (ix = -1; ix < arguments_args_size(args); ix++) {
    data = (ix < 0) ? self : arguments_get_arg(args, ix);
    asbool = data_cast(data, Bool);
    if (!asbool) {
      return data_exception(ErrorSyntax,
                        "or(): Cannot convert value '%s' of type '%s' to boolean",
                        arguments_arg_tostring(args, ix),
                        data_typename(arguments_get_arg(args, ix)));
    }
    boolval = data_intval(asbool);
    data_free(asbool);
    if (boolval) {
      return data_true();
    }
  }
  return data_false();
}

str_t * _any_tostring(data_t *self, char *name, arguments_t *args) {
  str_t  *ret;
  int     ix;

  (void) self;
  (void) name;

  ret = str_from_data(arguments_get_arg(args, 0));
  for (ix = 1; ix < arguments_args_size(args); ix++) {
    str_append_chars(ret, arguments_arg_tostring(args, ix));
  }
  return ret;
}

data_t * _any_hash(data_t *self, char *name, arguments_t *args) {
  data_t *obj;

  (void) name;

  obj = (args && arguments_args_size(args)) ? arguments_get_arg(args, 0) : self;
  return (data_t *) int_create(data_hash(obj));
}

data_t * _any_len(data_t *self, char *name, arguments_t *args) {
  data_t *obj;

  (void) name;

  obj = (args && arguments_args_size(args)) ? arguments_get_arg(args, 0) : self;
  return data_len(obj);
}

data_t * _any_type(data_t *self, char *func_name, arguments_t *args) {
  data_t      *d;
  typedescr_t *type;

  (void) func_name;

  d = (args  && arguments_args_size(args)) ? arguments_get_arg(args, 0) : self;
  type = data_typedescr(d);
  return (data_t *) type;
}

data_t * _any_hasattr(data_t *self, char *func_name, arguments_t *args) {
  data_t *attrname = arguments_get_arg(args, 0);
  name_t *name = name_create(1, data_tostring(attrname));
  data_t *r = data_resolve(self, name);
  data_t *ret = int_as_bool(r != NULL);

  (void) func_name;

  name_free(name);
  return ret;
}

data_t * _any_getattr(data_t *self, char _unused_ *func_name, arguments_t *args) {
  data_t  *attrname = arguments_get_arg(args, 0);
  name_t  *name = name_create(1, data_tostring(attrname));
  data_t  *ret;

  ret = data_get(self, name);
  name_free(name);
  return ret;
}

data_t * _any_setattr(data_t *self, char _unused_ *func_name, arguments_t *args) {
  data_t  *attrname = arguments_get_arg(args, 0);
  data_t  *value = arguments_get_arg(args, 1);
  name_t  *name = name_create(1, data_tostring(attrname));
  data_t  *ret;

  ret = data_set(self, name, value);
  name_free(name);
  return ret;
}

data_t * _any_callable(data_t *self, char _unused_ *name, arguments_t *args) {
  data_t *obj;

  obj = (arguments_args_size(args)) ? arguments_get_arg(args, 0) : self;
  return int_as_bool(data_is_callable(obj));
}

data_t * _any_iterable(data_t *self, char _unused_ *name, arguments_t *args) {
  data_t *obj;

  obj = (arguments_args_size(args)) ? arguments_get_arg(args, 0) : self;
  return int_as_bool(data_is_iterable(obj));
}

data_t * _any_iterator(data_t *self, char _unused_ *name, arguments_t *args) {
  data_t *obj;

  obj = (arguments_args_size(args)) ? arguments_get_arg(args, 0) : self;
  return int_as_bool(data_is_iterator(obj));
}

data_t * _any_iter(data_t *self, char _unused_ *name, arguments_t *args) {
  data_t *obj;

  obj = (arguments_args_size(args)) ? arguments_get_arg(args, 0) : self;
  return data_iter(obj);
}

data_t * _any_next(data_t *self, char _unused_ *name, arguments_t *args) {
  data_t *obj;

  obj = (arguments_args_size(args)) ? arguments_get_arg(args, 0) : self;
  return data_next(obj);
}

data_t * _any_has_next(data_t *self, char _unused_ *name, arguments_t *args) {
  data_t *obj;

  obj = (arguments_args_size(args)) ? arguments_get_arg(args, 0) : self;
  return data_has_next(obj);
}

data_t * _any_reduce(data_t *self, char _unused_ *name, arguments_t *args) {
  data_t *initial = (arguments_args_size(args) > 1) ? arguments_get_arg(args, 1) : data_null();

  return data_reduce(self, arguments_get_arg(args, 0), initial);
}

data_t * _any_visit(data_t *self, char _unused_ *name, arguments_t *args) {
  return data_visit(self, arguments_get_arg(args, 0));
}

data_t * _any_format(data_t *self, char _unused_ *name, arguments_t *args) {
  return data_interpolate(self, args);
}

data_t * _any_query(data_t *self, char _unused_ *name, arguments_t *args) {
  arguments_t *shifted;
  data_t      *querytext;
  data_t      *q;
  data_t      *ret = NULL;

  querytext = arguments_get_arg(args, 0);
  q = data_query(self, querytext);
  if (q) {
    shifted = arguments_shift(args, NULL);
    ret = data_interpolate(q, shifted);
    if (q != ret) {
      data_free(q);
    }
    arguments_free(shifted);
  }
  return ret;
}

data_t * _range_create(data_t *self, char *name, arguments_t *args) {
  data_t *from;
  data_t *to;
  int     infix = !strcmp(name, "~");

  if (data_debug) {
    _debug("_range_create");
    if (infix) {
      _debug("'%s' ~ '%s'", data_tostring(self), arguments_arg_tostring(args, 0));
    } else {
      _debug("range('%s', '%s')", arguments_arg_tostring(args, 0), arguments_arg_tostring(args, 1));
    }
  }
  from = (infix) ? self : arguments_get_arg(args, 0);
  to = arguments_get_arg(args, (infix) ? 0 : 1);
  return range_create(from, to);
}

data_t * _set_loglevel(data_t _unused_ *self, char _unused_ *name, arguments_t *args) {
  logging_set_level(arguments_arg_tostring(args, 0));
  return data_true();
}

data_t * _set_logfile(data_t _unused_ *self, char _unused_ *name, arguments_t *args) {
  logging_set_file(arguments_arg_tostring(args, 0));
  return data_true();
}

#ifndef NDEBUG

data_t * _enable_debug(data_t _unused_ *self, char _unused_ *name, arguments_t *args) {
  char *cat = arguments_arg_tostring(args, 0);

  logging_enable(cat);
  return data_copy(arguments_get_arg(args, 0));
}

#endif /* !NDEBUG */
