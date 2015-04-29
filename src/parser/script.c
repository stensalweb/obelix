/*
 * /obelix/src/script.c - Copyright (c) 2014 Jan de Visser <jan@finiandarcy.com>
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

#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <exception.h>
#include <logging.h>
#include <namespace.h>
#include <script.h>
#include <str.h>
#include <thread.h>
#include <wrapper.h>

#define  MAX_STACKDEPTH         200

int script_debug = 0;

static void             _script_init(void) __attribute__((constructor(102)));

static closure_t *      _closure_create_closure_reducer(entry_t *, closure_t *);
static listnode_t *     _closure_execute_instruction(instruction_t *, closure_t *);
static data_t *         _closure_get(closure_t *, char *);
static data_t *         _closure_start(closure_t *);

/* -- data_t type description structures ---------------------------------- */

static vtable_t _wrapper_vtable_script[] = {
  { .id = FunctionCopy,     .fnc = (void_t) script_copy },
  { .id = FunctionCmp,      .fnc = (void_t) script_cmp },
  { .id = FunctionFree,     .fnc = (void_t) script_free },
  { .id = FunctionToString, .fnc = (void_t) script_tostring },
  { .id = FunctionCall,     .fnc = (void_t) script_execute },
  { .id = FunctionNone,     .fnc = NULL }
};

static vtable_t _wrapper_vtable_bound_method[] = {
  { .id = FunctionCopy,     .fnc = (void_t) bound_method_copy },
  { .id = FunctionCmp,      .fnc = (void_t) bound_method_cmp },
  { .id = FunctionFree,     .fnc = (void_t) bound_method_free },
  { .id = FunctionToString, .fnc = (void_t) bound_method_tostring },
  { .id = FunctionCall,     .fnc = (void_t) bound_method_execute },
  { .id = FunctionNone,     .fnc = NULL }
};

static vtable_t _wrapper_vtable_closure[] = {
  { .id = FunctionCopy,     .fnc = (void_t) closure_copy },
  { .id = FunctionCmp,      .fnc = (void_t) closure_cmp },
  { .id = FunctionHash,     .fnc = (void_t) closure_hash },
  { .id = FunctionFree,     .fnc = (void_t) closure_free },
  { .id = FunctionToString, .fnc = (void_t) closure_tostring },
  { .id = FunctionResolve,  .fnc = (void_t) closure_resolve },
  { .id = FunctionCall,     .fnc = (void_t) closure_execute },
  { .id = FunctionSet,      .fnc = (void_t) closure_set },
  { .id = FunctionNone,     .fnc = NULL }
};

/* ------------------------------------------------------------------------ */

void _script_init(void) {
  logging_register_category("script", &script_debug);
  wrapper_register(Script, "script", _wrapper_vtable_script);
  wrapper_register(Closure, "closure", _wrapper_vtable_closure);
  wrapper_register(BoundMethod, "boundmethod", _wrapper_vtable_bound_method);
}

/* -- Script data functions ----------------------------------------------- */

data_t * data_create_script(script_t *script) {
  return data_create(Script, script);
}

data_t * data_create_closure(closure_t *closure) {
  return data_create(Closure, closure);
}

/* -- S C R I P T  P U B L I C  F U N C T I O N S  ------------------------ */

script_t * script_create(module_t *mod, script_t *up, char *name) {
  script_t   *ret;
  char       *anon = NULL;

  if (!name) {
    asprintf(&anon, "__anon__%d__", hashptr(ret));
    name = anon;
  }
  if (script_debug) {
    debug("Creating script '%s'", name);
  }
  ret = NEW(script_t);
  
  ret -> instructions = list_create();
  list_set_free(ret -> instructions, (free_t) instruction_free);
  list_set_tostring(ret -> instructions, (tostring_t) instruction_tostring);

  ret -> functions = strdata_dict_create();
  ret -> labels = strvoid_dict_create();
  ret -> pending_labels = datastack_create("pending labels");
  ret -> params = NULL;
  ret -> async = 0;
  ret -> current_line = -1;

  if (up) {
    dict_put(up -> functions, strdup(name), data_create_script(ret));
    ret -> up = script_copy(up);
    ret -> mod = mod_copy(up -> mod);
    ret -> name = name_deepcopy(up -> name);
    name_extend(ret -> name, name);
  } else {
    assert(mod);
    ret -> mod = mod_copy(mod);
    ret -> up = NULL;
    ret -> name = name_create(0);
  }
  ret -> fullname = NULL;
  ret -> str = NULL;
  ret -> refs = 1;
  free(anon);
  return ret;
}

