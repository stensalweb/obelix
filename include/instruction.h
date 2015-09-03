/*
 * /obelix/include/instruction.h - Copyright (c) 2014 Jan de Visser <jan@finiandarcy.com>
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

#ifndef __INSTRUCTION_H__
#define __INSTRUCTION_H__

#include <core.h>
#include <data.h>
#include <name.h>
#include <set.h>

#ifdef  __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct _instruction {
  data_t    _d;
  int       line;
  char      label[9];
  set_t    *labels;
  char     *name;
  data_t   *value;
} instruction_t;

typedef enum _callflag {
  CFNone        = 0x0000,
  CFInfix       = 0x0001,
  CFConstructor = 0x0002,
  CFVarargs     = 0x0004
} callflag_t;

typedef struct _function_call {
  data_t      _d;
  name_t     *name;
  callflag_t  flags;
  int         arg_count;
  array_t    *kwargs;
  char       *str;
} function_call_t;

extern int Instruction;
extern int ITByValue;
extern int ITByName;
extern int ITByNameValue;
extern int ITByValueOrName;

extern int ITAssign;
extern int ITDecr;
extern int ITDup;
extern int ITEndLoop;
extern int ITEnterContext;
extern int ITFunctionCall;
extern int ITIncr;
extern int ITIter;
extern int ITJump;
extern int ITLeaveContext;
extern int ITNext;
extern int ITNop;
extern int ITPop;
extern int ITPushCtx;
extern int ITPushVal;
extern int ITDeref;
extern int ITPushScope;
extern int ITReturn;
extern int ITStash;
extern int ITSubscript;
extern int ITSwap;
extern int ITTest;
extern int ITThrow;
extern int ITUnstash;
extern int ITVMStatus;
extern int ITYield;

extern void            instruction_trace(char *, char *, ...);

extern data_t *        instruction_create_enter_context(name_t *, data_t *);
extern data_t *        instruction_create_function(name_t *, callflag_t, long, array_t *);

extern instruction_t * instruction_assign_label(instruction_t *);
extern instruction_t * instruction_set_label(instruction_t *, data_t *);

#define data_is_instruction(d)  ((d) && data_hastype((d), Instruction))
#define data_as_instruction(d)  ((instruction_t *) (data_is_instruction((d)) ? (d) : NULL))
#define instruction_free(i)     (data_free((data_t *) (i)))
#define instruction_tostring(i) (data_tostring((data_t *) (i)))
#define instruction_copy(i)     ((instruction_t *) data_copy((data_t *) (i)))

#define instruction_create_assign(n)        ((data_t *) data_create(ITAssign, name_tostring((n)), data_create(Name, (n))))
#define instruction_create_decr()           ((data_t *) data_create(ITDecr, NULL, NULL))
#define instruction_create_dup()            ((data_t *) data_create(ITDup, NULL, NULL))
#define instruction_create_incr()           ((data_t *) data_create(ITIncr, NULL, NULL))
#define instruction_create_iter()           ((data_t *) data_create(ITIter, NULL, NULL))
#define instruction_create_jump(l)          ((data_t *) data_create(ITJump, data_tostring((l)), NULL))
#define instruction_create_leave_context(n) ((data_t *) data_create(ITLeaveContext, name_tostring((n)), data_create(Name, (n))))
#define instruction_create_mark(l)          ((data_t *) data_create(ITNop, NULL, data_create(Int, (l))))
#define instruction_create_nop()            ((data_t *) data_create(ITNop, NULL, NULL))
#define instruction_create_next(n)          ((data_t *) data_create(ITNext, data_tostring((n)), NULL))
#define instruction_create_pop()            ((data_t *) data_create(ITPop, NULL, NULL))
#define instruction_create_pushctx()        ((data_t *) data_create(ITPushCtx, NULL, NULL))
#define instruction_create_pushscope()      ((data_t *) data_create(ITPushScope, NULL, NULL))
#define instruction_create_pushval(v)       ((data_t *) data_create(ITPushVal, NULL, data_copy((v))))
#define instruction_create_deref(n)         ((data_t *) data_create(ITDeref, name_tostring((n)), data_create(Name, (n))))
#define instruction_create_return()         ((data_t *) data_create(ITReturn, NULL, NULL))
#define instruction_create_stash(s)         ((data_t *) data_create(ITStash, NULL, data_create(Int, (s))))
#define instruction_create_swap()           ((data_t *) data_create(ITSwap, NULL, NULL))
#define instruction_create_test(l)          ((data_t *) data_create(ITTest, data_tostring((l)), NULL))
#define instruction_create_throw()          ((data_t *) data_create(ITThrow, NULL, NULL))
#define instruction_create_unstash(s)       ((data_t *) data_create(ITUnstash, NULL, data_create(Int, (s))))

#ifdef  __cplusplus
}
#endif /* __cplusplus */

#endif /* __INSTRUCTION_H__ */
