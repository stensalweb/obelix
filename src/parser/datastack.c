/*
 * /obelix/src/datastack.c - Copyright (c) 2014 Jan de Visser <jan@finiandarcy.com>
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

#include <string.h>

#include <datastack.h>

typedef struct _bookmark {
  int depth;
} bookmark_t;

typedef struct _counter {
  int count;
} counter_t;

static bookmark_t * _bookmark_create(datastack_t *);
static void         _bookmark_free(bookmark_t *);
static int          _bookmark_depth(bookmark_t *);

static counter_t *  _counter_create();
static void         _counter_free(counter_t *);
static int          _counter_count(counter_t *);
static counter_t *  _counter_incr(counter_t *);

static void         _stack_list_visitor(data_t *);

/* ------------------------------------------------------------------------ */

bookmark_t * _bookmark_create(datastack_t *stack) {
  bookmark_t *ret = NEW(bookmark_t);

  ret -> depth = datastack_depth(stack);
  return ret;
}

void _bookmark_free(bookmark_t *bookmark) {
  if (bookmark) {
    free(bookmark);
  }
}

int _bookmark_depth(bookmark_t *bookmark) {
  return bookmark -> depth;
}

/* ------------------------------------------------------------------------ */

counter_t * _counter_create(void) {
  counter_t *ret = NEW(counter_t);
  
  ret -> count = 0;
  return ret;
}

void _counter_free(counter_t *counter) {
  if (counter) {
    free(counter);
  }
}

int _counter_count(counter_t *counter) {
  return counter -> count;
}

counter_t * _counter_incr(counter_t *counter) {
  counter -> count++;
  return counter;
}

/* ------------------------------------------------------------------------ */

void _stack_list_visitor(data_t *entry) {
  debug("   . %s", data_debugstr(entry));
}

/* ------------------------------------------------------------------------ */

datastack_t * datastack_create(char *name) {
  datastack_t *ret;

  ret = NEW(datastack_t);
  ret -> name = strdup(name);
  ret -> debug = 0;
  ret -> list = data_list_create();
  ret -> bookmarks = NULL;
  ret -> counters = NULL;
  return ret;
}

datastack_t * datastack_set_debug(datastack_t *stack, int debug) {
  stack -> debug = debug;
  return stack;
}

void datastack_free(datastack_t *stack) {
  if (stack) {
    list_free(stack -> list);
    list_free(stack -> bookmarks);
    free(stack -> name);
    free(stack);
  }
}

char * datastack_tostring(datastack_t *stack) {
  return stack -> name;
}

int datastack_hash(datastack_t *stack) {
  return strhash(stack -> name);
}

int datastack_cmp(datastack_t *s1, datastack_t *s2) {
  return strcmp(s1 -> name, s2 -> name);
}
							    
int datastack_depth(datastack_t *stack) {
  return list_size(stack -> list);
}

data_t * datastack_pop(datastack_t *stack) {
  data_t *ret;

  ret = (data_t *) list_pop(stack -> list);
  if (stack -> debug) {
    debug("  - %s", data_tostring(ret));
  }
  return ret;
}

data_t * datastack_peek(datastack_t *stack) {
  return (data_t *) list_tail(stack -> list);
}

datastack_t * datastack_push(datastack_t *stack, data_t *data) {
  list_push(stack -> list, data);
  if (stack -> debug) {
    debug("After push:");
    datastack_list(stack);
  }
  return stack;
}

datastack_t * datastack_push_int(datastack_t *stack, long value) {
  return datastack_push(stack, data_create(Int, value));
}

datastack_t * datastack_push_string(datastack_t *stack, char *value) {
  return datastack_push(stack, data_create(String, value));
}

datastack_t * datastack_push_float(datastack_t *stack, double value) {
  return datastack_push(stack, data_create(Float, value));
}

datastack_t * datastack_list(datastack_t *stack) {
  debug("-- Stack '%s' ---------------------------------------------", stack -> name);
  list_visit(stack -> list, (visit_t) _stack_list_visitor);
  debug("------------------------------------------------------------------");
  return stack;
}

datastack_t * datastack_clear(datastack_t *stack) {
  list_clear(stack -> list);
  return stack;
}

/* ------------------------------------------------------------------------ */

datastack_t * datastack_bookmark(datastack_t *stack) {
  if (!stack -> bookmarks) {
    stack -> bookmarks = list_create();
    list_set_free(stack -> bookmarks, (free_t) _bookmark_free);
  }
  list_push(stack -> bookmarks, _bookmark_create(stack));
  return stack;
}

array_t * datastack_rollup(datastack_t *stack) {
  bookmark_t *bookmark;
  data_t     *data;
  array_t    *ret;
  int         num;
  int         ix;

  if (!stack -> bookmarks || list_empty(stack -> bookmarks)) {
    return NULL;
  }
  bookmark = (bookmark_t *) list_tail(stack -> bookmarks);
  assert(_bookmark_depth(bookmark) <= datastack_depth(stack));
  num = datastack_depth(stack) - _bookmark_depth(bookmark);
  ret = data_array_create(num);
  for (ix = num - 1; ix >= 0; ix--) {
    data = datastack_pop(stack);
    array_set(ret, ix, data_copy(data));
    data_free(data);
  }
  bookmark = list_pop(stack -> bookmarks);
  _bookmark_free(bookmark);
  return ret;
}

name_t * datastack_rollup_name(datastack_t *stack) {
  array_t *arr;
  int      ix;
  data_t  *data;
  name_t  *ret;
  char    *str;

  arr = datastack_rollup(stack);
  ret = name_create(0);
  for (ix = 0; ix < array_size(arr); ix++) {
    data = data_array_get(arr, ix);
    name_extend_data(ret, data);
  }
  array_free(arr);
  return ret;
}

/* ------------------------------------------------------------------------ */

datastack_t * datastack_new_counter(datastack_t *stack) {
  if (!stack -> counters) {
    stack -> counters = list_create();
    list_set_free(stack -> counters, (free_t) _counter_free);
  }
  list_push(stack -> counters, _counter_create());
  return stack;
}

datastack_t * datastack_increment(datastack_t *stack) {
  counter_t *counter;

  if (!stack -> counters || list_empty(stack -> counters)) {
    return NULL;
  }
  counter = (counter_t *) list_tail(stack -> counters);
  _counter_incr(counter);
  return stack; 
}

int datastack_count(datastack_t *stack) {
  counter_t *counter;
  int        count;

  if (!stack -> counters || list_empty(stack -> counters)) {
    return -1;
  }
  counter = list_pop(stack -> counters);
  count = counter -> count;
  _counter_free(counter);
  return count;
}