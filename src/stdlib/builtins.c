/*
 * /obelix/src/stdlib/builtins.c - Copyright (c) 2014 Jan de Visser <jan@finiandarcy.com>
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
#include <unistd.h>

#include <array.h>
#include <data.h>
#include <dict.h>
#include <name.h>
#include <str.h>

__DLL_EXPORT__ data_t * _function_print(char *, array_t *, dict_t *);
__DLL_EXPORT__ data_t * _function_sleep(char *, array_t *, dict_t *);
__DLL_EXPORT__ data_t * _function_usleep(char *, array_t *, dict_t *);

data_t * _function_print(char *func_name, array_t *params, dict_t *kwargs) {
  data_t  *fmt;
  data_t  *s;

  (void) func_name;
  assert(array_size(params));
  fmt = (data_t *) array_get(params, 0);
  assert(fmt);
  if ((array_size(params) > 1) || (kwargs && dict_size(kwargs))) {
    params = array_slice(params, 1, -1);
    s = data_interpolate(fmt, params, kwargs);
    array_free(params);
  } else {
    s = fmt;
  }
  if (!data_is_exception(s)) {
    printf("%s\n", data_tostring(s));
    if (s != fmt) {
      data_free(s);
    }
    s = data_true();
  }
  return s;
}

data_t * _function_sleep(char *func_name, array_t *args, dict_t *kwargs) {
  data_t       *naptime;
  int           ret;

  (void) func_name;
  (void) kwargs;
  assert(array_size(args));
  naptime = (data_t *) array_get(args, 0);
  assert(naptime);
  ret = sleep((unsigned int) data_intval(naptime));
  return int_to_data(ret);
}

data_t * _function_usleep(char *func_name, array_t *args, dict_t *kwargs) {
  data_t  *naptime;

  (void) func_name;
  (void) kwargs;
  assert(array_size(args));
  naptime = (data_t *) array_get(args, 0);
  assert(naptime);
  return int_to_data(usleep(data_intval(naptime)));
}
