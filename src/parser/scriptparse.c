/*
 * /obelix/src/scriptparse.c - Copyright (c) 2014 Jan de Visser <jan@finiandarcy.com>
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
#include <string.h>

#include <exception.h>
#include <namespace.h>
#include <script.h>
#include <scriptparse.h>

static name_t *   _script_pop_operation(parser_t *);
static name_t *   _script_pop_and_build_varname(parser_t *);
static parser_t * _script_parse_emit_epilog(parser_t *);

/* ----------------------------------------------------------------------- */

name_t * _script_pop_operation(parser_t *parser) {
  data_t *data;
  char   *opstr;
  int     opint;
  char    buf[2];
  name_t *ret;
  
  data = datastack_pop(parser -> stack);
  switch (data_type(data)) {
    case String:
      opstr = data_charval(data);
      break;
    case Int:
      opint = data_intval(data);
      buf[0] = opint;
      buf[1] = 0;
      opstr = buf;
      break;
  }
  ret = name_create(1, opstr);
  data_free(data);
  if (parser_debug) {
    debug(" -- operation: %s", name_tostring(ret));
  }
  return ret;
}

name_t * _script_pop_and_build_varname(parser_t *parser) {
  data_t  *data;
  data_t  *count;
  int      ix;
  array_t *arr;
  name_t  *ret;
  str_t   *debugstr;

  count = (data_t *) datastack_pop(parser -> stack);
  if (parser_debug) {
    debug("  -- #components: %d", data_intval(count));
  }
  arr = str_array_create(data_intval(count));
  for (ix = data_intval(count) - 1; ix >= 0; ix--) {
    data = datastack_pop(parser -> stack);
    assert(data_type(data) == String);
    array_set(arr, ix, strdup(data_charval(data)));
    data_free(data);
  }
  ret = name_create(0);
  name_append_array(ret, arr);
  array_free(arr);
  data_free(count);
  if (parser_debug) {
    debug("  -- varname: %s", name_tostring(ret));
  }
  return ret;
}

parser_t * _script_parse_emit_epilog(parser_t *parser) {
  script_t *script;
  data_t   *data;
  name_t   *error_var;

  script = (script_t *) parser -> data;
  data = data_create(Int, 0);
  script_push_instruction(script, instruction_create_pushval(data));
  data_free(data);
  script_push_instruction(script, instruction_create_jump("END"));
  
  script -> label = strdup("ERROR");
  error_var = name_create(1, "$$ERROR");
  script_push_instruction(script, instruction_create_pushvar(error_var));
  name_free(error_var);
  
  script -> label = strdup("END");
  script_parse_emit_nop(parser);
  if (script_debug) {
    script_list(script);
  }
  return parser;
}

/* ----------------------------------------------------------------------- */

parser_t * script_parse_init(parser_t *parser) {
  char        *name;
  namespace_t *ns = NULL;
  data_t      *data;

  if (parser_debug) {
    debug("script_parse_init");
  }
  data = parser_get(parser, "name");
  name = data_tostring(data);
  data = parser_get(parser, "ns");
  ns = (data) ? data -> ptrval : NULL;
  assert(ns);
  parser -> data = script_create(ns, NULL, name);
  return parser;
}

parser_t * script_parse_done(parser_t *parser) {
  if (parser_debug) {
    debug("script_parse_done");
  }
  return _script_parse_emit_epilog(parser);
}

parser_t * script_make_nvp(parser_t *parser) {
  script_t *script;
  data_t   *name;
  data_t   *data;

  data = token_todata(parser -> last_token);
  assert(data);
  name = datastack_pop(parser -> stack);
  assert(name);
  if (parser_debug) {
    debug(" -- %s = %s", data_tostring(name), data_tostring(data));
  }
  datastack_push(parser -> stack, data_create(NVP, name, data));
  return parser;
}

/* ----------------------------------------------------------------------- */

/*
 * Stack frame for function call:
 *
 *   | kwarg           |
 *   +-----------------+
 *   | kwarg           |
 *   +-----------------+    <- Bookmark for kwarg names
 *   | func_name       |Name
 *   +-----------------+
 *   | . . .           |
 */
parser_t * script_parse_init_function(parser_t *parser) {
  datastack_new_counter(parser -> stack);
  datastack_bookmark(parser -> stack);
  return parser;
}

parser_t * script_parse_setup_function(parser_t *parser, data_t *func) {
  name_t *name;

  name = name_create(1, data_tostring(func));
  datastack_push(parser -> stack, data_create(Name, name));
  script_parse_init_function(parser);
  return parser;
}

/* ----------------------------------------------------------------------- */

parser_t * script_parse_emit_assign(parser_t *parser) {
  data_t   *varname;
  script_t *script;

  script = parser -> data;
  varname = datastack_pop(parser -> stack);
  script_push_instruction(script, 
			  instruction_create_assign(data_nameval(varname)));
  data_free(varname);
  return parser;
}

parser_t * script_parse_emit_pushvar(parser_t *parser) {
  script_t *script;
  data_t   *varname;

  script = parser -> data;
  varname = datastack_pop(parser -> stack);
  script_push_instruction(script, 
			  instruction_create_pushvar(data_nameval(varname)));
  data_free(varname);
  return parser;
}

