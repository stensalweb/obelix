/*
 * /obelix/src/stdlib/fileobj.c - Copyright (c) 2015 Jan de Visser <jan@finiandarcy.com>
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
#include <regex.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/types.h>

#include <data.h>
#include <exception.h>
#include <file.h>
#include <typedescr.h>
#include <wrapper.h>

int File = -1;
int FileIter = -1;

static void          _file_init(void) __attribute__((constructor));
static data_t *      _file_new(data_t *, va_list);
static data_t *      _file_copy(data_t *, data_t *);
static int           _file_cmp(data_t *, data_t *);
static char *        _file_tostring(data_t *);
static unsigned int  _file_hash(data_t *);
static data_t *      _file_enter(data_t *);
static data_t *      _file_leave(data_t *, data_t *);
static data_t *      _file_query(data_t *, data_t *);
static data_t *      _file_iter(data_t *);
static data_t *      _file_resolve(data_t *, char *);
static data_t *      _file_read(data_t *, char *, int);
static data_t *      _file_write(data_t *, char *, int);

static data_t *      _file_open(data_t *, char *, array_t *, dict_t *);
static data_t *      _file_adopt(data_t *, char *, array_t *, dict_t *);
static data_t *      _file_readline(data_t *, char *, array_t *, dict_t *);
static data_t *      _file_print(data_t *, char *, array_t *, dict_t *);
static data_t *      _file_close(data_t *, char *, array_t *, dict_t *);
static data_t *      _file_redirect(data_t *, char *, array_t *, dict_t *);
static data_t *      _file_seek(data_t *, char *, array_t *, dict_t *);

static vtable_t _vtable_file[] = {
  { .id = FunctionNew,      .fnc = (void_t) _file_new },
  { .id = FunctionCopy,     .fnc = (void_t) _file_copy },
  { .id = FunctionCmp,      .fnc = (void_t) _file_cmp },
  { .id = FunctionFree,     .fnc = (void_t) file_free },
  { .id = FunctionToString, .fnc = (void_t) _file_tostring },
  { .id = FunctionHash,     .fnc = (void_t) _file_hash },
  { .id = FunctionEnter,    .fnc = (void_t) _file_enter },
  { .id = FunctionLeave,    .fnc = (void_t) _file_leave },
  { .id = FunctionIter,     .fnc = (void_t) _file_iter },
  { .id = FunctionQuery,    .fnc = (void_t) _file_query },
  { .id = FunctionResolve,  .fnc = (void_t) _file_resolve },
  { .id = FunctionRead,     .fnc = (void_t) _file_read },
  { .id = FunctionWrite,    .fnc = (void_t) _file_write },
  { .id = FunctionNone,     .fnc = NULL }
};

static typedescr_t _typedescr_file =   {
  .type      = -1,
  .type_name = "file",
  .vtable    = _vtable_file,
};

static methoddescr_t _methoddescr_file[] = {
  { .type = Any,    .name = "open",     .method = _file_open,     .argtypes = { String, Int, Any },       .minargs = 1, .varargs = 1 },
  { .type = Any,    .name = "adopt",    .method = _file_adopt,    .argtypes = { Int, NoType, NoType },    .minargs = 1, .varargs = 0 },
  { .type = -1,     .name = "readline", .method = _file_readline, .argtypes = { NoType, NoType, NoType }, .minargs = 0, .varargs = 0 },
  { .type = -1,     .name = "print",    .method = _file_print,    .argtypes = { String, Any,    NoType }, .minargs = 1, .varargs = 1 },
  { .type = -1,     .name = "close",    .method = _file_close,    .argtypes = { NoType, NoType, NoType }, .minargs = 0, .varargs = 0 },
  { .type = -1,     .name = "redirect", .method = _file_redirect, .argtypes = { String, NoType, NoType }, .minargs = 1, .varargs = 0 },
  { .type = -1,     .name = "seek",     .method = _file_seek,     .argtypes = { Int, NoType, NoType },    .minargs = 1, .varargs = 0 },
  { .type = NoType, .name = NULL,       .method = NULL,           .argtypes = { NoType, NoType, NoType }, .minargs = 0, .varargs = 0 }
};

typedef struct _fileiter {
  file_t  *file;
  regex_t *regex;
  data_t  *next;
  int      refs;
  char    *str;
} fileiter_t;

static fileiter_t * _fileiter_create(va_list);
static void         _fileiter_free(fileiter_t *);
static fileiter_t * _fileiter_copy(fileiter_t *);
static int          _fileiter_cmp(fileiter_t *, fileiter_t *);
static char *       _fileiter_tostring(fileiter_t *);
static data_t *     _fileiter_has_next(fileiter_t *);
static data_t *     _fileiter_next(fileiter_t *);

static vtable_t _wrapper_vtable_fileiter[] = {
  { .id = FunctionFactory,  .fnc = (void_t) _fileiter_create },
  { .id = FunctionCopy,     .fnc = (void_t) _fileiter_copy },
  { .id = FunctionCmp,      .fnc = (void_t) _fileiter_cmp },
  { .id = FunctionFree,     .fnc = (void_t) _fileiter_free },
  { .id = FunctionToString, .fnc = (void_t) _fileiter_tostring },
  { .id = FunctionHasNext,  .fnc = (void_t) _fileiter_has_next },
  { .id = FunctionNext,     .fnc = (void_t) _fileiter_next },
  { .id = FunctionNone,     .fnc = NULL }
};

#define data_is_fileiter(d)  ((d) && (data_type((d)) == FileIter))
#define data_fileiterval(d)  ((fileiter_t *) (data_is_fileiter((d)) ? ((d) -> ptrval) : NULL))

/* ------------------------------------------------------------------------ */