script_t * script_copy(script_t *script) {
  if (script) {
    script -> refs++;
  }
  return script;
}

name_t * script_fullname(script_t *script) {
  if (!script -> fullname) {
    script -> fullname = name_deepcopy(script -> mod -> name);
    name_append(script -> fullname, script -> name);
  }
  return script -> fullname;
}

char * script_tostring(script_t *script) {
  if (!script -> str) {
    script -> str = strdup(name_tostring(script_fullname(script)));
  }
  return script -> str;
}

void script_free(script_t *script) {
  if (script && (--script -> refs <= 0)) {
    datastack_free(script -> pending_labels);
    list_free(script -> instructions);
    dict_free(script -> labels);
    array_free(script -> params);
    dict_free(script -> functions);
    script_free(script -> up);
    mod_free(script -> mod);
    name_free(script -> name);
    name_free(script -> fullname);
    free(script -> str);
    free(script);
  }
}

int script_cmp(script_t *s1, script_t *s2) {
  if (s1 && !s2) {
    return 1;
  } else if (!s1 && s2) {
    return -1;
  } else if (!s1 && !s2) {
    return 0;
  } else {
    return name_cmp(script_fullname(s1), script_fullname(s2));
  }
}

unsigned int script_hash(script_t *script) {
  return (script) ? name_hash(script_fullname(script)) : 0L;
}

script_t * script_push_instruction(script_t *script, instruction_t *instruction) {
  listnode_t    *node;
  data_t        *label;
  int            line;
  instruction_t *last;

  last = list_tail(script -> instructions);
  line = (last) ? last -> line : -1;
  if (script -> current_line > line) {
    instruction -> line = script -> current_line;
  }
  list_push(script -> instructions, instruction);
  if (!datastack_empty(script -> pending_labels)) {
    label = datastack_peek(script -> pending_labels);
    instruction_set_label(instruction, label);
    node = list_tail_pointer(script -> instructions);
    while (!datastack_empty(script -> pending_labels)) {
      label = datastack_pop(script -> pending_labels);
      dict_put(script -> labels, strdup(data_charval(label)), node);
      data_free(label);
    }
  }
  return script;
}

void script_list(script_t *script) {
  instruction_t *instr;
  
  debug("==================================================================");
  debug("Script Listing - %s", script_tostring(script));
  debug("------------------------------------------------------------------");
  debug("%-6s %-11.11s%-15.15s", "Line", "Label", "Instruction");
  debug("------------------------------------------------------------------");
  for (list_start(script -> instructions); list_has_next(script -> instructions); ) {
    instr = (instruction_t *) list_next(script -> instructions);
    debug(instruction_tostring(instr));
  }
  debug("==================================================================");
}

script_t * script_get_toplevel(script_t *script) {
  script_t *ret;

  for (ret = script; ret -> up; ret = ret -> up);
  return ret;
}

data_t * script_execute(script_t *script, array_t *args, dict_t *kwargs) {
  closure_t *closure;
  data_t    *retval;
  
  if (script_debug) {
    debug("script_execute(%s)", script_tostring(script));
  }
  closure = script_create_closure(script, NULL, NULL);
  retval = closure_execute(closure, args, kwargs);
  closure_free(closure);
  if (script_debug) {
    debug("  script_execute returns %s", data_tostring(retval));
  }
  return retval;
}

data_t * script_create_object(script_t *script, array_t *params, dict_t *kwparams) {
  object_t  *retobj;
  data_t    *retval;
  data_t    *dscript;
  data_t    *bm;
  data_t    *self;

  if (script_debug) {
    debug("script_create_object(%s)", script_tostring(script));
  }
  retobj = object_create(dscript = data_create(Script, script));
  if (!script -> up) {
    script -> mod -> obj = object_copy(retobj);
  }
  retobj -> constructing = TRUE;
  retval = bound_method_execute(data_boundmethodval(retobj -> constructor), 
                                params, kwparams);
  retobj -> constructing = FALSE;
  if (!data_is_exception(retval)) {
    retobj -> retval = retval;
    retval = data_create(Object, retobj);
  }
  if (script_debug) {
    debug("  script_create_object returns %s", data_tostring(retval));
  }
  data_free(dscript);
  return retval;
}