parser_t * script_parse_emit_pushval(parser_t *parser) {
  script_t *script;
  data_t   *data;

  script = parser -> data;
  data = token_todata(parser -> last_token);
  assert(data);
  if (parser_debug) {
    debug(" -- val: %s", data_debugstr(data));
  }
  script_push_instruction(script, instruction_create_pushval(data));
  data_free(data);
  return parser;
}

parser_t * script_parse_emit_pushval_from_stack(parser_t *parser) {
  script_t *script;
  data_t   *data;

  script = parser -> data;
  data = datastack_pop(parser -> stack);
  assert(data);
  if (parser_debug) {
    debug(" -- val: %s", data_debugstr(data));
  }
  script_push_instruction(script, instruction_create_pushval(data));
  data_free(data);
  return parser;
}

parser_t * script_parse_emit_pushconst(parser_t *parser, data_t *constval) {
  script_t *script;
  data_t   *data;

  script = parser -> data;
  data = data_decode(data_tostring(constval));
  assert(data);
  if (parser_debug) {
    debug(" -- val: %s", data_debugstr(data));
  }
  script_push_instruction(script, instruction_create_pushval(data));
  data_free(data);
  return parser;
}

parser_t *script_parse_push_signed_val(parser_t *parser) {
  data_t   *data;
  data_t   *signed_val;
  name_t   *op;
  script_t *script;

  script = parser -> data;
  data = token_todata(parser -> last_token);
  assert(data);
  op = _script_pop_operation(parser);
  if (parser_debug) {
    debug(" -- val: %s %s", name_tostring(op), data_debugstr(data));
  }
  signed_val = data_invoke(data, op, NULL, NULL);
  name_free(op);
  assert(data_type(signed_val) == data_type(data));
  script_push_instruction(script, instruction_create_pushval(signed_val));
  data_free(data);
  data_free(signed_val);
  return parser;  
}

parser_t *script_parse_emit_unary_op(parser_t *parser) {
  data_t *op = datastack_pop(parser -> stack);
  name_t *name = name_create(1, data_tostring(op));

  script_push_instruction(parser -> data,
    instruction_create_function(name, 1, 1, NULL));
  name_free(name);
  data_free(op);
  return parser;  
}

parser_t * script_parse_emit_infix_op(parser_t *parser) {
  data_t *op = datastack_pop(parser -> stack);
  name_t *name = name_create(0);
  
  name_extend_data(name, op);
  script_push_instruction(parser -> data,
    instruction_create_function(name, 1, 2, NULL));
  name_free(name);
  data_free(op);
  return parser;
}

parser_t * script_parse_jump(parser_t *parser, data_t *label) {
  script_t *script;

  script = parser -> data;
  if (parser_debug) {
    debug(" -- label: %s", data_debugstr(label));
  }
  script_push_instruction(script, instruction_create_jump(data_tostring(label)));
  return parser;
}

parser_t * script_parse_emit_func_call(parser_t *parser) {
  script_t      *script;
  data_t        *func_name;
  int            arg_count;
  array_t       *kwargs;
  instruction_t *instr;

  script = parser -> data;
  kwargs = datastack_rollup(parser -> stack);
  arg_count = datastack_count(parser -> stack);
  if (parser_debug) {
    debug(" -- arg_count: %d", arg_count);
  }
  func_name = datastack_pop(parser -> stack);
  instr = instruction_create_function(data_nameval(func_name), 0, arg_count, kwargs);
  script_push_instruction(script, instr);
  data_free(func_name);
  return parser;
}

parser_t * script_parse_import(parser_t *parser) {
  script_t *script;
  data_t   *module;

  script = parser -> data;
  module = datastack_pop(parser -> stack);
  script_push_instruction(script,
			  instruction_create_import(data_nameval(module)));
  data_free(module);
  return parser;
}

parser_t * script_parse_emit_test(parser_t *parser) {
  script_t      *script;
  instruction_t *test;
  char           label[9];

  script = parser -> data;
  strrand(label, 8);
  test = instruction_create_test(label);
  datastack_push_string(parser -> stack, label);
  script_push_instruction(script, test);
  return parser;
}

parser_t * script_parse_emit_jump(parser_t *parser) {
  script_t      *script;
  instruction_t *jump;
  char           label[9];

  script = parser -> data;
  strrand(label, 8);
  jump = instruction_create_jump(label);
  datastack_push_string(parser -> stack, jump -> name);
  script_push_instruction(script, jump);
  return parser;
}

parser_t * script_parse_emit_pop(parser_t *parser) {
  script_t      *script;

  script = parser -> data;
  script_push_instruction(script, instruction_create_pop());
  return parser;
}

parser_t * script_parse_emit_nop(parser_t *parser) {
  script_t      *script;

  script = parser -> data;
  script_push_instruction(script, instruction_create_nop());
  return parser;
}

parser_t * script_parse_push_label(parser_t *parser) {
  script_t      *script;

  script = parser -> data;
  script -> label = new(9);
  strrand(script -> label, 8);
  datastack_push_string(parser -> stack, script -> label);
  return parser;
}

