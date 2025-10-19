/*
** This file is part of the Matrix Brandy Basic V/VI Interpreter.
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
**      Target-specific declarations
**
** 20th August 2002 Crispian Daniels:
**      Included a Mac OS X target for conditional compilation.
**
** 04-Dec-2018 JGH: Rearranged to make for easier human parsing.
**
*/


#ifndef __target_h
#define __target_h

#define BRANDY_NAME  "Matrix"
#define BRANDY_MAJOR "1"
#define BRANDY_MINOR "23"
#define BRANDY_PATCHLEVEL "6"
#define BRANDY_DATE       "19 Oct 2025"
// #define BRANDY_PATCHDATE  "JGH191007"

#ifdef BRANDY_RELEASE
#undef DEBUG
#endif

/*
** DEFAULTSIZE and MINSIZE give the default and minimum Basic
** workspace sizes in bytes. DEFAULTSIZE is the amount of memory
** acquired when the interpreter first starts up and MINSIZE
** is the minimum it can be changed to.
** Default size is 64 megabytes. Override this by adding
** -DBRANDY_DEFAULT_SIZE to your BRANDY_BUILD_FLAGS environment
** variable, as a value in kilobytes. Don't go below 16.
*/

#ifndef BRANDY_DEFAULT_SIZE
#define BRANDY_DEFAULT_SIZE 65536
#endif
#define DEFAULTSIZE (BRANDY_DEFAULT_SIZE * 1024)
#define MINSIZE 16384

/* Make the startup mode a compile-time option.
** Default mode is 0 - the hardwired value up to now.
** add -DBRANDY_STARTUP_MODE=<mode> to your
** BRANDY_BUILD_FLAGS environment variable to override
*/
#ifndef BRANDY_STARTUP_MODE
#ifdef BRANDY_MODE7ONLY
#define BRANDY_STARTUP_MODE 7
#else
#define BRANDY_STARTUP_MODE 0
#endif /* BRANDY_MODE7ONLY */
#endif /* BRANDY_STARTUP_MODE */

/*
** Define the operating system-specific types used for integer
** and floating point types in Basic. 32-bit integer (signed
** and unsigned) and 64-bit floating point types are needed.
**
** The following are suitable for ARM and X86
*/
typedef unsigned char uint8;            /* Type for unsigned 8-bit integer variables in Basic */
typedef int int32;                      /* Type for 32-bit integer variables in Basic */
typedef unsigned int uint32;            /* 32-bit unsigned integer */
typedef double float64;                 /* Type for 64-bit floating point variables in Basic */
typedef long double float80;            /* High precision float for internal intermediary calculations */
typedef long long int int64;            /* Type for 64-bit integer variables */
typedef unsigned long long int uint64;  /* 64-bit unsigned integer */
typedef long int nativeint;             /* 32 or 64-bit depending on architecture */
typedef unsigned long int nativeuint;   /* 32 or 64-bit depending on architecture */

/*
** The following macros define the OS under which the program is being
** compiled and run. It uses macros predefined in various compilers to
** figure this out. Alternatively, the 'TARGET_xxx' macro can be hard-
** coded here. This is the most important macro and is used to control
** the compilation of OS-specific parts of the program.
**
** BRANDY_OS is displayed by the startup and *HELP string.
** MACTYPE indicates the system, returned by OSBYTE 0, and indicates the filing
** system type and other host hardware features.
**  0x0600 for directory.file/ext (eg RISC OS)
**  0x0800 for directory/file.ext (eg UNIX)
**  0x2000 for directory\file.ext (eg Win/DOS)
**
** OSVERSION indicates Matrix Brandy, returned by INKEY-256 and OSBYTE 129,-256.
**  This value is a unique identifier for Matrix Brandy, a program that
**  has obtained this value &4D can request further details from the
**  SYS "Brandy_Platform" call.
**
** LEGACY_OSVERSION indicates the host OS, returned by parameter R5 of
**  the SYS "Brandy_Platform" call, and in other versions of Brandy (and old
**  versions of Matrix Brandy) is returned by INKEY-256 and OSBYTE 129,-256.
**  These values are made up, but see beebwiki.mdfs.net/OSBYTE_&81
**
** Name of editor invoked by Basic 'EDIT' command.
** EDITOR_VARIABLE is the name of an environment variable that can be
**              read to find the name of the editor to use.
** DEFAULT_EDITOR is the name of the editor to use if there is no
**              environment variable.
**
** Characters used to separate directories in names of files
** DIR_SEPS     is a string containing all the characters that can be
**              be used to separate components of a file name (apart
**              from the file name's extension).
** DIR_SEP      gives the character to be used to separate directory names.
*/

