/*
 * /obelix/src/grammarparser.c - Copyright (c) 2014 Jan de Visser <jan@finiandarcy.com>
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

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <grammarparser.h>
#include <list.h>

extern int grammar_debug;

typedef struct _gp_state_rec {
  char     *name;
  reduce_t  handler;
} gp_state_rec_t;

static grammar_parser_t * _grammar_parser_state_start(token_t *, grammar_parser_t *);
static grammar_parser_t * _grammar_parser_state_options(token_t *, grammar_parser_t *);
static grammar_parser_t * _grammar_parser_state_option_name(token_t *, grammar_parser_t *);
static grammar_parser_t * _grammar_parser_state_header(token_t *, grammar_parser_t *);
static grammar_parser_t * _grammar_parser_state_nonterminal(token_t *, grammar_parser_t *);
static grammar_parser_t * _grammar_parser_state_rule(token_t *, grammar_parser_t *);
static grammar_parser_t * _grammar_parser_state_entry(token_t *, grammar_parser_t *);
static void               _grammar_parser_syntax_error(grammar_parser_t *, char *, ...);

static gp_state_rec_t _gp_state_recs[] = {
    { "GPStateStart",       (reduce_t) _grammar_parser_state_start },
    { "GPStateOptions",     (reduce_t) _grammar_parser_state_options },
    { "GPStateOptionName",  (reduce_t) _grammar_parser_state_option_name },
    { "GPStateHeader",      (reduce_t) _grammar_parser_state_header },
    { "GPStateNonTerminal", (reduce_t) _grammar_parser_state_nonterminal },
    { "GPStateRule",        (reduce_t) _grammar_parser_state_rule },
    { "GPStateEntry",       (reduce_t) _grammar_parser_state_entry }
};


/*
 * grammar_parser_t static functions
 */

grammar_parser_t * _grammar_parser_state_start(token_t *token, grammar_parser_t *grammar_parser) {
  int        code;
  char      *str, *state_str;
  char       terminal_str[2];
  grammar_t *g;
  gp_state_t state;

  g = grammar_parser -> grammar;
  state = grammar_parser -> state;
  code = token_code(token);
  str = token_token(token);
  switch (code) {
    case TokenCodeIdentifier:
      grammar_parser -> state = GPStateNonTerminal;
      grammar_parser -> nonterminal = nonterminal_create(grammar_parser -> grammar, str);
      grammar_parser -> rule = NULL;
      grammar_parser -> entry = NULL;
      grammar_parser -> ge = grammar_parser -> nonterminal -> ge;
      break;

    case TokenCodePercent:
      grammar_parser -> old_state = GPStateStart;
      grammar_parser -> state = GPStateOptions;
      grammar_parser -> ge = g -> ge;
      break;

    case TokenCodeOpenBracket:
      grammar_parser -> old_state = GPStateNonTerminal;
      grammar_parser -> state = GPStateOptions;
      break;

    default:
      _grammar_parser_syntax_error(
          grammar_parser,
          "Unexpected token '%s' at start of grammar text",
          token_tostring(token));
  }
  return grammar_parser;
}

grammar_parser_t * _grammar_parser_state_options(token_t *token, grammar_parser_t *grammar_parser) {
  int        code;
  char      *str, *state_str;
  char       terminal_str[2];
  grammar_t *g;
  gp_state_t state;

  g = grammar_parser -> grammar;
  state = grammar_parser -> state;
  code = token_code(token);
  str = token_token(token);

  switch (code) {
    case TokenCodeIdentifier:
      grammar_parser -> string = strdup(str);
      grammar_parser -> state = GPStateOptionName;
      break;

    case TokenCodePercent:
      if (grammar_parser -> old_state == GPStateStart) {
        grammar_parser -> state = grammar_parser -> old_state;
      }
      break;

    case TokenCodeCloseBracket:
      if (grammar_parser -> old_state != GPStateStart) {
        grammar_parser -> state = grammar_parser -> old_state;
      }
      break;

    default:
      _grammar_parser_syntax_error(
          grammar_parser,
          "Unexpected token '%s' in option block",
          token_tostring(token));
  }
  return grammar_parser;
}

