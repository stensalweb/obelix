/*
 * /obelix/include/arguments.h - Copyright (c) 2017 Jan de Visser <jan@finiandarcy.com>
 *
 * This file is part of Obelix.
 *
 * Obelix is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Obelix is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Obelix.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <data.h>
#include <dictionary.h>

#ifndef __ARGUMENTS_H__
#define __ARGUMENTS_H__

OBLCORE_IMPEXP arguments_t * arguments_create(array_t *, dict_t *);
OBLCORE_IMPEXP arguments_t * arguments_create_args(int num, ...);
OBLCORE_IMPEXP arguments_t * arguments_deepcopy(const arguments_t *);
OBLCORE_IMPEXP arguments_t * arguments_shift(const arguments_t *, data_t **);
OBLCORE_IMPEXP arguments_t * arguments_create_from_cmdline(int, char **);
OBLCORE_IMPEXP data_t *      arguments_get_arg(const arguments_t *, int);
OBLCORE_IMPEXP data_t *      arguments_get_kwarg(const arguments_t *, char *);
OBLCORE_IMPEXP int           arguments_has_kwarg(const arguments_t *, char *);
OBLCORE_IMPEXP arguments_t * _arguments_set_arg(arguments_t *, int, data_t *);
OBLCORE_IMPEXP arguments_t * _arguments_set_kwarg(arguments_t *, char *, data_t *);
OBLCORE_IMPEXP arguments_t * _arguments_push(arguments_t *arguments, data_t *);

OBLCORE_IMPEXP int Arguments;

type_skel(arguments, Arguments, arguments_t);

static inline char * arguments_arg_tostring(const arguments_t *arguments, int ix) {
  return data_tostring(data_uncopy(datalist_get(arguments -> args, ix)));
}

static inline char * arguments_kwarg_tostring(const arguments_t *arguments, char *kwarg) {
  return dictionary_value_tostring(arguments -> kwargs, kwarg);
}

static inline int arguments_has_args(const arguments_t *args) {
  return args && args -> args && datalist_size(args -> args);
}

static inline int arguments_args_size(const arguments_t *args) {
  return (args && args -> args) ? datalist_size(args -> args) : 0;
}

static inline int arguments_has_kwargs(const arguments_t *args) {
  return args && args -> kwargs && dictionary_size(args -> kwargs);
}

static inline int arguments_kwargs_size(const arguments_t *args) {
  return (args && args -> kwargs) ? dictionary_size(args -> kwargs) : 0;
}

static inline arguments_t * arguments_set_arg(arguments_t *args, int ix, void *data) {
  return _arguments_set_arg(args, ix, data_as_data(data));
}

static inline arguments_t * arguments_set_kwarg(arguments_t *args, char *key, void *data) {
  return _arguments_set_kwarg(args, key, data_as_data(data));
}

static inline arguments_t * arguments_push(arguments_t *args, void *data) {
  return _arguments_push(args, data_as_data(data));
}

static inline data_t * arguments_reduce_kwargs(arguments_t *args, void *reducer, void *initial) {
  return dictionary_reduce(args -> kwargs, reducer, initial);
}

#endif /* __ARGUMENTS_H__ */