#if defined(__i386__)
#define CPUTYPE "x86-32"
#elif defined(__x86_64__)
#define CPUTYPE "x86-64"
#elif defined(__arm__)
#if defined(__APCS_26__)
#define CPUTYPE "ARM-26bit"
#elif defined(__APCS_32__)
#define CPUTYPE "ARM-32bit"
#else
#define CPUTYPE "ARM-unknown"
#endif
#elif defined(__aarch64__)
#define CPUTYPE "ARM-64bit"
#else
#define CPUTYPE "Unknown"
#endif

#define OSVERSION 0x4D

#if defined(__LP64__) || defined(__WIN64__)
#define MATRIX64BIT 1
#define FMT_SZX "%llX"
#define FMT_SZD "%llu"
#else
#define FMT_SZX "%X"
#define FMT_SZD "%u"
#endif /* LP64 */

#ifdef __riscos
#define TARGET_RISCOS
#define BRANDY_OS "RISC OS"
#undef CPUTYPE
#ifdef __ARM_ARCH_2__
#define CPUTYPE "ARMv2-26bit"
#elif defined(__ARM_ARCH_3__)
#define CPUTYPE "ARMv3-32bit"
#elif defined(__ARM_ARCH_6ZK__)
#define CPUTYPE "ARMv6-32bit"
#else
#define CPUTYPE "UnixLib"
#endif /* __TARGET_SCL__ */
// OSVERSION returned by OS call
// This could be changed...
#define LEGACY_OSVERSION 0xA0
#define MACTYPE   0x0600
#define EDITOR_VARIABLE "Brandy$Editor"
#define DEFAULT_EDITOR  "Filer_Run"
#define DIR_SEPS ".:"
#define DIR_SEP  '.'
#define NOTEKGFX 1
/* It scales itself, but this gets close to maxing out the WimpSlot */
#define MAXRECDEPTH 119808
#endif

#ifdef __NetBSD__
#define TARGET_NETBSD
#define TARGET_UNIX
#define BRANDY_OS "NetBSD"
#define LEGACY_OSVERSION 0xFE
#define MACTYPE   0x0800
#define EDITOR_VARIABLE "BRANDY_EDITOR"
#define DEFAULT_EDITOR  "vi"
#define DIR_SEPS "/"
#define DIR_SEP  '/'
#define BRANDY_HAS_STRL_FUNCTIONS
#endif

#ifdef __FreeBSD__
#define TARGET_FREEBSD
#define TARGET_UNIX
#define BRANDY_OS "FreeBSD"
#define LEGACY_OSVERSION 0xF7
#define MACTYPE   0x0800
#define EDITOR_VARIABLE "BRANDY_EDITOR"
#define DEFAULT_EDITOR  "vi"
#define DIR_SEPS "/"
#define DIR_SEP  '/'
#define BRANDY_HAS_STRL_FUNCTIONS
#endif

#ifdef __OpenBSD__
#define TARGET_OPENBSD
#define TARGET_UNIX
#define BRANDY_OS "OpenBSD"
#define LEGACY_OSVERSION 0xF6
#define MACTYPE   0x0800
#define EDITOR_VARIABLE "BRANDY_EDITOR"
#define DEFAULT_EDITOR  "vi"
#define DIR_SEPS "/"
#define DIR_SEP  '/'
#define BRANDY_HAS_STRL_FUNCTIONS
#endif

