/*
 * obelix.c - Copyright (c) 2014 Jan de Visser <jan@finiandarcy.com>  
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

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <data.h>
#include <file.h>
#include <lexer.h>
#include <logging.h>

int alex_debug;

void debug_settings(char *debug) {
  int      debug_all = 0;
  array_t *cats;
  int      ix;
  
  if (debug) {
    debug("debug optarg: %s", debug);
    cats = array_split(debug, ",");
    for (ix = 0; ix < array_size(cats); ix++) {
      logging_enable(str_array_get(cats, ix));
    }
  }
}

void * alex_tokenize(token_t *token, void *lexer) {
  printf("%s ", token_tostring(token));
  return lexer;
}

int main(int argc, char **argv) {
  lexer_t *lexer;
  char    *path = NULL;
  char    *debug = NULL;
  int      opt;
  file_t  *file;

  logging_register_category("alex", &alex_debug);
  while ((opt = getopt(argc, argv, "d:v:")) != -1) {
    switch (opt) {
      case 'd':
        debug = optarg;
        break;
      case 'v':
        logging_set_level(atoi(optarg));
        break;
    }
  }
  debug_settings(debug);
  if (optind >= argc) {
    fprintf(stderr, "Usage: alex [options] file\n");
    exit(1);
  }
  path = argv[optind];
  file = file_open(path);
  lexer = lexer_create((data_t *) file);
  if (lexer) {
    lexer_tokenize(lexer, alex_tokenize, lexer);
    printf("\n");
    lexer_free(lexer);
    file_free(file);
  }
  return 0;
}

