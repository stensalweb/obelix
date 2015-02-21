/*
 * /obelix/src/resolve.c - Copyright (c) 2014 Jan de Visser <jan@finiandarcy.com>
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

#include <dlfcn.h>
#include <string.h>

#include <resolve.h>

static void _resolve_init(void) __attribute__((constructor));
static resolve_t *_singleton = NULL;

void _resolve_init(void) {
  void *main;

  assert(!_singleton);
  _singleton = NEW(resolve_t);

  _singleton -> images = list_create();
  list_set_free(_singleton -> images, (free_t) dlclose);
  _singleton -> functions = strvoid_dict_create();
  main = resolve_open(_singleton, NULL);
  if (!main) {
    error("Could not load main program image");
    list_free(_singleton -> images);
    dict_free(_singleton -> functions);
    _singleton = NULL;
  } else {
    atexit(resolve_free);
  }
}

resolve_t * resolve_get() {
  return _singleton;
}

void resolve_free() {
  if (_singleton) {
    list_free(_singleton -> images);
    dict_free(_singleton -> functions);
    free(_singleton);
    _singleton = NULL;
  }
}

resolve_t * resolve_open(resolve_t *resolve, char *image) {
  void *handle;

  handle = dlopen(image, RTLD_NOW | RTLD_GLOBAL);
  if (handle) {
    if (list_append(resolve -> images, handle)) {
      return resolve;
    }
    dlclose(handle);
  }
  return NULL;
}

void_t resolve_resolve(resolve_t *resolve, char *func_name) {
  listiterator_t *iter;
  void           *handle;
  void_t          ret;

  ret = (void_t) dict_get(resolve -> functions, func_name);
  if (ret) return ret;

  ret = NULL;
  for (iter = li_create(resolve -> images); !ret && iter && li_has_next(iter); ) {
    handle = li_next(iter);
    ret = (void_t) dlsym(handle, func_name);
  }
  li_free(iter);
  if (ret) {
    dict_put(resolve -> functions, strdup(func_name), ret);
  }
  return ret;
}

int resolve_library(char *library) {
  resolve_t *resolve;

  resolve = resolve_get();
  assert(resolve);
  return resolve_open(resolve, library) != NULL;
}

void_t resolve_function(char *func_name) {
  resolve_t *resolve;
  void_t     fnc;

  resolve = resolve_get();
  assert(resolve);
  fnc = resolve_resolve(resolve, func_name);
  if (!fnc) {
    error("Could not resolve function %s", func_name);
  }
  return fnc;
}
