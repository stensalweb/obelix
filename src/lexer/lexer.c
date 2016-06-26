/*
 * lexer.c - Copyright (c) 2014 Jan de Visser <jan@finiandarcy.com>
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

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <array.h>
#include <data.h>
#include <function.h>
#include <lexer.h>

#define LEXER_DEBUG
#ifdef LEXER_DEBUG
  #define debuglexer(fmt, args...)    if (lexer_debug) { debug(fmt, ## args); }
#else
  #define debuglexer(fmt, args...)
#endif

typedef lexer_t *    (*newline_fnc)(lexer_t *, int);

typedef struct _tokenize_ctx {
  data_t  *parser;
  array_t *args;
} tokenize_ctx_t;

static inline void      _lexer_init(void);
static void             _dequotify(str_t *);

static token_t *        _lexer_match_token(lexer_t *);
static lexer_t *        _lexer_on_newline(lexer_t *, int);

static void             _lexer_free(lexer_t *);
static char *           _lexer_allocstring(lexer_t *);
static data_t *         _lexer_resolve(lexer_t *, char *);
static data_t *         _lexer_has_next(lexer_t *);
static data_t *         _lexer_next(lexer_t *);
//static data_t *       _lexer_set(lexer_t *, char *);
static data_t *         _lexer_mth_rollup(lexer_t *, char *, array_t *, dict_t *);
static tokenize_ctx_t * _lexer_tokenize_reducer(token_t *, tokenize_ctx_t *);
static data_t *         _lexer_mth_tokenize(lexer_t *, char *, array_t *, dict_t *);

static code_label_t lexer_state_names[] = {
  { .code = LexerStateNoState,            .label = "LexerStateNoState" },
  { .code = LexerStateFresh,              .label = "LexerStateFresh" },
  { .code = LexerStateParameter,          .label = "LexerStateParameter" },
  { .code = LexerStateInit,               .label = "LexerStateInit" },
  { .code - LexerStateSecondPass,         .label = "LexerStateSecondPass" },
  { .code = LexerStateSuccess,            .label = "LexerStateSuccess" },
  { .code = LexerStateWhitespace,         .label = "LexerStateWhitespace" },
  { .code = LexerStateNewLine,            .label = "LexerStateNewLine" },
  { .code = LexerStateIdentifier,         .label = "LexerStateIdentifier" },
  { .code = LexerStateURIComponent,       .label = "LexerStateURIComponent" },
  { .code = LexerStateURIComponentPercent,.label = "LexerStateURIComponentPercent" },
  { .code = LexerStateKeyword,            .label = "LexerStateKeyword" },
  { .code = LexerStateMatchLost,          .label = "LexerStateMatchLost" },
  { .code = LexerStatePlusMinus,          .label = "LexerStatePlusMinus" },
  { .code = LexerStateZero,               .label = "LexerStateZero" },
  { .code = LexerStateNumber,             .label = "LexerStateNumber" },
  { .code = LexerStateDecimalInteger,     .label = "LexerStateDecimalInteger" },
  { .code = LexerStateHexInteger,         .label = "LexerStateHexInteger" },
  { .code = LexerStateFloat,              .label = "LexerStateFloat" },
  { .code = LexerStateSciFloat,           .label = "LexerStateSciFloat" },
  { .code = LexerStateQuotedStr,          .label = "LexerStateQuotedStr" },
  { .code = LexerStateQuotedStrEscape,    .label = "LexerStateQuotedStrEscape" },
  { .code = LexerStateHashPling,          .label = "LexerStateHashPling" },
  { .code = LexerStateSlash,              .label = "LexerStateSlash" },
  { .code = LexerStateBlockComment,       .label = "LexerStateBlockComment" },
  { .code = LexerStateLineComment,        .label = "LexerStateLineComment" },
  { .code = LexerStateStar,               .label = "LexerStateStar" },
  { .code = LexerStateDone,               .label = "LexerStateDone" },
  { .code = LexerStateStale,              .label = "LexerStateStale" },
  { .code = -1,                           .label = NULL }
};

static code_label_t lexer_option_labels[] = {
  { .code = LexerOptionIgnoreWhitespace,    .label = "LexerOptionIgnoreWhitespace" },
  { .code = LexerOptionIgnoreNewLines,      .label = "LexerOptionIgnoreNewLines" },
  { .code = LexerOptionIgnoreAllWhitespace, .label = "LexerOptionIgnoreAllWhitespace" },
  { .code = LexerOptionCaseSensitive,       .label = "LexerOptionCaseSensitive" },
  { .code = LexerOptionHashPling,           .label = "LexerOptionHashPling" },
  { .code = LexerOptionSignedNumbers,       .label = "LexerOptionSignedNumbers" },
  { .code = LexerOptionOnNewLine,           .label = "LexerOptionOnNewLine" },
  { .code = LexerOptionBufferSize,          .label = "LexerOptionBufferSize" },
  { .code = LexerOptionLAST,                .label = NULL }
};

int lexer_debug = 0;
int Lexer = -1;

/* ------------------------------------------------------------------------ */

