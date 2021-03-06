/*
 * /obelix/src/lib/name.c - Copyright (c) 2015 Jan de Visser <jan@finiandarcy.com>
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

#include "libcore.h"
#include <data.h>
#include <str.h>

static void          _name_debug(name_t *, char *);

static void          _name_free(name_t *);
static char *        _name_tostring(name_t *);
static data_t *      _name_append(data_t *, char *, arguments_t *);
static data_t *      _name_resolve(name_t *, char *);

static name_t *      _name_create(int);

static vtable_t _vtable_Name[] = {
  { .id = FunctionParse,    .fnc = (void_t) name_parse },
  { .id = FunctionCmp,      .fnc = (void_t) name_cmp },
  { .id = FunctionFree,     .fnc = (void_t) _name_free },
  { .id = FunctionToString, .fnc = (void_t) _name_tostring },
  { .id = FunctionHash,     .fnc = (void_t) name_hash },
  { .id = FunctionResolve,  .fnc = (void_t) _name_resolve },
  { .id = FunctionLen,      .fnc = (void_t) name_size },
  { .id = FunctionNone,     .fnc = NULL }
};

static methoddescr_t _methods_Name[] = {
  { .type = -1,     .name = "append", .method = _name_append, .argtypes = { Any, Any, Any },          .minargs = 1, .varargs = 1 },
  { .type = NoType, .name = NULL,     .method = NULL,         .argtypes = { NoType, NoType, NoType }, .minargs = 0, .varargs = 0 },
};

int name_debug;
int Name = -1;

/* ----------------------------------------------------------------------- */

void name_init(void) {
  if (Name < 1) {
    logging_register_category("name", &name_debug);
    typedescr_register_with_methods(Name, name_t);
    hierarchy_init();
  }
}

/* ----------------------------------------------------------------------- */

data_t * _name_append(data_t *self, char _unused_ *fnc_name, arguments_t *args) {
  name_t *name = data_as_name(self);
  int     ix;

  for (ix = 0; ix < datalist_size(args -> args); ix++) {
    name_extend(name, arguments_arg_tostring(args, ix));
  }
  return self;
}

/* ----------------------------------------------------------------------- */

void _name_debug(name_t *name, char *msg) {
  debug(name, "%s: %p = %s (%d)", msg, name, name_tostring(name), name -> _d.refs);
}

name_t * _name_create(int count) {
  name_t *ret;

  name_init();
  ret = data_new(Name, name_t);
  ret -> name = str_array_create(count);
  ret -> sep = NULL;
  _name_debug(ret, "_name_create");
  return ret;
}

void _name_free(name_t *name) {
  if (name) {
    array_free(name -> name);
    free(name -> sep);
  }
}

char * _name_tostring(name_t *name) {
  name_tostring_sep(name, ".");
  return NULL;
}

data_t * _name_resolve(name_t *n, char *name) {
  long    ix;

  if (!strtoint(name, &ix)) {
    return ((ix >= 0) && (ix < name_size(n)))
      ? str_to_data(name_get(n, (int) ix))
      : NULL;
  } else {
    return NULL;
  }
}

/* ----------------------------------------------------------------------- */

name_t * name_create(int count, ...) {
  name_t  *ret;
  va_list  components;

  va_start(components, count);
  ret = name_vcreate(count, components);
  va_end(components);
  _name_debug(ret, "name_create");
  return ret;
}

name_t * name_vcreate(int count, va_list components) {
  name_t *ret;
  int     ix;

  ret = _name_create(count);
  for (ix = 0; ix < count; ix++) {
    name_extend(ret, va_arg(components, char *));
  }
  _name_debug(ret, "name_vcreate");
  return ret;
}

name_t * name_deepcopy(name_t *src) {
  name_t *ret;

  if (src && name_size(src)) {
    ret = _name_create(name_size(src));
    name_append(ret, src);
  } else {
    ret = _name_create(0);
  }
  return ret;
}

name_t * name_split(char *name, char *sep) {
  name_t  *ret;
  array_t *array;

  if (name && *name) {
    array = array_split(name, sep);
    ret = _name_create(array_size(array));
    name_append_array(ret, array);
    array_free(array);
    ret -> _d.str = strdup(name);
    ret -> sep = strdup(sep);
  } else {
    ret = name_create(0);
  }
  return ret;
}

name_t * name_parse(char *name) {
  return name_split(name, ".");
}

