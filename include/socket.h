/*
 * /obelix/include/socket.h - Copyright (c) 2015 Jan de Visser <jan@finiandarcy.com>
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

#ifndef __SOCKET_H__
#define	__SOCKET_H__

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_WINSOCK2_H
#include <winsock2.h>
#endif

#include <file.h>
#include <thread.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef HAVE_TYPE_SOCKET
typedef int SOCKET;
#endif

typedef struct _connection {
  struct _socket *server;
  struct _socket *client;
  data_t         *context;
  thread_t       *thread;
} connection_t;

typedef void * (*service_t)(connection_t *);
    
typedef struct _socket {
  data_t     _d;
  file_t    *sockfile;
  SOCKET     fh;
  char      *host;
  char      *service;
  service_t  service_handler;
  thread_t  *thread;
  void      *context;
  int        _errno;
} socket_t;

OBLCORE_IMPEXP socket_t *    socket_create(char *, int);
OBLCORE_IMPEXP socket_t *    socket_create_byservice(char *, char *);
OBLCORE_IMPEXP socket_t *    serversocket_create(int);
OBLCORE_IMPEXP socket_t *    serversocket_create_byservice(char *);
OBLCORE_IMPEXP int           socket_close(socket_t *);
OBLCORE_IMPEXP unsigned int  socket_hash(socket_t *);
OBLCORE_IMPEXP int           socket_cmp(socket_t *, socket_t *);
OBLCORE_IMPEXP int           socket_listen(socket_t *, service_t, void *);
OBLCORE_IMPEXP int           socket_listen_detach(socket_t *, service_t, void *);
OBLCORE_IMPEXP socket_t *    socket_interrupt(socket_t *);
OBLCORE_IMPEXP socket_t *    socket_nonblock(socket_t *);
OBLCORE_IMPEXP int           socket_read(socket_t *, void *, int);
OBLCORE_IMPEXP int           socket_write(socket_t *, void *, int);

OBLCORE_IMPEXP void *        connection_listener_service(connection_t *);

#define data_is_socket(d)  ((d) && (data_hastype((d), Socket)))
#define data_as_socket(d)  ((socket_t *) (data_is_socket((data_t *) (d)) ? (d) : NULL))
#define socket_free(o)     (data_free((data_t *) (o)))
#define socket_tostring(o) (data_tostring((data_t *) (o)))
#define socket_copy(o)     ((socket_t *) data_copy((data_t *) (o)))


OBLCORE_IMPEXP int Socket;

#ifdef	__cplusplus
}
#endif

#endif	/* __SOCKET_H__ */