static void             _lexer_free(lexer_t *);
static char *           _lexer_allocstring(lexer_t *);
static data_t *         _lexer_resolve(lexer_t *, char *);
static data_t *         _lexer_call(lexer_t *, array_t *, dict_t *);
static data_t *         _lexer_has_next(lexer_t *);
static data_t *         _lexer_next(lexer_t *);
//static data_t *       _lexer_set(lexer_t *, char *);
static data_t *         _lexer_mth_rollup(lexer_t *, char *, array_t *, dict_t *);
static tokenize_ctx_t * _lexer_tokenize_reducer(token_t *, tokenize_ctx_t *);
static data_t *         _lexer_mth_tokenize(lexer_t *, char *, array_t *, dict_t *);

static vtable_t _vtable_lexer[] = {
  { .id = FunctionFree,        .fnc = (void_t) _lexer_free },
  { .id = FunctionAllocString, .fnc = (void_t) _lexer_allocstring },
  { .id = FunctionCall,        .fnc = (void_t) _lexer_call },
  { .id = FunctionResolve,     .fnc = (void_t) _lexer_resolve },
  { .id = FunctionNext,        .fnc = (void_t) _lexer_next },
  { .id = FunctionHasNext,     .fnc = (void_t) _lexer_has_next },
  { .id = FunctionNone,        .fnc = NULL }
};

static methoddescr_t _methoddescr_lexer[] = {
  { .type = -1,     .name = "tokenize", .method = (method_t) _lexer_mth_tokenize, .argtypes = { Int, NoType, NoType },    .minargs = 1, .varargs = 0 },
  { .type = -1,     .name = "rollup",   .method = (method_t) _lexer_mth_rollup,   .argtypes = { Int, NoType, NoType },    .minargs = 1, .varargs = 0 },
  { .type = NoType, .name = NULL,       .method = NULL,                .argtypes = { NoType, NoType, NoType }, .minargs = 0, .varargs = 0 }
};

/* ------------------------------------------------------------------------ */

void _lexer_init(void) {
  if (Lexer < 0) {
    logging_register_category("lexer", &lexer_debug);
    Lexer = typedescr_create_and_register(Lexer,
                                          "lexer",
                                          _vtable_lexer,
                                          _methoddescr_lexer);
  }
}

/*
 * public utility functions
 */

char * _state_name(code_label_t *str_table, int state) {
  char *ret = label_for_code(str_table, state);

  if (!ret) {
    ret = "[Unknown state]";
  }
  return ret;
}

char * lexer_state_name(lexer_state_t state) {
  return label_for_code(lexer_state_names, state);
}

/* -- L E X E R  D A T A  F U N C T I O N S ------------------------------ */

void _lexer_free(lexer_t *lexer) {
  scanner_t *scanner;

  if (lexer) {
    token_free(lexer -> last_token);
    for (scanner = lexer -> scanners; scanner; scanner = scanner -> next) {
      if (scanner -> next) {
        scanner -> next -> prev = NULL;
      }
      lexer -> scanners = scanner -> next;
      scanner_free(scanner);
    }
    str_free(lexer -> token);
    str_free(lexer -> pushed_back);
    if ((data_t *) lexer -> buffer != lexer -> reader) {
      str_free(lexer -> buffer);
    }
    str_free(lexer -> error);
  }
}

char * _lexer_allocstring(lexer_t *lexer) {
  char *buf;

  asprintf(&buf, "Lexer for '%s'",
           data_tostring(lexer -> reader));
  return buf;
}