#ifdef __linux__
#define TARGET_LINUX
#define TARGET_UNIX
#define BRANDY_OS "Linux"
#define LEGACY_OSVERSION 0xF9
#define MACTYPE   0x0800
#define EDITOR_VARIABLE "BRANDY_EDITOR"
#define DEFAULT_EDITOR  "vi"
#define DIR_SEPS "/"
#define DIR_SEP  '/'
#if defined(__GLIBC__) && ((__GLIBC__ > 2) || ((__GLIBC__ == 2) && (__GLIBC_MINOR__ >= 38)))
#define BRANDY_HAS_STRL_FUNCTIONS
#elif !defined(__GLIBC__)
#define _GNU_SOURCE
#define BRANDY_HAS_STRL_FUNCTIONS
#endif
#endif

#ifdef __minix
#define TARGET_MINIX
#define TARGET_UNIX
#define BRANDY_OS "Minix"
#define LEGACY_OSVERSION 0xF2
#define MACTYPE 0x0800
#define EDITOR_VARIABLE "BRANDY_EDITOR"
#define DEFAULT_EDITOR  "vi"
#define DIR_SEPS "/"
#define DIR_SEP  '/'
#define BRANDY_HAS_STRL_FUNCTIONS
#endif

#ifdef __midipix__
#define TARGET_MIDIPIX
#define TARGET_UNIX
#define BRANDY_OS "Midipix"
#define LEGACY_OSVERSION 0xFD
#define MACTYPE   0x0800
#define EDITOR_VARIABLE "BRANDY_EDITOR"
#define DEFAULT_EDITOR "vi"
#define DIR_SEPS "/"
#define DIR_SEP '/'
#endif

#ifdef __sun__
#define TARGET_SUNOS
#define TARGET_UNIX
#ifdef __illumos__
#define BRANDY_OS "illumos"
#else
#define BRANDY_OS "SunOS"
#endif /* illumos */
#define LEGACY_OSVERSION 0xF2
#define MACTYPE   0x0800
#define EDITOR_VARIABLE "BRANDY_EDITOR"
#define DEFAULT_EDITOR  "vi"
#define DIR_SEPS "/"
#define DIR_SEP  '/'
#define BRANDY_HAS_STRL_FUNCTIONS
#endif

/* Same as Linux, but can be treated exactly like it, see the Linux specific
 * XCASE in src/keyboard.c */
#ifdef __FreeBSD_kernel__
#define TARGET_GNUKFREEBSD
#define TARGET_UNIX
#define BRANDY_OS "GNU/kFreeBSD"
#define LEGACY_OSVERSION 0xF4
#define MACTYPE   0x0800
#define EDITOR_VARIABLE "BRANDY_EDITOR"
#define DEFAULT_EDITOR  "vi"
#define DIR_SEPS "/"
#define DIR_SEP  '/'
#if defined(__GLIBC__) && ((__GLIBC__ > 2) || ((__GLIBC__ == 2) && (__GLIBC_MINOR__ >= 38)))
#define BRANDY_HAS_STRL_FUNCTIONS
#endif
#endif

#ifdef __GNU__
#define TARGET_GNU
#define BRANDY_OS "GNU/Hurd"
#define LEGACY_OSVERSION 0xF3
#define MACTYPE   0x0800
#define EDITOR_VARIABLE "BRANDY_EDITOR"
#define DEFAULT_EDITOR  "vi"
#define DIR_SEPS "/"
#define DIR_SEP  '/'
#if defined(__GLIBC__) && ((__GLIBC__ > 2) || ((__GLIBC__ == 2) && (__GLIBC_MINOR__ >= 38)))
#define BRANDY_HAS_STRL_FUNCTIONS
#elif !defined(__GLIBC__)
#define _GNU_SOURCE
#define BRANDY_HAS_STRL_FUNCTIONS
#endif
#endif