grammar_parser_t * _grammar_parser_state_option_name(token_t *token, grammar_parser_t *grammar_parser) {
  int        code;
  char      *str, *state_str;
  char       terminal_str[2];
  grammar_t *g;
  gp_state_t state;
  token_t   *t;

  g = grammar_parser -> grammar;
  state = grammar_parser -> state;
  code = token_code(token);
  str = token_token(token);

  switch (code) {
    case TokenCodeIdentifier:
    case TokenCodeInteger:
    case TokenCodeHexNumber:
    case TokenCodeFloat:
    case TokenCodeSQuotedStr:
    case TokenCodeDQuotedStr:
    case TokenCodeBQuotedStr:
    case TokenCodeDollar:
      ge_set_option(grammar_parser -> ge, grammar_parser -> string, token);
      free(grammar_parser -> string);
      grammar_parser -> string = NULL;
      grammar_parser -> state = GPStateOptions;
      break;

    case TokenCodeColon:
    case TokenCodeEquals:
      break;

    case TokenCodePercent:
      if (grammar_parser -> old_state == GPStateStart) {
        /* Hack - need to revisit the whole options thing. */
        t = token_create(TokenCodeIdentifier, grammar_parser -> string);
        ge_set_option(grammar_parser -> ge, strdup("done"), t);
        token_free(t);
        free(grammar_parser -> string);
        grammar_parser -> string = NULL;
        grammar_parser -> state = grammar_parser -> old_state;
      }
      break;

    case TokenCodeCloseBracket:
      if (grammar_parser -> old_state != GPStateStart) {
        /* Hack - need to revisit the whole options thing. */
        t = token_create(TokenCodeIdentifier, grammar_parser -> string);
        ge_set_option(grammar_parser -> ge, strdup("done"), t);
        token_free(t);
        free(grammar_parser -> string);
        grammar_parser -> string = NULL;
        grammar_parser -> state = grammar_parser -> old_state;
      }
      break;

    default:
      _grammar_parser_syntax_error(
          grammar_parser,
          "Unexpected token '%s' in option block",
          token_tostring(token));
  }
  return grammar_parser;
}

grammar_parser_t * _grammar_parser_state_header(token_t *token, grammar_parser_t *grammar_parser) {
  return grammar_parser;
}

grammar_parser_t * _grammar_parser_state_nonterminal(token_t *token, grammar_parser_t *grammar_parser) {
  int        code;
  char      *str, *state_str;
  char       terminal_str[2];
  char       buf[100];
  grammar_t *g;
  gp_state_t state;

  g = grammar_parser -> grammar;
  state = grammar_parser -> state;
  code = token_code(token);
  str = token_token(token);

  switch (code) {
    case TokenCodeIdentifier:
      grammar_parser -> nonterminal = nonterminal_create(grammar_parser -> grammar, str);
      grammar_parser -> rule = NULL;
      grammar_parser -> entry = NULL;
      grammar_parser -> ge = grammar_parser -> nonterminal -> ge;
      break;

    case TokenCodeOpenBracket:
      grammar_parser -> old_state = GPStateNonTerminal;
      grammar_parser -> state = GPStateOptions;
      break;

    case 200:
      if (grammar_parser -> nonterminal) {
        grammar_parser -> rule = rule_create(grammar_parser -> nonterminal);
        grammar_parser -> state = GPStateRule;
        grammar_parser -> ge = grammar_parser -> rule -> ge;
      } else {
        _grammar_parser_syntax_error(
            grammar_parser,
            "The ':=' operator must be preceded by a non-terminal name",
            token_tostring(token));
      }
      break;

    default:
      if (grammar_parser -> nonterminal) {
        _grammar_parser_syntax_error(
            grammar_parser,
            "Unexpected token '%s' in definition of non-terminal '%s'",
            token_tostring(token), grammar_parser -> nonterminal -> name);
      } else {
        _grammar_parser_syntax_error(
            grammar_parser,
            "Unexpected token '%s', was expecting non-terminal definition",
            token_tostring(token));
      }
  }
  return grammar_parser;
}