data_t * _lexer_resolve(lexer_t *lexer, char *name) {
  if (!strcmp(name, "reader")) {
    return data_copy(lexer -> reader);
  } else if (!strcmp(name, "statename")) {
    return str_to_data(lexer_state_name(lexer -> state));
  } else if (!strcmp(name, "state")) {
    return int_to_data(lexer -> state);
  } else if (!strcmp(name, "line")) {
    return int_to_data(lexer -> line);
  } else if (!strcmp(name, "column")) {
    return int_to_data(lexer -> column);
  } else {
    return NULL;
  }
}

data_t * _lexer_has_next(lexer_t *lexer) {
  return int_to_data(lexer_next_token(lexer) ? TRUE : FALSE);
}

data_t * _lexer_next(lexer_t *lexer) {
  return (data_t *) token_copy(lexer -> last_token);
}

data_t * _lexer_mth_rollup(lexer_t *lexer, char *n, array_t *args, dict_t *kwargs) {
  return (data_t *) lexer_rollup_to(lexer, data_intval(data_array_get(args, 0)));
}

tokenize_ctx_t * _lexer_tokenize_reducer(token_t *token, tokenize_ctx_t *ctx) {
  data_t *d;

  array_set(ctx -> args, 0, (data_t *) token_copy(token));
  d = data_call(ctx -> parser, ctx -> args, NULL);
  if (d != data_array_get(ctx -> args, 0)) {
    array_set(ctx -> args, 0, d);
  }
  if (!data_cmp(d, data_null()) || ((data_type(d) == Bool) && !data_intval(d))) {
    return NULL;
  } else {
    return ctx;
  }
}

data_t * _lexer_mth_tokenize(lexer_t *lexer, char *n, array_t *args, dict_t *kwargs) {
  tokenize_ctx_t  ctx;
  data_t         *ret;

  ctx.parser = data_copy(data_array_get(args, 0));
  ctx.args = data_array_create(3);

  array_set(ctx.args, 0, NULL);
  array_set(ctx.args, 1, data_copy(data_array_get(args, 1)));
  array_set(ctx.args, 2, (data_t *) lexer_copy( lexer));
  lexer_tokenize(lexer, (reduce_t) _lexer_tokenize_reducer, (void *) &ctx);
  ret = data_copy(data_array_get(ctx.args, 0));
  array_free(ctx.args);
  data_free(ctx.parser);
  return ret;
}

/* -- L E X E R  S T A T I C  F U N C T I O N S --------------------------- */

lexer_t * _lexer_init_buffer(lexer_t *lexer) {
  if (data_is_string(lexer -> reader)) {
    lexer -> buffer = data_as_string(lexer -> reader);
  } else {
    lexer -> buffer = str_create(lexer_config_get_bufsize(lexer -> config));
    if (!lexer -> buffer) {
      return NULL;
    }
  }
  return lexer;
}

void _lexer_update_location(lexer_t *lexer, int ch) {
  if (strchr("\r\n", ch)) {
    if (!strchr("\r\n", lexer -> prev_char) || (ch != lexer -> prev_char)) {
      lexer -> line++;
      lexer -> column = 0;
    }
  } else {
     lexer -> column++;
  }
}

token_t * _lexer_match_token(lexer_t *lexer) {
  token_t   *ret;
  scanner_t *scanner;

  ret = NULL;
  for (scanner = lexer -> scanners; scanner && !ret; scanner = scanner -> next) {
    if (scanner -> config -> def -> funcs.match) {
      lexer -> state = LexerStateFirstPass;
      lexer_rewind(lexer);
      if (scanner -> config -> def -> funcs.match) {
	ret = scanner -> config -> def -> funcs.match(scanner -> data, lexer);
      }
    }
  }
  if (!ret) {
    for (scanner = lexer -> scanners; scanner && !ret; scanner = scanner -> next) {
      if (scanner -> config -> def -> funcs.match) {
        lexer_rewind(lexer);
        lexer -> state = LexerStateSecondPass;
        ret = scanner -> config -> def -> funcs.match(scanner);
	if (scanner -> config -> def -> funcs.match_2nd_pass) {
	  ret = scanner -> config -> def -> funcs.match_2nd_pass(scanner -> data, lexer);
	}
      }
    }
  }

  if (ret) {
    ret -> line = lexer -> line;
    ret -> column = lexer -> column;
    lexer -> state = LexerStateSuccess;
    debuglexer("_lexer_match_token out - state: %s token: %s",
               lexer_state_name(lexer -> state), token_code_name(ret -> code));
  } else {
    if (str_len(lexer -> token)) {
      /*
       * No scanner wanted this guy...
       */
      lexer_reset(lexer);
      lexer -> state = LexerStateInit;
    } else {
      lexer -> state = LexerStateDone;
    }
    debuglexer("_lexer_match_token out - state: %s", lexer_state_name(lexer -> state));
  }
  return ret;
}