name_t * name_extend(name_t *name, char *n) {
  array_push(name -> name, strdup(n));
  free(name -> _d.str);
  name -> _d.str = NULL;
  _name_debug(name, "name_extend");
  return name;
}

name_t * name_append(name_t *name, name_t *additions) {
  return name_append_array(name, additions -> name);
}

name_t * name_extend_data(name_t *name, data_t *data) {
  char *str;

  if (data_hastype(data, Int)) {
    char buf[2];
    buf[0] = (char) data_intval(data);
    buf[1] = 0;
    str = buf;
  } else {
    str = data_tostring(data);
  }
  return name_extend(name, str);
}

name_t * name_append_array(name_t *name, array_t *additions) {
  int ix;

  for (ix = 0; ix < array_size(additions); ix++) {
    array_push(name -> name, strdup(str_array_get(additions, ix)));
  }
  free(name -> _d.str);
  name -> _d.str = NULL;
  _name_debug(name, "name_append_array");
  return name;
}

name_t * name_append_data_array(name_t *name, array_t *additions) {
  int ix;

  for (ix = 0; ix < array_size(additions); ix++) {
    name_extend_data(name, data_array_get(additions, ix));
  }
  return name;
}

int name_size(name_t *name) {
  return array_size(name -> name);
}

char * name_first(name_t *name) {
  return (name && (name_size(name) > 0)) ? str_array_get(name -> name, 0) : NULL;
}

char * name_last(name_t *name) {
  return (name && (name_size(name) > 0)) ? str_array_get(name -> name, -1) : NULL;
}

char * name_get(name_t *name, int ix) {
  return str_array_get(name -> name, ix);
}

array_t * name_as_array(name_t *name) {
  return array_copy(name -> name);
}

name_t * name_tail(name_t *name) {
  name_t  *ret = data_new(Name, name_t);
  array_t *tail = array_slice(name -> name, 1, -1);

  ret -> name = str_array_create(name_size(name) - 1);
  name_append_array(ret, tail);
  array_free(tail);
  return ret;
}

name_t * name_head(name_t *name) {
  name_t  *ret = data_new(Name, name_t);
  array_t *head = array_slice(name -> name, 0, -2);

  ret -> name = str_array_create(name_size(name) - 1);
  name_append_array(ret, head);
  array_free(head);
  return ret;
}

char * name_tostring_sep(name_t *name, char *sep) {
  str_t *s = NULL;

  if (!name) {
    return "name:NULL";
  }
  if (name -> sep && strcmp(name -> sep, sep)) {
    if (name -> _d.free_str == Normal) {
      free(name -> _d.str);
    }
    free(name -> sep);
    name -> sep = NULL;
    name -> _d.str = NULL;
    name -> _d.free_str = Normal;
  }
  if (!name -> sep) {
    name -> sep = strdup(sep);
  }
  if (!name -> _d.str) {
    if (name_size(name)) {
      s = array_join(name -> name, name -> sep);
      name -> _d.str = str_reassign(s);
      name -> _d.free_str = Normal;
    } else {
      name -> _d.str = "";
      name -> _d.free_str = DontFreeData;
    }
  }
  return name -> _d.str;
}

int name_cmp(name_t *n1, name_t *n2) {
  int ix;
  int cmp;

  if (name_size(n1) != name_size(n2)) {
    return name_size(n1) - name_size(n2);
  }
  for (ix = 0; ix < name_size(n1); ix++) {
    cmp = strcmp(name_get(n1, ix), name_get(n2, ix));
    if (cmp) {
      return cmp;
    }
  }
  return 0;
}

int name_startswith(name_t *name, name_t *start) {
  int ix;
  int cmp;

  if (name_size(name) < name_size(start)) {
    return FALSE;
  } else {
    for (ix = 0; ix < name_size(start); ix++) {
      cmp = strcmp(name_get(name, ix), name_get(start, ix));
      if (cmp) {
        return FALSE;
      }
    }
    return TRUE;
  }
}

unsigned int name_hash(name_t *name) {
  unsigned int ret;
  int          ix;

  ret = hashptr(name);
  _name_debug(name, "name_hash");
  debug(name, "name_hash. name = %p size = %d", name, name_size(name));
  for (ix = 0; ix < name_size(name); ix++) {
    debug(name, "ix = %d name_get = %s", ix, name_get(name, ix));
    hashblend(ret, strhash(name_get(name, ix)));
  }
  return ret;
}
