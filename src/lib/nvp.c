  /*
 * /obelix/src/types/nvp.c - Copyright (c) 2015 Jan de Visser <jan@finiandarcy.com>
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
#include <nvp.h>
#include <str.h>

/* ------------------------------------------------------------------------ */

static nvp_t *      _nvp_new(nvp_t *, va_list);
static void         _nvp_free(nvp_t *);
static char *       _nvp_allocstring(nvp_t *);
static data_t *     _nvp_resolve(nvp_t *, char *);

static data_t *     _nvp_create(char *, arguments_t *);

/* ------------------------------------------------------------------------ */

static vtable_t _vtable_NVP[] = {
  { .id = FunctionNew,         .fnc = (void_t) _nvp_new },
  { .id = FunctionFree,        .fnc = (void_t) _nvp_free },
  { .id = FunctionCmp,         .fnc = (void_t) nvp_cmp },
  { .id = FunctionAllocString, .fnc = (void_t) _nvp_allocstring },
  { .id = FunctionParse,       .fnc = (void_t) nvp_parse },
  { .id = FunctionHash,        .fnc = (void_t) nvp_hash },
  { .id = FunctionResolve,     .fnc = (void_t) _nvp_resolve },
  { .id = FunctionNone,        .fnc = NULL }
};

int NVP = -1;

/* ------------------------------------------------------------------------ */

void nvp_init(void) {
  if (NVP < 1) {
    typedescr_register(NVP, nvp_t);
  }
}

nvp_t * _nvp_new(nvp_t *nvp, va_list args) {
  data_t *name;
  data_t *value;

  name = va_arg(args, data_t *);
  value = va_arg(args, data_t *);
  nvp -> name = data_copy(name);
  nvp -> value = data_copy(value);
  return nvp;
}

void _nvp_free(nvp_t *nvp) {
  if (nvp) {
    data_free(nvp -> name);
    data_free(nvp -> value);
  }
}

char * _nvp_allocstring(nvp_t *nvp) {
  char *buf;

  asprintf(&buf, "%s=%s", 
      data_tostring(nvp -> name), 
      data_tostring(nvp -> value));
  return buf;
}

data_t * _nvp_resolve(nvp_t *nvp, char *name) {
  if (!strcmp(name, "name")) {
    return data_copy(nvp -> name);
  } else if (!strcmp(name, "value")) {
    return data_copy(nvp -> value);
  } else {
    return NULL;
  }
}
  
/* ------------------------------------------------------------------------ */

nvp_t * nvp_create(data_t *name, data_t *value) {
  nvp_init();
  return (nvp_t *) data_create(NVP, name, value);
}

nvp_t * nvp_parse(char *str) {
  char   *cpy;
  char   *ptr;
  char   *name;
  char   *val;
  data_t *n;
  data_t *v;
  nvp_t  *ret;

  // FIXME Woefully inadequate.
  cpy = strdup(str);
  ptr = strchr(cpy, '=');
  name = cpy;
  val = NULL;
  if (ptr) {
    *ptr = 0;
    val = ptr + 1;
    val = strtrim(val);
  }
  name = strtrim(name);
  n = str_to_data(name);
  v = (val) ? data_decode(val) : data_true();
  ret = nvp_create(n, v);
  data_free(n);
  data_free(v);
  free(cpy);
  return ret;
}

int nvp_cmp(nvp_t *nvp1, nvp_t *nvp2) {
  int ret = data_cmp(nvp1 -> name, nvp2 -> name);
  if (!ret) {
    ret = data_cmp(nvp1 -> value, nvp2 -> value);
  }
  return ret;
}

unsigned int nvp_hash(nvp_t *nvp) {
  return hashblend(data_hash(nvp -> name), data_hash(nvp -> value));
}

/* ------------------------------------------------------------------------ */

data_t * _nvp_create(char *name, arguments_t *args) {
  return (data_t *) nvp_create(
      data_uncopy(arguments_get_arg(args, 0)),
      data_uncopy(arguments_get_arg(args, 1)));
}