#ifdef DJGPP
#define TARGET_DJGPP
#define TARGET_DOSWIN
#define BRANDY_OS "DJGPP"
#define LEGACY_OSVERSION 0xFA
#define MACTYPE   0x2000
#define EDITOR_VARIABLE "BRANDY$EDITOR"
#define DEFAULT_EDITOR  "edit"
#define DIR_SEPS "\\/:"
#define DIR_SEP  '\\'
#define BRANDY_HAS_STRL_FUNCTIONS
#define NOTEKGFX 1
#endif

#ifdef __MINGW32__
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0600 /* Require Win7 or later */
#define TARGET_MINGW
#define TARGET_DOSWIN
#define BRANDY_OS "MinGW"
#define LEGACY_OSVERSION 0xFC
#define MACTYPE   0x2000
#define EDITOR_VARIABLE "BRANDY$EDITOR"
#define DEFAULT_EDITOR  "edit"
#define DIR_SEPS "\\/:"
#define DIR_SEP  '\\'
#define NOTEKGFX 1
/* This is overridden if pthreads behaves itself */
#define MAXRECDEPTH 3072
#endif

#if defined(__LCC__) & defined(WIN32)
#define TARGET_WIN32
#define TARGET_DOSWIN
#define BRANDY_OS "LCC-WIN32"
#define LEGACY_OSVERSION 0xFC
#define MACTYPE   0x2000
#define EDITOR_VARIABLE "BRANDY$EDITOR"
#define DEFAULT_EDITOR  "edit"
#define DIR_SEPS "\\/:"
#define DIR_SEP  '\\'
#define NOTEKGFX 1
#endif

#ifdef __BORLANDC__
#define TARGET_BCC32
#define TARGET_DOSWIN
#define BRANDY_OS "BCC"
#define LEGACY_OSVERSION 0xFC
#define MACTYPE   0x2000
#define EDITOR_VARIABLE "BRANDY$EDITOR"
#define DEFAULT_EDITOR  "edit"
#define DIR_SEPS "\\/:"
#define DIR_SEP  '\\'
#define NOTEKGFX 1
#endif

#if defined(__GNUC__) && ( defined(__APPLE_CPP__) || defined(__APPLE_CC__) )
#define TARGET_MACOSX
#define TARGET_UNIX
#define BRANDY_OS "MacOS X"
#define LEGACY_OSVERSION 0xF8
#define MACTYPE   0x0A00
#define EDITOR_VARIABLE "BRANDY_EDITOR"
#define DEFAULT_EDITOR  "/Applications/TextEdit.app/Contents/MacOS/TextEdit"
#define DIR_SEPS "/"
#define DIR_SEP  '/'
#define BRANDY_HAS_STRL_FUNCTIONS
#endif

#if defined(_AMIGA) || defined(__amigaos__)
#define TARGET_AMIGA
#define BRANDY_OS "Amiga"
#define LEGACY_OSVERSION 0xF5
#define MACTYPE   0x0800
#define EDITOR_VARIABLE "BRANDY$EDITOR"
#define DEFAULT_EDITOR  "ed"
#define DIR_SEPS "/:"
#define DIR_SEP  '/'
#define NOTEKGFX 1
#endif

// Nothing tests for BEOS, but set up defines here to claim OSVER=&FB.
#ifdef __BEOS__
#define TARGET_BEOS
#define BRANDY_OS "BEOS"
#define LEGACY_OSVERSION 0xFB
// BEOS uses dir/file.ext filesystem, so MACTYPE must be %000x1xxx
// We use 8 because it is Unix-y
#define MACTYPE   0x0800
// Don't really know what the editor should be
#define EDITOR_VARIABLE "BRANDY$EDITOR"
#define DEFAULT_EDITOR  "vi"
#define DIR_SEPS "/"
#define DIR_SEP  '/'
#endif

#ifndef BRANDY_OS
#error Target operating system for interpreter is either missing or not supported
#endif

#ifdef USE_SDL
#define SFX1 "/SDL"
#else
#define SFX1 ""
#endif

#ifdef USE_ANSI
#define SFX2 "/ANSI"
#else
#define SFX2 ""
#endif

