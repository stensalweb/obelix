/*
 * /obelix/src/types/pointer.c - Copyright (c) 2014 Jan de Visser <jan@finiandarcy.com>
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
#include <stdlib.h>
#include <string.h>

#include <core.h>
#include <data.h>
#include <resolve.h>

static void          _ptr_init(void) __attribute__((constructor(101)));
static pointer_t *   _ptr_new(int, va_list);
static int           _ptr_cmp(pointer_t *, pointer_t *);
static data_t *      _ptr_cast(pointer_t *, int);
static unsigned int  _ptr_hash(pointer_t *);
static char *        _ptr_tostring(pointer_t *);
static pointer_t *   _ptr_parse(char *);

static data_t *      _ptr_copy(data_t *, char *, array_t *, dict_t *);
static data_t *      _ptr_fill(data_t *, char *, array_t *, dict_t *);

static vtable_t _vtable_ptr[] = {
  { .id = FunctionFactory,  .fnc = (void_t) _ptr_new },
  { .id = FunctionCmp,      .fnc = (void_t) _ptr_cmp },
  { .id = FunctionToString, .fnc = (void_t) _ptr_tostring },
  { .id = FunctionCast,     .fnc = (void_t) _ptr_cast },
  { .id = FunctionHash,     .fnc = (void_t) _ptr_hash },
  { .id = FunctionParse,    .fnc = (void_t) _ptr_parse },
  { .id = FunctionNone,     .fnc = NULL }
};

static typedescr_t _typedescr_ptr = {
  .type =      Pointer,
  .type_name = "ptr",
  .vtable =    _vtable_ptr
};

static methoddescr_t _methoddescr_ptr[] = {
  { .type = Pointer, .name = "copy",  .method = _ptr_copy, .argtypes = { Pointer, NoType, NoType }, .minargs = 0, .varargs = 1  },
  { .type = Pointer, .name = "fill",  .method = _ptr_fill, .argtypes = { Pointer, NoType, NoType }, .minargs = 1, .varargs = 1  },
  { .type = NoType,  .name = NULL,    .method = NULL,      .argtypes = { NoType, NoType, NoType },  .minargs = 0, .varargs = 0  },
};


/*
 * --------------------------------------------------------------------------
 * Pointer datatype functions
 * --------------------------------------------------------------------------
 */

static pointer_t * _null;

void _ptr_init(void) {
  typedescr_register(&_typedescr_ptr);
  typedescr_register_methods(_methoddescr_ptr);
  _null = (pointer_t *) data_create(Pointer, 0, NULL);
  _null -> _d.free_me = Constant;
}

pointer_t * _ptr_new(int type, va_list arg) {
  pointer_t *ret = data_new(Pointer, pointer_t);

  ret -> size = va_arg(arg, int);
  ret -> ptr = va_arg(arg, void *);
  return ret;
}

data_t * _ptr_cast(pointer_t *src, int totype) {
  data_t *ret = NULL;

  if (totype == Bool) {
    ret = data_create(Bool, src -> ptr != NULL);
  } else if (totype == Int) {
    ret = data_create(Int, (long) src -> ptr);
  }
  return ret;
}

int _ptr_cmp(pointer_t *p1, pointer_t *p2) {
  if (p1 -> ptr == p2 -> ptr) {
    return 0;
  } else if (p1 -> size != p2 -> size) {
    return p1 -> size - p2 -> size;
  } else {
    return memcmp(p1 -> ptr, p2 -> ptr, p1 -> size);
  }
}

char * _ptr_tostring(pointer_t *p) {
  if (!p -> _d.str) {
    if (p == _null) {
      p -> _d.str = strdup("Null");
    } else {
      asprintf(&p -> _d.str, "%p", p -> ptr);
    }
  }
  return NULL;
}

pointer_t * _ptr_parse(char *str) {
  long l;

  if (!strcmp(str, "null")) {
    return (pointer_t *) data_null();
  } else {
    if (strtoint(str, &l)) {
      return (pointer_t *) data_create(Pointer, 0, (void *) l);
    } else {
      return (pointer_t *) data_null();
    }
  }
}

unsigned int _ptr_hash(pointer_t *data) {
  return hash(data -> ptr, data -> size);
}

data_t * data_null(void) {
  return (data_t *) _null;
}

/* ----------------------------------------------------------------------- */

data_t * _ptr_copy(data_t *self, char *name, array_t *args, dict_t *kwargs) {
  void      *newbuf;
  pointer_t *p = data_as_pointer(self);

  newbuf = (void *) new(p -> size);
  memcpy(newbuf, p -> ptr, p -> size);
  return data_create(Pointer, p -> size, newbuf);
}

data_t * _ptr_fill(data_t *self, char *name, array_t *args, dict_t *kwargs) {
  pointer_t *p = data_as_pointer(self);
  data_t    *fillchar = data_array_get(args, 0);

  memset(p -> ptr, data_intval(fillchar), p -> size);
  return data_copy(self);
}

