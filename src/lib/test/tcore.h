/*
 * tcore.h - Copyright (c) 2016 Jan de Visser <jan@finiandarcy.com>
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

#ifndef __TCORE_H__
#define __TCORE_H__

#include <check.h>
#include <testsuite.h>

extern void list_init(void);
extern void dict_init(void);
extern void tdatalist_init();
extern void str_test_init(void);
extern void str_format_init(void);
extern void resolve_init(char *);
extern void init_suite(int, char **);

#endif /* __TCORE_H__ */