lexer_t * _lexer_on_newline(lexer_t *lexer, int line) {
  data_t      *on_newline_opt;
  function_t  *fnc;
  newline_fnc  on_newline;

  on_newline_opt = lexer_get_option(lexer, LexerOptionOnNewLine);
  if (data_is_function(on_newline_opt)) {
    fnc = (function_t *) on_newline_opt;
    on_newline = (newline_fnc) fnc -> fnc;
    return on_newline(lexer, line);
  } else {
    return lexer;
  }
}

/* -- L E X E R  P U B L I C  I N T E R F A C E --------------------------- */

lexer_t * lexer_create(lexer_config_t *config, data_t *reader) {
  lexer_t          *ret;
  scanner_t        *scanner;
  scanner_t        *prev = NULL;
  scanner_config_t *scanner_config;
  int      ix;

  _lexer_init();
  ret = data_new(Lexer, lexer_t);
  ret -> config = lexer_config_copy(config);
  ret -> reader = data_copy(reader);

  ret -> buffer = NULL;
  ret -> state = LexerStateFresh;
  ret -> last_token = NULL;
  ret -> line = 1;
  ret -> column = 0;
  ret -> prev_char = 0;
  ret -> token = str_create(LEXER_INIT_TOKEN_SZ);
  ret -> lookahead = NULL;
  ret -> lookahead_live = FALSE;
  ret -> error = NULL;

  ret -> scanners = NULL;
  for (scanner_config = config -> scanners; scanner_config; scanner_config = scanner_config -> next) {
    scanner = scanner_config_instantiate(scanner_config, ret);
    if (!prev) {
      ret -> scanners = scanner;
    } else {
      prev -> next = scanner;
    }
    prev = scanner;
  }
  return ret;
}

void * _lexer_tokenize(lexer_t *lexer, reduce_t parser, void *data) {
  while (lexer_next_token(lexer)) {
    data = parser(lexer -> last_token, data);
    if (!data) {
      break;
    }
  }
  return data;
}

token_t * lexer_next_token(lexer_t *lexer) {
  if (!lexer -> last_token) {
    _lexer_on_newline(lexer, 1);
  } else if (token_code(lexer -> last_token) == TokenCodeEnd) {
    token_free(lexer -> last_token);
    lexer -> last_token = token_create(TokenCodeExhausted, "$$$$");
    return NULL;
  } else if (token_code(lexer -> last_token) == TokenCodeExhausted) {
    return NULL;
  } else {
    token_free(lexer -> last_token);
  }

  do {
    lexer_reset(lexer);

    do {
      lexer -> last_token = _lexer_match_token(lexer);
    } while ((lexer -> state != LexerStateDone) &&
             (lexer -> state != LexerStateSuccess));
    if (lexer -> last_token) {
      debuglexer("lexer_next_token: matched token: %s [%s]",
                 token_code_name(lexer -> last_token -> code),
                 lexer -> last_token -> token);
    }
    if (!lexer -> last_token && (lexer -> state == LexerStateDone)) {
      lexer -> last_token = token_create(TokenCodeEnd, "$$");
      lexer -> last_token -> line = lexer -> line;
      lexer -> last_token -> column = lexer -> column;
    } else if (lexer -> last_token && (token_code(lexer -> last_token) == TokenCodeNewLine)) {
      _lexer_on_newline(lexer, lexer -> last_token -> line);
    }
  } while (!lexer -> last_token);

  debuglexer("lexer_next_token out: token: %s [%s], state %s",
              token_code_name(token_code(lexer -> last_token)),
              token_token(lexer -> last_token),
              lexer_state_name(lexer -> state));
  return token_copy(lexer -> last_token);
}