bound_method_t * script_bind(script_t *script, object_t *object) {
  bound_method_t *ret = bound_method_create(script, object);
  
  return ret;
}

closure_t * script_create_closure(script_t *script, closure_t *up, data_t *self) {
  return closure_create(script, up, self);
}

/* -- B O U N D  M E T H O D  F U N C T I O N S   ------------------------- */

bound_method_t * bound_method_create(script_t *script, object_t *self) {
  bound_method_t *ret = NEW(bound_method_t);
  
  ret -> script = script_copy(script);
  ret -> self = object_copy(self);
  ret -> closure = NULL;
  ret -> str = NULL;
  ret -> refs = 1;
  return ret;
}

void bound_method_free(bound_method_t *bm) {
  if (bm) {
    bm -> refs--;
    if (bm -> refs <= 0) {
      script_free(bm -> script);
      object_free(bm -> self);
      closure_free(bm -> closure);
      free(bm -> str);
      free(bm);
    }
  }
}

bound_method_t * bound_method_copy(bound_method_t *bm) {
  if (bm) {
    bm -> refs++;
  }
  return bm;
}

int bound_method_cmp(bound_method_t *bm1, bound_method_t *bm2) {
  return (!object_cmp(bm1 -> self, bm2 -> self)) 
    ? script_cmp(bm1 -> script, bm2 -> script)
    : 0;
}

char * bound_method_tostring(bound_method_t *bm) {
  if (bm -> script) {
    asprintf(&bm -> str, "%s (bound)", 
             script_tostring(bm -> script));
  } else {
    bm -> str = strdup("uninitialized");
  }
  return bm -> str;
}

data_t * bound_method_execute(bound_method_t *bm, array_t *params, dict_t *kwparams) {
  closure_t *closure;
  data_t    *self;
  data_t    *ret;

  self = (bm -> self) ? data_create(Object, bm -> self) : NULL;
  closure = closure_create(bm -> script, bm -> closure, self);
  ret = closure_execute(closure, params, kwparams);
  closure_free(closure);
  return ret;  
}

/* -- C L O S U R E  S T A T I C  F U N C T I O N S ------------------------*/

closure_t * _closure_create_closure_reducer(entry_t *entry, closure_t *closure) {
  char           *name = (char *) entry -> key;
  data_t         *func = (data_t *) entry -> value;
  data_t         *value = NULL;
  bound_method_t *bm;
  object_t       *self;

  if (data_is_script(func)) {
    self = data_objectval(closure -> self);
    bm = bound_method_create(data_scriptval(func), self);
    bm -> closure = closure;
    value = data_create(BoundMethod, bm);
  } else {
    /* Native function */
    /* TODO: Do we have a closure-like structure to bind the function to self? */
    value = data_copy(func);
  }
  if (value) {
    closure_set(closure, name, value);
  }
  return closure;
}

listnode_t * _closure_execute_instruction(instruction_t *instr, closure_t *closure) {
  data_t     *ret;
  char       *label = NULL;
  listnode_t *node = NULL;
  data_t     *catchpoint;

  ret = ns_exit_code(closure -> script -> mod -> ns);
  if (!ret) {
    if (instr -> line > 0) {
      closure -> line = instr->line;
    }
    ret = instruction_execute(instr, closure);
  }
  if (ret) {
    switch (data_type(ret)) {
      case String:
        label = strdup(data_tostring(ret));
        break;
      case Exception:
        catchpoint = datastack_pop(closure -> catchpoints);
        label = strdup(data_charval(catchpoint));
        data_free(catchpoint);
        closure_push(closure, data_copy(ret));
        break;
      default:
        debug("Instr '%s' returned '%s'", instruction_tostring(instr), data_tostring(ret));
        assert(0);
        break;
    }
    data_free(ret);
  }
  if (label) {
    if (script_debug) {
      debug("  Jumping to '%s'", label);
    }
    node = (listnode_t *) dict_get(closure -> script -> labels, label);
    assert(node);
    free(label);
  }
  return node;
}

