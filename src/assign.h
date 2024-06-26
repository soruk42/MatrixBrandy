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
**      This file defines functions that handle assignments
**      in assign.c
*/

#ifndef __assign_h
#define __assign_h

extern void exec_assignment(void);
extern void assign_staticvar(void);
extern void assign_intvar(void);
extern void assign_uint8var(void);
extern void assign_int64var(void);
extern void assign_floatvar(void);
extern void assign_stringvar(void);
extern void assign_pseudovar(void);

#endif