lexer_t * lexer_rewind(lexer_t *lexer) {
  lexer_push(lexer);
  str_erase(lexer -> token);
  str_rewind(lexer -> lookahead);
  lexer -> lookahead_live = str_len(lexer -> lookahead);
  return lexer;
}

lexer_t * lexer_reset(lexer_t *lexer) {
  if (lexer -> last_token) {
    token_free(lexer -> last_token);
    lexer -> last_token = NULL;
  }
  lexer -> state = LexerStateInit;
  lexer_push(lexer);
  if (lexer -> lookahead_live) {
    str_chop(lexer -> lookahead, str_len(lexer -> token));
    lexer -> lookahead_live = str_len(lexer -> lookahead);
  } else {
    str_erase(lexer -> lookahead);
  }
  str_erase(lexer -> token);
  return lexer;
}

token_t * lexer_accept(lexer_t *lexer, token_code_t code) {
  token_t *ret;

  ret = token_create(code, (lexer -> token) ? str_chars(lexer -> token) : NULL);
  lexer_reset(lexer);
  lexer -> last_token = token_copy(ret);
  return lexer -> last_token;
}

/**
 * Rewinds the buffer and reads <code>num</code> characters from the buffer,
 * returning the read string as a token with code <code>code</code>.
 */
token_t * lexer_get_accept(lexer_t *lexer, token_code_t code, int num) {
  int i;

  lexer_rewind(lexer);
  for (i = 0; i < num; i++) {
    lexer_get_char(lexer);
  }
  return lexer_accept(lexer, code);
}

lexer_t * lexer_push(lexer_t *lexer) {
  if (lexer -> current) {
    str_append_char(lexer -> token, lexer -> current);
    if (!lexer -> lookahead_live) {
      if (!lexer -> lookahead) {
        lexer -> lookahead = str_create(LEXER_INIT_TOKEN_SZ);
      }
      str_append_char(lexer -> lookahead, lexer -> current);
    }
  }
  lexer -> prev_char = lexer -> current;
  lexer -> current = 0;
  return lexer;
}

int lexer_get_char(lexer_t *lexer) {
  int   ch;
  int   ret;

  if (lexer -> lookahead_live) {
    ch = str_readchar(lexer -> lookahead);
    if (ch > 0) {
      debuglexer("_lexer_get_char: read '%c' from lookahead buffer", ch);
      lexer -> current = ch;
      return ch;
    } else {
      debuglexer("_lexer_get_char: lookahead buffer exhausted");
      lexer -> lookahead_live = FALSE;
    }
  }

  if (!lexer -> buffer) {
    if (!_lexer_init_buffer(lexer)) {
      return 0;
    }
    ret = str_readinto(lexer -> buffer, lexer -> reader);
    debuglexer("_lexer_get_char: Created buffer and did initial read: %d", ret);
    if (ret <= 0) {
      return 0;
    }
  }
  ch = str_readchar(lexer -> buffer);
  debuglexer("_lexer_get_char: Read character: '%c'", (ch > 0) ? ch : '$');
  if (ch <= 0 && (lexer -> reader != (data_t *) lexer -> buffer)) {
    ret = str_readinto(lexer -> buffer, lexer -> reader);
    debuglexer("_lexer_get_char: Read new chunk into buffer: %d", ret);
    if (ret <= 0) {
      return 0;
    } else {
      ch = str_readchar(lexer -> buffer);
      debuglexer("_lexer_get_char: Read character from new chunk: '%c'", (ch > 0) ? ch : '$');
    }
  }
  _lexer_update_location(lexer, ch); // FIXME needs to be moved?
  return ch;
}

token_t * lexer_rollup_to(lexer_t *lexer, int marker) {
  token_t *ret;
  str_t   *str;
  int      ch;
  char    *msgbuf;

  str = str_create(10);
  for (ch = lexer_get_char(lexer);
       ch && (ch != marker);
       ch = lexer_get_char(lexer)) {
    if (ch == '\\') {
      ch = lexer_get_char(lexer);
      if (!ch) {
        break;
      }
    }
    str_append_char(str, ch);
  }
  if (ch == marker) {
    ret = token_create(TokenCodeRawString, str_chars(str));
  } else {
    asprintf(&msgbuf, "Unterminated '%c'", marker);
    ret = token_create(TokenCodeError, msgbuf);
  }
  str_free(str);
  return ret;
}
