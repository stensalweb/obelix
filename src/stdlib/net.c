/*
 * obelix/src/stdlib/net.c - Copyright (c) 2015 Jan de Visser <jan@finiandarcy.com>
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

#include <data.h>
#include <socket.h>

static void          _net_init(void) __attribute__((constructor));

/* -------------------------------------------------------------------------*/

int    Socket;
int    net_debug = 0;

typedef struct _socket_wrapper {
  socket_t *socket;
  data_t   *fd;
} socket_wrapper_t;

static socket_wrapper_t * _wrapper_create(socket_t *);
static void               _wrapper_free(socket_wrapper_t *);

static data_t *      _socket_new(data_t *, va_list);
static int           _socket_cmp(data_t *, data_t *);
static char *        _socket_tostring(data_t *);
static unsigned int  _socket_hash(data_t *);
static data_t *      _socket_resolve(data_t *, char *);

static data_t *      _socket_close(data_t *, char *, array_t *, dict_t *);

static vtable_t _vtable_list[] = {
  { .id = FunctionNew,      .fnc = (void_t) _socket_new },
  { .id = FunctionCmp,      .fnc = (void_t) _socket_cmp },
  { .id = FunctionFree,     .fnc = (void_t) socket_free },
  { .id = FunctionToString, .fnc = (void_t) _socket_tostring },
  { .id = FunctionHash,     .fnc = (void_t) _socket_hash },
  { .id = FunctionResolve,  .fnc = (void_t) _socket_resolve },
  { .id = FunctionNone,     .fnc = NULL }
};

static typedescr_t _typedescr_socket =   {
  .type      = -1,
  .type_name = "socket",
  .vtable    = _vtable_list,
};

static methoddescr_t _methoddescr_socket[] = {
  { .type = -1,     .name = "close", .method = _socket_close,   .argtypes = { NoType, NoType, NoType }, .minargs = 0, .varargs = 0 },
  { .type = NoType, .name = NULL,    .method = NULL,            .argtypes = { NoType, NoType, NoType }, .minargs = 0, .varargs = 0 },
};

#define data_is_socket(d) ((d) && (data_type((d)) == Socket))
#define data_socketval(d) ((socket_wrapper_t *) ((data_is_socket((d)) ? (d) -> ptrval : NULL)))


/* -------------------------------------------------------------------------*/

void _net_init(void) {
  int ix;

  logging_register_category("net", &net_debug);
  Socket = typedescr_register(&_typedescr_socket);
  for (ix = 0; _methoddescr_socket[ix].type != NoType; ix++) {
    if (_methoddescr_socket[ix].type == -1) {
      _methoddescr_socket[ix].type = Socket;
    }
  }
  typedescr_register_methods(_methoddescr_socket);
}

/* -------------------------------------------------------------------------*/

socket_wrapper_t * _wrapper_create(socket_t *socket) {
  socket_wrapper_t   *wrapper;
  static typedescr_t *filetype = NULL;
  file_t             *f;
  
  
  if (!filetype) {
    filetype = typedescr_get_byname("file");
    assert(filetype);
  }
  
  wrapper = NEW(socket_wrapper_t);
  wrapper -> socket = socket;
  
  /* FIXME Need better API to create file atoms from open files */
  wrapper -> fd = data_create(filetype -> type, NULL);
  f = (file_t *) wrapper -> fd -> ptrval;
  file_free(f);
  wrapper -> fd -> ptrval = file_copy(wrapper -> socket -> sockfile);
  return wrapper;
}

void _wrapper_free(socket_wrapper_t *wrapper) {
  if (wrapper) {
    data_free(wrapper -> fd);
    socket_free(wrapper -> socket);
    free(wrapper);
  }
}

/* -------------------------------------------------------------------------*/

data_t * _function_connect(char *name, array_t *params, dict_t *kwargs) {
  data_t *host;
  data_t *service;
  data_t *socket;

  assert(params && (array_size(params) >= 2));
  host = (data_t *) array_get(params, 0);
  service = (data_t *) array_get(params, 1);
  
  socket = data_create(Socket, data_tostring(host), data_tostring(service));
  return socket;
}

void * _listener_service(void **params) {
  data_t   *server = (data_t) params[0];
  socket_t *client = (socket_t) params[1];
  data_t   *socket = data_create(Socket, NULL, NULL);
  data_t   *ret;
  array_t  *p;
  
  socket -> ptrval = _wrapper_create(client);
  p = data_array_create(1);
  array_push(p, socket);
  ret = data_call(server, p, NULL);
  array_free(p);
  return ret;
}

data_t * _function_listen(char *name, array_t *params, dict_t *kwargs) {
  data_t *service;
  data_t *server;
  data_t *ret;

  assert(params && (array_size(params) >= 2));
  service = data_array_get(params, 0);
  server = data_array_get(params, 0);
  assert(data_is_callable(server));
  
  ret = data_create(Int, socket_listen(data_intval(service), 
                                       _listener_service, 
                                       server));
}

/* -------------------------------------------------------------------------*/

data_t * _socket_new(data_t *target, va_list args) {
  char               *host;
  char               *service;
  
  
  host = va_arg(args, char *);
  service = va_arg(args, char *);

  /* FIXME Error handling */
  if (host && service) {
    target -> ptrval = _wrapper_create(socket_create_byservice(host, service));
  }
  return target;
}

int _socket_cmp(data_t *d1, data_t *d2) {
  return socket_cmp(data_socketval(d1) -> socket, data_socketval(d2) -> socket);
}

char * _socket_tostring(data_t *data) {
  return socket_tostring(data_socketval(data) -> socket);
}

unsigned int _socket_hash(data_t *data) {
  return socket_hash(data_socketval(data) -> socket);
}

data_t * _socket_resolve(data_t *data, char *attr) {
  if (!strcmp(attr, "fd")) {
    return data_copy(data_socketval(data) -> fd);
  } else if (!strcmp(attr, "host")) {
    return data_create(String, data_socketval(data) -> socket -> host);
  } else if (!strcmp(attr, "service")) {
    return data_create(String, data_socketval(data) -> socket -> service);
  } else {
    return NULL;
  }
}

data_t * _socket_close(data_t *self, char *name, array_t *args, dict_t *kwargs) {
  return data_create(Int, socket_close(data_socketval(self) -> socket));
}
