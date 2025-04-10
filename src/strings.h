/*
** This file is part of the Matrix Brandy Basic VI Interpreter.
** Copyright (C) 2000-2014 David Daniels
** Copyright (C) 2018-2024 Michael McConnell and contributors
**
** Brandy is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2, or (at your option)
** any later version.
**
** Brandy is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Brandy; see the file COPYING.  If not, write to
** the Free Software Foundation, 59 Temple Place - Suite 330,
** Boston, MA 02111-1307, USA.
**
**
**      This file defines the functions and so forth associated with
**      memory manangement for strings
*/

#ifndef __strings_h
#define __strings_h

#include "common.h"
#include "basicdefs.h"

#define SHORT_STRING 256

extern void *alloc_string(int32);
extern void free_string(basicstring);
extern void discard_strings(byte *, int32);
extern char *resize_string(char *, int32, int32);
extern void clear_strings(void);
extern int32 get_stringlen(size_t);
extern void show_stringstats(void);
extern void check_alloc(void);

/* Additional string functions needed for RISC OS CLib build */
#ifdef TARGET_RISCOS
size_t strnlen(const char *, size_t);
#endif /* TARGET_RISCOS */

#endif /* __strings_h */