data_t * _closure_get(closure_t *closure, char *varname) {
  data_t *ret = NULL;

  if (closure -> self && !strcmp(varname, "self")) {
    ret = closure -> self;
  } else if (closure -> variables) {
    ret = (data_t *) dict_get(closure -> variables, varname);
  }
  if (!ret && closure -> params) {
    /* 
     * We store the passed-in parameter values. If the parameter variable gets
     * re-assigned, the new value shadows the old one because it will be set 
     * in the variables dict, not in the params one.
     */
    ret = (data_t *) dict_get(closure -> params, varname);    
  }
  return ret;
}

data_t * _closure_start(closure_t *closure) {
  data_t      *ret;
  exception_t *e;

  ret = data_thread_frame_element(data_create_closure(closure));
  if (!data_is_exception(ret)) {
    list_process(closure -> script -> instructions,
                (reduce_t) _closure_execute_instruction,
                closure);
    ret = (datastack_notempty(closure -> stack)) ? closure_pop(closure) : data_null();
    if (script_debug) {
      debug("    Execution of %s done: %s", closure_tostring(closure), data_tostring(ret));
    }
    if (data_is_exception(ret)) {
      e = data_exceptionval(ret);
      if ((e -> code == ErrorExit) && (data_exceptionval(ret) -> throwable)) {
        ns_exit(closure -> script -> mod -> ns, ret);
      }
    }
  }
  
  if (closure -> free_params) {
    /*
     * kwargs was assigned to closure -> params. Freeing the closure should
     * not free the params in that case, since the owner is responsible for
     * kwargs,
     */
    closure -> params = NULL;
  }
  return ret;
}

/* -- C L O S U R E  P U B L I C  F U N C T I O N S ------------------------*/

closure_t * closure_create(script_t *script, closure_t *up, data_t *self) {
  closure_t *ret;
  int        depth;

  if (script_debug) {
    debug("Creating closure for script '%s'", script_tostring(script));
  }
  
  ret = NEW(closure_t);
  ret -> script = script_copy(script);

  ret -> variables = NULL;
  ret -> params = NULL;
  ret -> stack = NULL;
  ret -> catchpoints = NULL;
  ret -> up = up;
  ret -> depth = depth;
  ret -> self = data_copy(self);

  dict_reduce(script -> functions, 
              (reduce_t) _closure_create_closure_reducer, ret);
  ret -> refs++;
  
  if (!up) {
    /* Import standard lib: */
    closure_import(ret, NULL);
  }
  return ret;
}

void closure_free(closure_t *closure) {
  if (closure) {
    closure -> refs--;
    if (closure <= 0) {
      script_free(closure -> script);
      datastack_free(closure -> stack);
      datastack_free(closure -> catchpoints);
      dict_free(closure -> variables);
      dict_free(closure -> params);
      data_free(closure -> self);
      data_free(closure -> thread);
      free(closure -> str);
      free(closure);
    }
  }
}

closure_t * closure_copy(closure_t *closure) {
  if (closure) {
    closure -> refs++;
  }
  return closure;
}

char * closure_tostring(closure_t *closure) {
  char *params;
  
  if (!closure -> str) {
    params = (closure -> params && dict_size(closure -> params)) 
               ? dict_tostring_custom(closure -> params, "", "%s=%s", ",", "")
               : strdup("");
    asprintf(&closure -> str, "%s(%s)",
             script_tostring(closure -> script),
             params);
    free(params);
  }
  return closure -> str;
           
}

int closure_cmp(closure_t *c1, closure_t *c2) {
  return name_cmp(c1 -> script -> name, c2 -> script -> name);
}

unsigned int closure_hash(closure_t *closure) {
  return hashptr(closure);
}

data_t * closure_pop(closure_t *closure) {
  return datastack_pop(closure -> stack);
}

data_t * closure_peek(closure_t *closure) {
  return datastack_peek(closure -> stack);
}

/**
 * @brief Pushes a data value onto the closure run-time stack.
 * 
 * @param closure The closure on whose stack the value should be
 * pushed.
 * @param value The data value to push onto the closure's stack.
 * @return closure_t* The same closure as the one passed in.
 */
closure_t * closure_push(closure_t *closure, data_t *value) {
  datastack_push(closure -> stack, data_copy(value));
  return closure;
}

data_t * closure_import(closure_t *closure, name_t *module) {
  data_t *ret;
  
  ret = mod_import(closure -> script -> mod, module);
}