void _file_init(void) {
  int ix;
  
  File = typedescr_register(&_typedescr_file);
  if (file_debug) {
    debug("File type initialized");
  }
  for (ix = 0; _methoddescr_file[ix].type != NoType; ix++) {
    if (_methoddescr_file[ix].type == -1) {
      _methoddescr_file[ix].type = File;
    }
  }
  typedescr_register_methods(_methoddescr_file);
  FileIter = wrapper_register(FileIter, "fileiterator", _wrapper_vtable_fileiter);
}

/* -- F I L E  I T E R A T O R -------------------------------------------- */

fileiter_t * _fileiter_create(va_list args) {
  fileiter_t *ret = NEW(fileiter_t);
  char       *regex;
  int         retval;
  
  ret -> file = va_arg(args, file_t *);
  regex = va_arg(args, char *);
  retval = file_seek(ret -> file, 0);
  if (retval >= 0) {
    if (file_readline(ret -> file)) {
      ret -> next = data_create(String, ret -> file -> line);
    } else if (!file_errno(ret -> file)) {
      ret -> next = data_exception(ErrorExhausted, "Iterator exhausted");
    } else {
      ret -> next = data_exception_from_my_errno(file_errno(ret -> file));
    }
  } else {
    ret -> next = data_exception_from_my_errno(file_errno(ret -> file));
  }
  ret -> refs = 1;
  ret -> str = NULL;
  return ret;
}

void _fileiter_free(fileiter_t *fileiter) {
  if (fileiter && (--fileiter -> refs <= 0)) {
    data_free(fileiter -> next);
    free(fileiter -> str);
    free(fileiter);
  }
}

fileiter_t * _fileiter_copy(fileiter_t *fileiter) {
  if (fileiter) {
    fileiter -> refs++;
  }
  return fileiter;
}

int _fileiter_cmp(fileiter_t *fileiter1, fileiter_t *fileiter2) {
  return fileiter1 -> file -> fh - fileiter2 -> file -> fh;
}

char * _fileiter_tostring(fileiter_t *fileiter) {
  if (!fileiter -> str) {
    fileiter -> str = strdup(file_tostring(fileiter -> file));
  }
  return fileiter -> str;
}