grammar_parser_t * _grammar_parser_state_rule(token_t *token, grammar_parser_t *grammar_parser) {
  int        code;
  char      *str, *state_str;
  char       terminal_str[2];
  grammar_t *g;
  gp_state_t state;

  g = grammar_parser -> grammar;
  state = grammar_parser -> state;
  code = token_code(token);
  str = token_token(token);

  switch (code) {
    case TokenCodePipe:
      grammar_parser -> rule = rule_create(grammar_parser -> nonterminal);
      grammar_parser -> state = GPStateRule;
      grammar_parser -> ge = grammar_parser -> rule -> ge;
      break;
    case TokenCodeSemiColon:
      grammar_parser -> nonterminal = NULL;
      grammar_parser -> rule = NULL;
      grammar_parser -> entry = NULL;
      grammar_parser -> state = GPStateNonTerminal;
      break;
    case TokenCodeOpenBracket:
      if (grammar_parser -> state == GPStateEntry) {
        grammar_parser -> old_state = grammar_parser -> state;
        grammar_parser -> state = GPStateOptions;
      }
      break;
    case TokenCodeIdentifier:
      grammar_parser -> entry = rule_entry_non_terminal(grammar_parser -> rule, str);
      grammar_parser -> ge = grammar_parser -> entry -> ge;
      grammar_parser -> state = GPStateEntry;
      break;
    case TokenCodeDQuotedStr:
      grammar_parser -> entry = rule_entry_terminal(grammar_parser -> rule, token);
      grammar_parser -> ge = grammar_parser -> entry -> ge;
      grammar_parser -> state = GPStateEntry;
      break;
    case TokenCodeSQuotedStr:
      if (strlen(str) == 1) {
        code = str[0];
        strcpy(terminal_str, str);
        token = token_create(code, terminal_str);
        grammar_parser -> entry = rule_entry_terminal(grammar_parser -> rule, token);
        grammar_parser -> ge = grammar_parser -> entry -> ge;
        token_free(token);
        grammar_parser -> state = GPStateEntry;
      } else {
        _grammar_parser_syntax_error(
            grammar_parser,
            "Single-quoted string longer than 1 character '%s' cannot be used in a rule or rule entry definition",
            token_tostring(token));
      }
      break;
    default:
      if ((code >= '!') && (code <= '~')) {
        grammar_parser -> entry = rule_entry_terminal(grammar_parser -> rule, token);
        grammar_parser -> ge = grammar_parser -> entry -> ge;
        grammar_parser -> state = GPStateEntry;
      } else {
        _grammar_parser_syntax_error(
            grammar_parser,
            "Token '%s' cannot be used in a rule or rule entry definition",
            token_tostring(token));
      }
      break;
  }
  return grammar_parser;
}

grammar_parser_t * _grammar_parser_state_entry(token_t *token, grammar_parser_t *grammar_parser) {
  return _grammar_parser_state_rule(token, grammar_parser);
}

#define  SYNTAX_ERROR

void _grammar_parser_syntax_error(grammar_parser_t *gp, char *msg, ...) {
  va_list  args;
  char     buf[256];

  va_start(args, msg);
  vsnprintf(buf, 256, msg, args);
  va_end(args);
  error("Syntax error in grammar: %s", buf);
  gp -> state = GPStateError;
}


grammar_parser_t * _grammar_token_handler(token_t *token, grammar_parser_t *grammar_parser) {
  int        code;
  char      *str, *state_str;
  char       terminal_str[2];
  grammar_t *g;
  gp_state_t state;

  g = grammar_parser -> grammar;
  state = grammar_parser -> state;
  code = token_code(token);
  str = token_token(token);

  if (grammar_debug) {
    debug("%-18.18s %s", _gp_state_recs[state].name, token_tostring(token));
  }
  _gp_state_recs[state].handler(token, grammar_parser);
  return (grammar_parser -> state != GPStateError) ? grammar_parser : NULL;
}

/*
 * grammar_parser_t public functions
 */

grammar_parser_t * _grammar_parser_create(reader_t *reader) {
  grammar_parser_t *grammar_parser;

  grammar_parser = NEW(grammar_parser_t);
  grammar_parser -> reader = reader;
  grammar_parser -> state = GPStateStart;
  grammar_parser -> grammar = NULL;
  grammar_parser -> string = NULL;
  grammar_parser -> nonterminal = NULL;
  grammar_parser -> rule = NULL;
  grammar_parser -> entry = NULL;
  grammar_parser -> ge = NULL;
  return grammar_parser;
}

void grammar_parser_free(grammar_parser_t *grammar_parser) {
  if (grammar_parser) {
    free(grammar_parser -> string);
    free(grammar_parser);
  }
}

grammar_t * grammar_parser_parse(grammar_parser_t *gp) {
  lexer_t          *lexer;
  grammar_parser_t *grammar_parser;

  gp -> grammar = grammar_create();
  lexer = lexer_create(gp -> reader);
  lexer_add_keyword(lexer, 200, ":=");
  lexer_set_option(lexer, LexerOptionIgnoreWhitespace, TRUE);
  lexer_tokenize(lexer, _grammar_token_handler, gp);
  if (grammar_analyze(gp -> grammar)) {
    if (grammar_debug) {
      grammar_dump(gp -> grammar);
      info("Grammar successfully analyzed");
    }
  } else {
    error("Error(s) analyzing grammar");
  }
  lexer_free(lexer);
  return gp -> grammar;
}

