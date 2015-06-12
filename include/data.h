/*
 * /obelix/include/data.h - Copyright (c) 2014 Jan de Visser <jan@finiandarcy.com>
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


#ifndef __DATA_H__
#define __DATA_H__

#include <core.h>
#include <array.h>
#include <dict.h>
#include <typedescr.h>

#ifdef  __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef enum _datatype {
  Exception,
  Pointer,
  String,
  Int,
  Float,
  Bool,
  List
} datatype_t;

typedef enum _free_semantics {
  Normal,
  DontFreeData,
  Constant
} free_semantics_t;

typedef struct _data {
  int               type;
  free_semantics_t  free_me;
  int               refs;
  char             *str;
} data_t;

typedef struct _pointer {
  data_t  _d;
  void   *ptr;
  int     size;
} pointer_t;

typedef struct _flt {
  data_t _d;
  double dbl;
} flt_t;

typedef struct _int {
  data_t _d;
  long   i;
} int_t;

struct _name;

typedef data_t * (*factory_t)(int, va_list);
typedef data_t * (*cast_t)(data_t *, int);
typedef data_t * (*resolve_name_t)(void *, char *);
typedef data_t * (*call_t)(void *, array_t *, dict_t *);
typedef data_t * (*setvalue_t)(void *, char *, data_t *);
typedef data_t * (*data_fnc_t)(data_t *);
typedef data_t * (*data2_fnc_t)(data_t *, data_t *);

extern data_t *        data_create_noinit(int);
extern data_t *        _data_new(int, size_t);
extern data_t *        data_create(int, ...);
extern data_t *        data_settype(data_t *, int);
extern data_t *        data_cast(data_t *, int);
extern data_t *        data_promote(data_t *);
extern data_t *        data_parse(int, char *);
extern data_t *        data_decode(char *);
extern void            data_free(data_t *);
extern int             data_hastype(data_t *, int);
extern typedescr_t *   data_typedescr(data_t *);
extern void_t          data_get_function(data_t *, int);
extern int             data_implements(data_t *, int);
extern int             data_is_numeric(data_t *);
extern int             data_is_exception(data_t *t);
extern int             data_is_callable(data_t *);
extern int             data_is_iterable(data_t *);
extern int             data_is_iterator(data_t *);
extern data_t *        data_copy(data_t *);
extern unsigned int    data_hash(data_t *);
extern data_t *        data_len(data_t *);
extern char *          data_tostring(data_t *);
extern int             data_cmp(data_t *, data_t *);
extern data_t *        data_call(data_t *, array_t *, dict_t *);
extern data_t *        data_method(data_t *, char *);
extern data_t *        data_execute(data_t *, char *, array_t *, dict_t *);
extern data_t *        data_resolve(data_t *, struct _name *);
extern data_t *        data_invoke(data_t *, struct _name *, array_t *, dict_t *);
extern int             data_has(data_t *, struct _name *);
extern int             data_has_callable(data_t *, struct _name *);
extern data_t *        data_get(data_t *, struct _name *);
extern data_t *        data_set(data_t *, struct _name *, data_t *);
extern data_t *        data_iter(data_t *);
extern data_t *        data_has_next(data_t *);
extern data_t *        data_next(data_t *);
extern data_t *        data_query(data_t *, data_t *);
extern data_t *        data_visit(data_t *, data_t *);
extern data_t *        data_reduce(data_t *, data_t *, data_t *);
extern data_t *        data_read(data_t *, char *, int);
extern data_t *        data_write(data_t *, char *, int);
extern data_t *        data_push(data_t *, data_t *);
extern data_t *        data_pop(data_t *);
extern int             data_count(void);

extern double          data_floatval(data_t *);
extern int             data_intval(data_t *);

#define data_new(dt,st)     ((st *) _data_new((dt), sizeof(st)))
#define data_type(d)        (((data_t *) (d)) -> type)
#define data_typename(d)    (typedescr_get(data_type((data_t *) (d))) -> type_name)

#define data_is_pointer(d)  ((d) && (data_hastype((d), Pointer)))
#define data_is_list(d)     ((d) && (data_hastype((d), List)))

#define data_arrayval(d)    ((array_t *) (((pointer_t *) (d)) -> ptr))
#define data_as_pointer(d)  (data_is_pointer((data_t *) (d)) ? (pointer_t *) (d) : NULL)
#define data_unwrap(d)      ((data_is_pointer((d)) ? (data_as_pointer(d) -> ptr) : NULL)

extern array_t *       data_add_all_reducer(data_t *, array_t *);
extern array_t *       data_add_all_as_data_reducer(char *, array_t *);
extern array_t *       data_add_strings_reducer(data_t *, array_t *);
extern dict_t *        data_put_all_reducer(entry_t *, dict_t *);

extern data_t *        data_null(void);
//extern data_t *        data_true(void);
//extern data_t *        data_false(void);
extern data_t *        data_create_list(array_t *);
extern array_t *       data_list_copy(data_t *);
extern array_t *       data_list_to_str_array(data_t *);
extern data_t *        data_str_array_to_list(array_t *);

extern int_t *         bool_true;
extern int_t *         bool_false;

#define data_true()    ((data_t *) bool_true)
#define data_false()   ((data_t *) bool_false)

/* -- Standard vtable functions ------------------------------------------ */
extern data_t *        data_embedded(int, va_list);

#define strdata_dict_create()   dict_set_tostring_data( \
                                  dict_set_tostring_key( \
                                    dict_set_free_data( \
                                      dict_set_free_key( \
                                        dict_set_hash( \
                                          dict_create((cmp_t) strcmp),\
                                          (hash_t) strhash), \
                                        (free_t) free), \
                                      (free_t) data_free), \
                                    (tostring_t) chars), \
                                  (tostring_t) data_tostring)

#define intdata_dict_create()   dict_set_tostring_data( \
                                  dict_set_tostring_key( \
                                    dict_set_free_data( \
                                      dict_create(NULL),\
                                      (free_t) data_free), \
                                    (tostring_t) itoa), \
                                  (tostring_t) data_tostring)

#define datadata_dict_create()   dict_set_tostring_data( \
                                  dict_set_tostring_key( \
                                    dict_set_free_data( \
                                      dict_set_free_key( \
                                        dict_set_hash( \
                                          dict_create((cmp_t) strcmp),\
                                          (hash_t) data_hash), \
                                        (free_t) data_free), \
                                      (free_t) data_free), \
                                    (tostring_t) data_tostring), \
                                  (tostring_t) data_tostring)
#define data_dict_get(d, k)     ((data_t *) dict_get((d), (k)))

#define data_array_create(i)    array_set_tostring( \
                                  array_set_free( \
                                    array_create((i)), \
                                    (free_t) data_free), \
                                  (tostring_t) data_tostring)
#define data_array_get(a, i)    ((data_t *) array_get((a), (i)))

#define data_list_create()     _list_set_tostring( \
                                 _list_set_hash( \
                                   _list_set_cmp( \
                                     _list_set_free( \
                                       list_create(), \
                                       (free_t) data_free), \
                                     (cmp_t) data_cmp), \
                                   (hash_t) data_hash), \
                                 (tostring_t) data_tostring)
#define data_list_pop(l)      (data_t *) list_pop((l))
#define data_list_shift(l)    (data_t *) list_shift((l))

#ifdef  __cplusplus
}
#endif /* __cplusplus */

#endif /* __DATA_H__ */