data_t * _fileiter_has_next(fileiter_t *fi) {
  exception_t *ex;
  data_t      *ret;
  
  if (data_hastype(fi -> next, String)) {
    ret = data_create(Bool, 1);
  } else if (data_is_exception(fi -> next)) {
    ex = data_exceptionval(fi -> next);
    ret = (ex -> code == ErrorExhausted) ? data_create(Bool, 0) : data_copy(fi -> next);
  }
  if (file_debug) {
    debug("%s._fileiter_has_next() -> %s", _fileiter_tostring(fi), data_tostring(ret));
  }
  return ret;
}

data_t * _fileiter_next(fileiter_t *fi) {
  data_t *ret = data_copy(fi -> next);
  
  if (data_hastype(ret, String)) {
    data_free(fi -> next);
    if (file_readline(fi -> file)) {
      fi -> next = data_create(String, fi -> file -> line);
    } else if (!file_errno(fi -> file)) {
      fi -> next = data_exception(ErrorExhausted, "Iterator exhausted");
    } else {
      fi -> next = data_exception_from_my_errno(file_errno(fi -> file));
    }
  }
  if (file_debug) {
    debug("%s._fileiter_next() -> %s", _fileiter_tostring(fi), data_tostring(ret));
  }
  return ret;
}


/* ------------------------------------------------------------------------ */

/* -- F I L E  D A T A T Y P E -------------------------------------------- */

data_t * data_wrap_file(file_t *file) {
  data_t *ret = data_create_noinit(File);
  
  ret -> ptrval = file_copy(file);
  return ret;
}

data_t * _file_new(data_t *target, va_list arg) {
  file_t *f;
  char   *name;

  name = va_arg(arg, char *);
  if (name) {
    f = file_open(name);
    if (file_isopen(f)) {
      target -> ptrval = f;
      return target;
    } else {
      return data_exception_from_my_errno(f -> _errno);
    }
  } else {
    f = file_create(-1);
    target -> ptrval = f;
    return target;
  }
}

int _file_cmp(data_t *d1, data_t *d2) {
  file_t *f1 = data_fileval(d1);
  file_t *f2 = data_fileval(d2);

  return file_cmp(f1, f2);
}

data_t * _file_copy(data_t *dest, data_t *src) {
  dest -> ptrval = file_copy(data_fileval(src));
  return dest;
}

char * _file_tostring(data_t *data) {
  return file_tostring(data_fileval(data));
}

unsigned int _file_hash(data_t *data) {
  return strhash(data_fileval(data) -> fname);
}

data_t * _file_enter(data_t *file) {
  if (file_debug) {
    debug("%s._file_enter()", data_tostring(file));
  }
  return file;
}

data_t * _file_leave(data_t *data, data_t *param) {
  data_t *ret = param;
  
  if (file_close(data_fileval(data))) {
    ret = data_exception_from_my_errno(file_errno(data_fileval(data)));
  }
  if (file_debug) {
    debug("%s._file_leave() -> %s", data_tostring(data), data_tostring(ret));
  }
  return ret;
}

data_t * _file_resolve(data_t *file, char *name) {
  file_t *f = data_fileval(file);
  
  if (!strcmp(name, "errno")) {
    return data_create(Int, file_errno(f));
  } else if (!strcmp(name, "errormsg")) {
    return data_create(String, file_error(f));
  } else if (!strcmp(name, "name")) {
    return data_create(String, file_name(f));
  } else if (!strcmp(name, "fh")) {
    return data_create(Int, f -> fh);
  } else if (!strcmp(name, "eof")) {
    return data_create(Bool, file_eof(f));
  } else {
    return NULL;
  }
}

data_t * _file_iter(data_t *file) {
  file_t     *f = data_fileval(file);
  data_t     *ret = data_create(FileIter, f);
  fileiter_t *fi = _fileiter_copy(data_fileiterval(ret));
  
  if (data_is_exception(fi -> next) && 
      (data_exceptionval(fi -> next) -> code != ErrorExhausted)) {
    data_free(ret);
    ret = data_copy(fi -> next);
    _fileiter_free(fi);
  }
  if (file_debug) {
    debug("%s._file_iter() -> %s", data_tostring(file), data_tostring(ret));
  }
  return ret;
}

