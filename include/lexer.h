/*
 * /obelix/src/lexer.h - Copyright (c) 2014 Jan de Visser <jan@finiandarcy.com>
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


#ifndef __LEXER_H__
#define __LEXER_H__

#include <array.h>
#include <data.h>
#include <dict.h>
#include <logging.h>
#include <str.h>

#ifndef OBLLEXER_IMPEXP
  #define OBLLEXER_IMPEXP	__DLL_IMPORT__
#endif /* OBLLEXER_IMPEXP */

#include <token.h>

#define LEXER_BUFSIZE             16384
#define LEXER_INIT_TOKEN_SZ       256
#define LEXER_MAX_SCANNERS        32
#define SCANNER_CONFIG_SEPARATORS ",.;"
#define PARAM_PRIORITY            "priority"
#define PARAM_CONFIGURATION       "configuration"

typedef enum _lexer_where {
  LexerWhereBegin,
  LexerWhereMiddle,
  LexerWhereEnd
} lexer_where_t;

typedef enum _lexer_state {
  LexerStateNoState,
  LexerStateFresh,
  LexerStateInit,
  LexerStateSuccess,
  LexerStateDone,
  LexerStateStale,
  LexerStateLAST
} lexer_state_t;

struct _lexer;
struct _lexer_config;
struct _scanner;
struct _scanner_config;

typedef token_t * (*matcher_t)(struct _scanner *);

typedef struct _scanner_config {
  data_t                   _d;
  int                      priority;
  struct _scanner_config  *prev;
  struct _scanner_config  *next;
  struct _lexer_config    *lexer_config;
  matcher_t                match;
  matcher_t                match_2nd_pass;
  dict_t                  *config;
} scanner_config_t;

typedef struct _scanner {
  data_t            _d;
  struct _scanner  *next;
  struct _scanner  *prev;
  scanner_config_t *config;
  struct _lexer    *lexer;
  int               state;
  void             *data;
} scanner_t;

typedef struct _lexer_config {
  data_t            _d;
  int               num_scanners;
  scanner_config_t *scanners;
  size_t            bufsize;
  char             *build_func;
  data_t           *data;
} lexer_config_t;

typedef struct _lexer {
  data_t           _d;
  lexer_config_t  *config;
  scanner_t       *scanners;
  data_t          *reader;
  str_t           *buffer;
  str_t           *token;
  lexer_state_t    state;
  lexer_where_t    where;
  token_t         *last_token;
  int              scanned;
  int              count;
  int              scan_count;
  int              current;
  int              prev_char;
  int              line;
  int              column;
  void            *data;
} lexer_t;

OBLLEXER_IMPEXP char *             lexer_state_name(lexer_state_t);

/*
 * ---------------------------------------------------------------------------
 * S C A N N E R  C O N F I G
 * ---------------------------------------------------------------------------
 */

OBLLEXER_IMPEXP int                scanner_config_typeid(void);
OBLLEXER_IMPEXP typedescr_t *      scanner_config_register(typedescr_t *);
OBLLEXER_IMPEXP typedescr_t *      scanner_config_load(char *, char *);
OBLLEXER_IMPEXP typedescr_t *      scanner_config_get(char *);
OBLLEXER_IMPEXP scanner_config_t * scanner_config_create(char *, lexer_config_t *);
OBLLEXER_IMPEXP scanner_t *        scanner_config_instantiate(scanner_config_t *, lexer_t *);
OBLLEXER_IMPEXP scanner_config_t * scanner_config_setvalue(scanner_config_t *, char *, data_t *);
OBLLEXER_IMPEXP scanner_config_t * scanner_config_configure(scanner_config_t *, data_t *);
OBLLEXER_IMPEXP scanner_config_t * scanner_config_dump(scanner_config_t *);

/*
 * ---------------------------------------------------------------------------
 * S C A N N E R
 * ---------------------------------------------------------------------------
 */

OBLLEXER_IMPEXP scanner_t *        scanner_create(scanner_config_t *, lexer_t *);
// OBLLEXER_IMPEXP token_t *          scanner_match(scanner_t *);
OBLLEXER_IMPEXP scanner_t *        scanner_reconfigure(scanner_t *, char *, data_t *);