#if defined(TARGET_UNIX) && !defined(TARGET_MIDIPIX) && !defined(TARGET_MACOSX)
#define USE_X11
#endif

#ifdef BRANDY_NODISPLAYOS
#define IDSTRING "Matrix Brandy BASIC VI version " BRANDY_MAJOR "." BRANDY_MINOR "." BRANDY_PATCHLEVEL " (" BRANDY_DATE ")"
#else
#ifdef BRANDY_RELEASE
#define IDSTRING "Matrix Brandy BASIC VI version " BRANDY_MAJOR "." BRANDY_MINOR "." BRANDY_PATCHLEVEL "-Release (" BRANDY_OS "/" CPUTYPE SFX1 SFX2 ") " BRANDY_DATE
#else
#ifdef BRANDY_GITCOMMIT
#define IDSTRING "Matrix Brandy BASIC VI version " BRANDY_MAJOR "." BRANDY_MINOR "." BRANDY_PATCHLEVEL "+dev (" BRANDY_OS "/" CPUTYPE SFX1 SFX2 ") " BRANDY_DATE "\r\n\nDevelopment snapshot at git " BRANDY_GITBRANCH ":" BRANDY_GITCOMMIT " (" BRANDY_GITDATE ")"
#else
#define IDSTRING "Matrix Brandy BASIC VI version " BRANDY_MAJOR "." BRANDY_MINOR "." BRANDY_PATCHLEVEL "+dev (" BRANDY_OS "/" CPUTYPE SFX1 SFX2 ") " BRANDY_DATE "\r\n\nDevelopment snapshot"
#endif
#endif /* BRANDY_RELEASE */
#endif /* BRANDY_NODISPLAYOS */

/*
** MAXSTRING is the length of the longest string the interpreter
** allows. This value can be safely reduced but not increased
** without altering the string memory allocation code in strings.c
** 1024 is probably a sensible minimum value
*/

#define MAXSTRING 65536

#ifndef MAXRECDEPTH
#define MAXRECDEPTH 4096
#endif

#ifdef USE_SDL
#define MODE7FB 0x7C00
#endif

/*
** The ALIGN macro is used to control the sizes of blocks of
** memory allocated from the heap. They are always a multiple
** of ALIGN bytes.
*/
#ifdef TARGET_HPUX
#define ALIGN(x) ((x+sizeof(double)-1) & -(int)sizeof(double))
#else
#define ALIGN(x) ((x+sizeof(size_t)-1) & -(int)sizeof(size_t))
#endif

#if defined(TARGET_MINGW) || defined(__TARGET_SCL__)
#include <setjmp.h>
#define sigsetjmp(env, savesigs) __builtin_setjmp(env)
#define siglongjmp(env, val) __builtin_longjmp(env, val)
typedef jmp_buf sigjmp_buf;
#endif /* TARGET_MINGW || __TARGET_SCL__ */

/* OpenBSD recommends strlcpy() and friends over strncpy(), but it's not
 * available on all systems. So macro it to resolve as strlcpy where available
 * and an internal implementation on others. Also, strlcat() takes into account
 * the current destination string length, strncat() does not. */
#ifdef BRANDY_HAS_STRL_FUNCTIONS
#define STRLCPY(x,y,z) strlcpy(x,y,z)
#define STRLCAT(x,y,z) strlcat(x,y,z)
#else
#define STRLCPY(x,y,z) my_strlcpy(x,y,z)
#define STRLCAT(x,y,z) strncat(x,y,z - strlen(x) -1)
#endif /* BRANDY_HAS_STRL_FUNCTIONS */

#ifdef __TARGET_SCL__
/* FIXME: Implement this properly! */
#define usleep(x)
#endif

#ifdef TARGET_RISCOS
#define MAXSYSPARMS 10          /* Maximum number of parameters allowed in a 'SYS' statement */
#else
#define MAXSYSPARMS 16          /* Maximum number of parameters allowed in a 'SYS' statement */
#endif

#endif