data_t * _file_query(data_t *file, data_t *regex) {
  if (file_debug) {
    debug("%s._file_query(%s)", data_tostring(file), data_tostring(regex));
  }
  return data_null();
}

data_t * _file_read(data_t *file, char *buf, int num) {
  int retval;
  
  if (file_debug) {
    debug("%s.read(%d)", data_tostring(file), num);
  }
  retval = file_read(data_fileval(file), buf, num);
  if (retval >= 0) {
    return data_create(Int, retval);
  } else {
    return data_exception_from_errno();
  }
}

data_t * _file_write(data_t *file, char *buf, int num) {
  int retval;
  
  if (file_debug) {
    debug("%s.write(%d)", data_tostring(file), num);
  }
  retval = file_write(data_fileval(file), buf, num);
  if (retval >= 0) {
    return data_create(Int, retval);
  } else {
    return data_exception_from_errno();
  }
}

/* ----------------------------------------------------------------------- */

data_t * _file_open(data_t *self, char *name, array_t *args, dict_t *kwargs) {
  char   *n;
  data_t *ret;
  
  if (!args || !array_size(args)) {
    n = data_tostring(self);
  } else if (array_size(args) > 1) {
    // FIXME open mode!
    return data_exception(ErrorArgCount, "open() takes exactly one argument");
  } else {
    n = data_tostring(array_get(args, 0));
  }
  ret = data_create(File, n);
  if (!ret) {
    ret = data_create(Bool, 0);
  }
  return ret;
}

data_t * _file_adopt(data_t *self, char *name, array_t *args, dict_t *kwargs) {
  data_t *ret = data_create(File, NULL);
  int     fh = data_intval(data_array_get(args, 0));
  
  data_fileval(ret) -> fh = fh;
  if (file_debug) {
    debug("_file_adopt(%d) -> %s", fh, data_tostring(ret));
  }
  return ret;
}

data_t * _file_seek(data_t *self, char *name, array_t *args, dict_t *kwargs) {
  int     offset = data_intval(data_array_get(args, 0));
  int     retval;
  data_t *ret;
  
  retval = file_seek(data_fileval(self), offset);
  if (retval >= 0) {
    ret = data_create(Int, retval);
  } else {
    ret = data_exception_from_my_errno(file_errno(data_fileval(self)));
  }
  return ret;
}

data_t * _file_readline(data_t *self, char *name, array_t *args, dict_t *kwargs) {
  file_t *file = data_fileval(self);
  data_t *ret;
  char   *line;
  
  (void) name;
  (void) args;
  (void) kwargs;
  if (line = file_readline(file)) {
    ret = data_create(String, line);
  } else if (!file_errno(file)) {
    ret = data_null();
  } else {
    ret = data_exception_from_my_errno(file_errno(file));
  }
  return ret;
}

data_t * _file_print(data_t *self, char *name, array_t *args, dict_t *kwargs) {
  data_t *s;
  data_t *fmt;
  char   *line;
  int     ret = 1;

  fmt = data_array_get(args, 0);
  assert(fmt);
  args = array_slice(args, 1, -1);
  s = data_execute(fmt, "format", args, kwargs);
  array_free(args);
 
  line = data_tostring(s);
  if (file_write(data_fileval(self), line, strlen(line)) == strlen(line)) {
    if (file_write(data_fileval(self), "\n", 1) == 1) {
      file_flush(data_fileval(self));
      ret = 1;
    }
  }
  data_free(s);
  return data_create(Bool, ret);
}

data_t * _file_close(data_t *self, char *name, array_t *args, dict_t *kwargs) {
  int retval = file_close(data_fileval(self));
  
  if (!retval) {
    return data_create(Bool,  1);
  } else {
    return data_exception_from_my_errno(file_errno(data_fileval(self)));
  }
}

data_t * _file_redirect(data_t *self, char *name, array_t *args, dict_t *kwargs) {
  return data_create(Bool, file_redirect(data_fileval(self), 
					 data_tostring(data_array_get(args, 0))) == 0);
}