data_t * closure_set(closure_t *closure, char *name, data_t *value) {
  if (script_debug) {
    if (strcmp(name, "self")) {
      debug("  Setting local '%s' = '%s' in closure for %s",
            name, data_tostring(value), closure_tostring(closure));
    } else {
      debug("  Setting local '%s' in closure for %s",
            name, closure_tostring(closure));
    }
  }
  if (!closure -> variables) {
    closure -> variables = strdata_dict_create();
  }
  dict_put(closure -> variables, strdup(name), data_copy(value));
  return value;
}

data_t * closure_get(closure_t *closure, char *varname) {
  data_t *ret = _closure_get(closure, varname);
  
  if (ret) {
    ret = data_copy(ret);
  } else {
    ret = data_exception(ErrorName,
                     "Closure '%s' has no attribute '%s'",
                     closure_tostring(closure),
                     varname);
  }
  return ret;
}

int closure_has(closure_t *closure, char *name) {
  int ret;

  ret = (closure -> self && !strcmp(name, "self")) ||
        (closure -> variables && dict_has_key(closure -> variables, name)) || 
        (closure -> params && dict_has_key(closure -> params, name));
  if (script_debug) {
    debug("   closure_has('%s', '%s'): %d", closure_tostring(closure), name, ret);
  }
  return ret;
}

data_t * closure_resolve(closure_t *closure, char *name) {
  data_t  *ret = _closure_get(closure, name);
  
  if (!ret) {
    if (closure -> up) {
      if (!strcmp(name, "^") ||
          !strcmp(name, name_last(script_fullname(closure -> up -> script)))) {
        ret = data_create(Closure, closure -> up);
      } else {
        ret = closure_resolve(closure -> up, name);
      }
    } else {
      ret = mod_resolve(closure -> script -> mod, name);
    }
  }
  return data_copy(ret);
}

data_t * closure_execute(closure_t *closure, array_t *args, dict_t *kwargs) {
  int        ix;
  data_t    *param;
  script_t  *script;
  object_t  *self;
  pthread_t  thr_id;

  script = closure -> script;
  closure -> free_params = FALSE;
  if (script -> params && array_size(script -> params)) {
    if (!args || (array_size(script -> params) > array_size(args))) {
      return data_exception(ErrorArgCount, 
                        "Function %s takes %d arguments, %d provided",
                        name_tostring(script -> name), 
                        array_size(script -> params),
                        (args) ? array_size(args) : 0);
    }
    if (script -> async || !kwargs) {
      closure -> params = strdata_dict_create();
      closure -> free_params = TRUE;
    } else {
      closure -> params = kwargs;
    }
    if (script -> async && kwargs) {
      dict_reduce(kwargs, (reduce_t) data_put_all_reducer, closure -> params);
    }
    for (ix = 0; ix < array_size(args); ix++) {
      param = data_copy(data_array_get(args, ix));
      dict_put(closure -> params, 
               (char *) array_get(script -> params, ix),
               param);
    }
  }
  
  if (!closure -> stack) {
    closure -> stack = datastack_create(script_tostring(script));
    datastack_set_debug(closure -> stack, script_debug);
  } else {
    datastack_clear(closure -> stack);
  }
  
  if (!closure -> catchpoints) {
    closure -> catchpoints = datastack_create("catchpoints");
    datastack_set_debug(closure -> catchpoints, script_debug);
  } else {
    datastack_clear(closure -> catchpoints);
  }
  datastack_push(closure -> catchpoints, data_create(String, "ERROR"));
  
  if (script -> async) {
    closure -> thread = data_create(Thread, 
                                    closure_tostring(closure),
                                    (threadproc_t) _closure_start,
                                    closure_copy(closure));
    return closure -> thread;
  } else {
    closure -> thread = data_current_thread();
    return _closure_start(closure);
  }
}

closure_t * closure_set_location(closure_t *closure, data_t *location) {
  if (data_type(location) == Int) {
    closure -> line = data_intval(location);
  }
  return closure;
}

data_t * closure_stash(closure_t *closure, unsigned int stash, data_t *data) {
  if (stash < NUM_STASHES) {
    closure -> stashes[stash] = data;
    return data;
  } else {
    return NULL;
  }
}

data_t * closure_unstash(closure_t *closure, unsigned int stash) {
  if (stash < NUM_STASHES) {
    return closure -> stashes[stash];
  } else {
    return NULL;
  }
}
