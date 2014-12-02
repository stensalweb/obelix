/*
 * list.h - Copyright (c) 2014 Jan de Visser <jan@finiandarcy.com>
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

#ifndef __LIST_H__
#define __LIST_H__

#include <core.h>
#include <str.h>

struct _list;

typedef struct _listnode {
  struct _listnode *prev;
  struct _listnode *next;
  struct _list     *list;
  void             *data;
} listnode_t;

typedef struct _list {
  listnode_t *head;
  listnode_t *tail;
  int         size;
  free_t      freefnc;
  cmp_t       cmp;
  tostring_t  tostring;
  hash_t      hash;
} list_t;

extern list_t *     list_create();
extern list_t *     _list_set_free(list_t *, visit_t);
extern list_t *     _list_set_cmp(list_t *, cmp_t);
extern list_t *     _list_set_tostring(list_t *, tostring_t);
extern list_t *     _list_set_hash(list_t *, hash_t);
extern void         list_free(list_t *);
extern list_t *     list_append(list_t *, void *);
extern list_t *     list_unshift(list_t *, void *);
extern list_t *     list_add_all(list_t *, list_t *);
extern int          list_size(list_t *);
extern unsigned int list_hash(list_t *);
extern void *       _list_reduce(list_t *, reduce_t, void *, reduce_type_t);
extern list_t *     _list_visit(list_t *, visit_t);
extern void *       _list_process(list_t *, reduce_t, void *);
extern list_t *     list_clear(list_t *);
extern void *       list_head(list_t *);
extern void *       list_tail(list_t *);
extern listnode_t * list_head_pointer(list_t *);
extern listnode_t * list_tail_pointer(list_t *);
extern void *       list_shift(list_t *);
extern void *       list_pop(list_t *);
extern str_t *      list_tostr(list_t *);

#define list_push(l, d)         list_append((l), (d))
#define list_peek(l)            list_tail((l))
#define list_empty(l)           (list_size((l)) == 0)
#define list_notempty(l)        (list_size((l)) > 0)

#define list_set_free(l, f)           (_list_set_free((l), (free_t) (f)))
#define list_set_cmp(l, f)            (_list_set_cmp((l), (cmp_t) (f)))
#define list_set_tostring(l, f)       (_list_set_tostring((l), (tostring_t) (f)))
#define list_set_hash(l, f)           (_list_set_hash((l), (hash_t) (f)))
#define list_reduce(l, r, d)          (_list_reduce((l), (reduce_t) (r), (d), RTObjects))
#define list_reduce_chars(l, r, d)    (_list_reduce((l), (reduce_t) (r), (d), RTChars))
#define list_reduce_str(l, r, d)      (_list_reduce((l), (reduce_t) (r), (d), RTStrs))
#define list_visit(l, v)              (_list_visit((l), (visit_t) (v)))
#define list_process(l, r, d)         (_list_process((l), (reduce_t) (r), (d)))

typedef struct _listiter {
  list_t     *list;
  listnode_t *current;
} listiterator_t;

extern listiterator_t * li_create(list_t *);
extern void             li_free(listiterator_t *);
extern void             li_head(listiterator_t *);
extern void             li_tail(listiterator_t *);
extern void *           li_current(listiterator_t *);
extern listnode_t *     li_pointer(listiterator_t *);
extern void             li_replace(listiterator_t *, void *);
extern int              li_insert(listiterator_t *, void *);
extern void             li_remove(listiterator_t *);
extern int              li_has_next(listiterator_t *);
extern int              li_has_prev(listiterator_t *);
extern void *           li_next(listiterator_t *);
extern void *           li_prev(listiterator_t *);

#endif /* __LIST_H__ */