/*
 * ---------------------------------------------------------------------------
 * L E X E R  C O N F I G
 * ---------------------------------------------------------------------------
 */

OBLLEXER_IMPEXP lexer_config_t *   lexer_config_create(void);
OBLLEXER_IMPEXP scanner_config_t * lexer_config_add_scanner(lexer_config_t *, char *);
OBLLEXER_IMPEXP scanner_config_t * lexer_config_get_scanner(lexer_config_t *, char *);
OBLLEXER_IMPEXP lexer_config_t *   lexer_config_set_bufsize(lexer_config_t *, size_t);
OBLLEXER_IMPEXP size_t             lexer_config_get_bufsize(lexer_config_t *);
OBLLEXER_IMPEXP data_t *           lexer_config_set(lexer_config_t *, char *, data_t *);
OBLLEXER_IMPEXP data_t *           lexer_config_get(lexer_config_t *, char *, char *);
OBLLEXER_IMPEXP lexer_config_t *   lexer_config_tokenize(lexer_config_t *, reduce_t, data_t *);
OBLLEXER_IMPEXP lexer_config_t *   lexer_config_dump(lexer_config_t *);

/*
 * ---------------------------------------------------------------------------
 * L E X E R
 * ---------------------------------------------------------------------------
 */

OBLLEXER_IMPEXP lexer_t *   lexer_create(lexer_config_t *, data_t *);
OBLLEXER_IMPEXP token_t *   lexer_next_token(lexer_t *);
OBLLEXER_IMPEXP int         lexer_get_char(lexer_t *);
OBLLEXER_IMPEXP int         lexer_at_top(lexer_t *);
OBLLEXER_IMPEXP int         lexer_at_end(lexer_t *);
OBLLEXER_IMPEXP void        lexer_pushback(lexer_t *);
OBLLEXER_IMPEXP void        lexer_clear(lexer_t *);
OBLLEXER_IMPEXP void        lexer_flush(lexer_t *);
OBLLEXER_IMPEXP lexer_t *   lexer_reset(lexer_t *);
OBLLEXER_IMPEXP lexer_t *   lexer_rewind(lexer_t *);
OBLLEXER_IMPEXP token_t *   lexer_accept(lexer_t *, token_code_t);
OBLLEXER_IMPEXP token_t *   lexer_accept_token(lexer_t *, token_t *);
OBLLEXER_IMPEXP void        lexer_skip(lexer_t *);
OBLLEXER_IMPEXP token_t *   lexer_get_accept(lexer_t *, token_code_t, int);
OBLLEXER_IMPEXP lexer_t *   lexer_push(lexer_t *);
OBLLEXER_IMPEXP lexer_t *   lexer_push_as(lexer_t *, int);
OBLLEXER_IMPEXP lexer_t *   lexer_discard(lexer_t *);
OBLLEXER_IMPEXP void *      _lexer_tokenize(lexer_t *, reduce_t, void *);
OBLLEXER_IMPEXP scanner_t * lexer_get_scanner(lexer_t *, char *);
OBLLEXER_IMPEXP lexer_t *   lexer_reconfigure_scanner(lexer_t *, char *, char *, data_t *);

OBLLEXER_IMPEXP void        lexer_init(void);

OBLLEXER_IMPEXP int LexerConfig;
OBLLEXER_IMPEXP int Lexer;
OBLLEXER_IMPEXP int ScannerConfig;
OBLLEXER_IMPEXP int Scanner;
OBLLEXER_IMPEXP int lexer_debug;

type_skel(lexer_config, LexerConfig, lexer_config_t);
type_skel(lexer, Lexer, lexer_t);
type_skel(scanner_config, ScannerConfig, scanner_config_t);
type_skel(scanner, Scanner, scanner_t);

static inline void * lexer_tokenize(lexer_t *lexer, void *reducer, void *ctx) {
  return _lexer_tokenize(lexer, (reduce_t) reducer, ctx);
}

#endif /* __LEXER_H__ */