parser_t * script_parse_emit_else(parser_t *parser) {
  script_t      *script;
  instruction_t *jump;
  data_t        *label;
  char           newlabel[9];

  script = parser -> data;
  label = datastack_pop(parser -> stack);
  if (parser_debug) {
    debug(" -- label: %s", data_debugstr(label));
  }
  script -> label = strdup(data_charval(label));
  data_free(label);
  strrand(newlabel, 8);
  jump = instruction_create_jump(newlabel);
  datastack_push_string(parser -> stack, newlabel);
  script_push_instruction(script, jump);
  return parser;
}

parser_t * script_parse_emit_end(parser_t *parser) {
  script_t      *script;
  data_t        *label;

  script = parser -> data;
  label = datastack_pop(parser -> stack);
  if (parser_debug) {
    debug(" -- label: %s", data_debugstr(label));
  }
  script -> label = strdup(data_charval(label));
  data_free(label);
  return parser;
}

parser_t * script_parse_emit_end_while(parser_t *parser) {
  script_t      *script;
  data_t        *label;
  instruction_t *jump;
  char          *block_label;

  script = parser -> data;

  /*
   * First label: The one pushed at the end of the expression. This is the
   * label to be set at the end of the loop:
   */
  label = datastack_pop(parser -> stack);
  if (parser_debug) {
    debug(" -- end block label: %s", data_debugstr(label));
  }
  block_label = strdup(data_charval(label));
  data_free(label);

  /*
   * Second label: The one pushed after the while statement. This is the one
   * we have to jump back to:
   */
  label = datastack_pop(parser -> stack);
  if (parser_debug) {
    debug(" -- jump back label: %s", data_debugstr(label));
  }
  jump = instruction_create_jump(data_charval(label));
  script_push_instruction(script, jump);
  data_free(label);

  script -> label = block_label;

  return parser;
}

parser_t * script_parse_start_function(parser_t *parser) {
  script_t  *up;
  script_t  *func;
  data_t    *data;
  char      *fname;
  data_t    *params;
  int        ix;
  int        async;

  up = (script_t *) parser -> data;

  /* Top of stack: Parameter names as List */
  params = datastack_pop(parser -> stack);

  /* Next on stack: function name */
  data = datastack_pop(parser -> stack);
  fname = strdup(data_tostring(data));
  data_free(data);
  
  /* sync/async flag */
  data = datastack_pop(parser -> stack);
  async = data_intval(data);
  data_free(data);

  func = script_create(NULL, up, fname);
  func -> async = async;
  func -> params = str_array_create(array_size(data_arrayval(params)));
  array_reduce(data_arrayval(params),
	       (reduce_t) data_add_strings_reducer, 
	       func -> params);
  free(fname);
  data_free(params);
  if (parser_debug) {
    debug(" -- defining function %s", func -> name);
  }
  parser -> data = func;
  return parser;
}

parser_t * script_parse_end_function(parser_t *parser) {
  script_t  *func;

  func = (script_t *) parser -> data;
  _script_parse_emit_epilog(parser);
  parser -> data = func -> up;
  return parser;
}

parser_t * script_parse_native_function(parser_t *parser) {
  script_t     *script;
  native_fnc_t *func;
  data_t       *data;
  char         *fname;
  data_t       *params;
  name_t       *lib_func;
  native_t      c_func;
  parser_t     *ret = parser;
  int           async;
  
  script = (script_t *) parser -> data;

  /* Top of stack: Parameter names as List */
  params = datastack_pop(parser -> stack);

  /* Next on stack: function name */
  data = datastack_pop(parser -> stack);
  fname = strdup(data_tostring(data));
  data_free(data);

  /* sync/async flag */
  data = datastack_pop(parser -> stack);
  async = data_intval(data);
  data_free(data);

  lib_func = name_split(parser -> last_token -> token, ":");
  if (name_size(lib_func) > 2 || name_size(lib_func) == 0) {
    ret = NULL;
  } else {
    if (name_size(lib_func) == 2) {
      if (parser_debug) {
        debug("Library: %s", name_first(lib_func));
      }
      if (!resolve_library(name_first(lib_func))) {
        error("Error loading library '%s': %s", name_first(lib_func), strerror(errno));
      }
      /* TODO Error handling */
    }
    if (parser_debug) {
      debug("C Function: %s", name_last(lib_func));
    }
    c_func = (native_t) resolve_function(name_last(lib_func));
    if (c_func) {
      func = native_fnc_create(script, fname, c_func);
      func -> params = str_array_create(array_size(data_arrayval(params)));
      func -> async = async;
      array_reduce(data_arrayval(params),
		   (reduce_t) data_add_strings_reducer, 
		   func -> params);
      if (parser_debug) {
	debug(" -- defined native function %s", name_tostring(func -> name));
      }
    } else {
      error("C function '%s' not found", c_func);
      /* FIXME error handling
	 return data_error(ErrorName,
	 "Could not find native function '%s'", fname);
      */
      ret = NULL;
    }
  }
  free(fname);
  data_free(params);
  name_free(lib_func);
  return ret;
}