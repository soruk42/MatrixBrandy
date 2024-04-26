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
**      This file contains the file I/O routines for the interpreter
*/

/*
** The functions here map the Basic VI file I/O facilities on to those
** provided by the underlying operating system.
*/

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include "common.h"
#include "miscprocs.h"
#include "target.h"
#include "errors.h"
#include "fileio.h"
#include "strings.h"
#include "screen.h"
#ifndef NONET
#include "net.h"
#endif
#include "keyboard.h"


/* Floating point number format */
enum {XMIXED_ENDIAN, XLITTLE_ENDIAN, XBIG_ENDIAN, XBIG_MIXED_ENDIAN} double_type;

typedef enum {CLOSED, OPENIN, OPENUP, OPENOUT, NETWORK} filestate;

typedef enum {OKAY, PENDING, ATEOF} eofstate;

typedef struct {
  FILE *stream;                 /* 'C' file handle for the file */
  filestate filetype;           /* Way in which file has been opened */
  eofstate eofstatus;           /* Current end-of-file status */
  boolean lastwaswrite;         /* TRUE if the last operation on a file was a write */
  int nethandle;                /* network handle */
} fileblock;

#ifdef TARGET_RISCOS
#define MAXFILES 4              /* Maximum number of files that can be open simultaneously - only tracks networking on RISC OS */
#define FIRSTHANDLE 4           /* Number of first handle */
#else
#define MAXFILES 25             /* Maximum number of files that can be open simultaneously */
#define FIRSTHANDLE 254         /* Number of first handle */
#endif

static fileblock fileinfo [MAXFILES+1];

#ifdef TARGET_RISCOS

/* ================================================================= */
/* ================= RISC OS versions of functions ================= */
/* ================================================================= */

#include "kernel.h"
#include "swis.h"
#ifndef _kernel_ERROR
#define _kernel_ERROR (-2)
#endif

/*
** Under RISC OS, direct operating system calls such as OS_Find are used
** for file I/O so that Basic programs can use a mix of Basic statements
** and SWIs to carry out file I/O. The code does not keep track of what
** files have been opened as it possible to do such things as open a
** file using the SWI OS_Find directly and then read it using the Basic
** function BGET.
*/

/* OS_Find calls */

#define OPEN_INPUT 0x40
#define OPEN_OUTPUT 0x80
#define OPEN_UPDATE 0xC0

boolean isapath(char *name) {
  return strpbrk(name, DIR_SEPS)!=NIL;
}

/*
** 'report' is called when one of the _kernel_xxx functions
** returns -2 (indicating that the corresponding SWI call
** failed)
*/
static void report(void) {
  _kernel_oserror *oserror;
  oserror = _kernel_last_oserror();
  error(ERR_CMDFAIL, oserror->errmess);
}

/*
** 'read' reads a byte from a file, dealing with any error
** conditions that might arise
*/
static int32 fileio_read(int32 handle) {
  int32 value;
  value = _kernel_osbget(handle);
  if (value==_kernel_ERROR) report();   /* Function returned -2 = SWI call failed */
  if (value==-1) error(ERR_CANTREAD);
  return value;
}

/*
** 'fileio_openin' opens a file for input
*/
int32 fileio_openin(char *name, int32 namelen) {
  int32 handle;
  char filename [FNAMESIZE];
  char *tfilename;
  memmove(filename, name, namelen);
  filename[namelen] = NUL;              /* Get a null-terminated version of the file name */
  handle = _kernel_osfind(OPEN_INPUT, filename);
  if (handle==_kernel_ERROR) {
    if (matrixflags.translatefname == 0) {
      report();
    } else {
      tfilename=translatefname(filename);
      handle = _kernel_osfind(OPEN_INPUT, tfilename);
      if (handle==_kernel_ERROR) {
        report();
      } else {
        /* We had to translate so set flags to always translate on save */
        matrixflags.translatefname = 1;
      }
    }
  }
  return handle;
}

/*
** 'fileio_openout' opens file 'name' for output
*/
int32 fileio_openout(char *name, int32 namelen) {
  int32 handle;
  char filename [FNAMESIZE];
  char *tfilename;
  memmove(filename, name, namelen);
  filename[namelen] = NUL;
  if(matrixflags.translatefname == 1) {
    tfilename=translatefname(filename);
    handle = _kernel_osfind(OPEN_OUTPUT, tfilename);
  } else {
    handle = _kernel_osfind(OPEN_OUTPUT, filename);
  }
  if (handle==_kernel_ERROR) report();
  return handle;
}

/*
** 'fileio_openup' opens a file for both input and output
*/
int32 fileio_openup(char *name, int32 namelen) {
  int32 handle;
#ifndef NONET
  int32 n;
#endif
  char filename [FNAMESIZE];
  char *tfilename;
  memmove(filename, name, namelen);
  filename[namelen] = NUL;
#ifndef NONET
  /* Check, does it start "ip4:" if so use network handler to open it. */
  if (strncmp(filename, "ip0:", 4)==0 || strncmp(filename, "ip4:", 4)==0 || strncmp(filename, "ip6:", 4)==0) {
    for (n=FIRSTHANDLE; n>0 && fileinfo[n].stream!=NIL; n--);   /* Find an unused handle */
    handle=brandynet_connect(filename+4, filename[2], 1);
    if (handle == -1) return 0;
    fileinfo[n].stream = (void *)42; /* Not used, but != NIL */
    fileinfo[n].filetype = NETWORK;
    fileinfo[n].eofstatus = OKAY;
    fileinfo[n].lastwaswrite = FALSE;
    fileinfo[n].nethandle = handle;
    return n;
  } else {
#endif
    handle = _kernel_osfind(OPEN_UPDATE, filename);
    if (handle==_kernel_ERROR) {
      if (matrixflags.translatefname == 0) {
        report();
      } else {
        tfilename=translatefname(filename);
        handle = _kernel_osfind(OPEN_INPUT, tfilename);
        if (handle==_kernel_ERROR) {
          report();
        } else {
          /* We had to translate so set flags to always translate on save */
          matrixflags.translatefname = 1;
        }
      }
    }
    return handle;
#ifndef NONET
  }
#endif
}

/*
** 'fileio_close' closes the fie given by 'handle' or all open files
** if 'handle' is zero
*/
static void close_file(int32 handle) {
#ifndef NONET
  if ((handle != 0) && (handle <= FIRSTHANDLE) && (fileinfo[handle].filetype == NETWORK)) {
    brandynet_close(fileinfo[handle].nethandle);
    fileinfo[handle].stream = NIL;
    fileinfo[handle].filetype = CLOSED;
    fileinfo[handle].lastwaswrite = FALSE;
    fileinfo[handle].nethandle = -1;
  } else {
#endif
    _kernel_oserror *oserror;
    _kernel_swi_regs regs;
    regs.r[0] = 0;
    regs.r[1] = handle;
    oserror = _kernel_swi(OS_Find, &regs, &regs);
    if (oserror!=NIL) error(ERR_CMDFAIL, oserror->errmess);
#ifndef NONET
  }
#endif
}

void fileio_close(int32 handle) {
  int32 n;
  if (handle==0) {      /* Close all open files */
    for (n=FIRSTHANDLE; n>0; n--) {
      if (fileinfo[n].filetype!=CLOSED) close_file(n);
    }
    close_file(handle);
  }
  else {        /* close one file */
    close_file(handle);
  }
}

/*
** 'fileio_bget' returns the next character from file 'handle'.
*/
int32 fileio_bget(int32 handle) {
  int32 ch = 0;
  if (handle==0) error(ERR_BADHANDLE);

#ifndef NONET
  if ((handle <= FIRSTHANDLE) && (fileinfo[handle].filetype == NETWORK)) {
    ch=net_bget(fileinfo[handle].nethandle);
    if (ch == -2) {
      if (fileinfo[handle].eofstatus == PENDING) {
        fileinfo[handle].eofstatus = ATEOF;
        error(ERR_HITEOF);
      } else {
        fileinfo[handle].eofstatus = PENDING;
      }
    }
  } else {
#endif
    _kernel_oserror *oserror;
    _kernel_swi_regs regs;
    regs.r[0] = 0;
    regs.r[1] = handle;
    oserror = _kernel_swi(OS_BGet, &regs, &regs);
    if (oserror!=NIL) error(ERR_CMDFAIL, oserror->errmess);
    ch=regs.r[0];
#ifndef NONET
  }
#endif
  return ch;

}

/*
** 'fileio_getdol' reads a string from a file. It saves the text read at
** 'buffer'. Note that there is no check on the size of the buffer
** so it is up to the functions that call this one to ensure that the
** buffer is large enough to hold MAXSTRING (65536) characters.
*/
int32 fileio_getdol(int32 handle, char *buffer) {
  int32 ch, length;
  length = 0;
  do {
    ch = fileio_bget(handle);
    if (ch==_kernel_ERROR) report();    /* Function returned -2 = SWI call failed */
    if (ch==-1 || ch==LF) break;        /* At end of file or reached end of line */
    buffer[length] = ch;
    length++;
  } while (TRUE);
  return length;
}

/*
** 'fileio_getnumber' reads a binary number from the file with
** handle 'handle'. It stores the result at the address given
** by 'ip' if integer or at 'fp' is floating point. 'isint' is
** set to TRUE if the value is an integer.
**
** Note that integers are stored in big endian format in the file
** by PRINT#. Floating point values are written in the byte order
** used in memory but this causes its own problems as the ARM
** format for eight-byte floating point values is different to
** that used on other processors meaning that the RISC OS and
** NetBSD/arm32 versions of the program are not compatible with
** other versions. Also, this code will only read eight-byte
** floating point values produced by this program or basic64.
** It does not handle numbers in Acorn's five byte format.
*/
void fileio_getnumber(int32 handle, boolean *isint, int64 *ip, float64 *fp) {
  int32 n, marker;
  char temp[sizeof(float64)];
  memset(temp,0,sizeof(float64));
  marker = fileio_read(handle);
  switch (marker) {
  case PRINT_INT:
    for (n=1; n<=sizeof(int32); n++) temp[sizeof(int32)-n] = fileio_read(handle);
    memmove(ip, temp, sizeof(int32));
    *isint = TRUE;
    break;
  case PRINT_UINT8:
    for (n=1; n<=sizeof(uint8); n++) temp[sizeof(uint8)-n] = fileio_read(handle);
    memmove(ip, temp, sizeof(uint8));
    *isint = TRUE;
    break;
  case PRINT_INT64:
    for (n=1; n<=sizeof(int64); n++) temp[sizeof(int64)-n] = fileio_read(handle);
    memmove(ip, temp, sizeof(int64));
    *isint = TRUE;
    break;
  case PRINT_FLOAT:
    for (n=0; n<sizeof(float64); n++) temp[n] = fileio_read(handle);
    memmove(fp, temp, sizeof(float64));
    *isint = FALSE;
    break;
  case PRINT_FLOAT5: {          /* Acorn 5-byte floating point format */
/*
** Acorn's five byte format consists of a four byte mantissa and
** a one byte exponent. The mantissa, x, is in little endian format
** and represents a number in the range 0.5<=x<1.0. The value is
** such that x times 2 raised to the power y (where y is the
** exponent) gives the floating point value. The most significant
** bit will always be one, so it is used as the sign bit. The
** exponent has 0x80 added to it.
*/
    int32 exponent;
    int32 mantissa = fileio_read(handle);
    mantissa |= fileio_read(handle) << 8;
    mantissa |= fileio_read(handle) << 16;
    mantissa |= fileio_read(handle) << 24;
    exponent = fileio_read(handle);
    if (exponent || mantissa) {
      *fp = ((float64)(mantissa & 0x7FFFFFFF) / 4294967296.0 + 0.5)
            * pow (2, (float64)(exponent - 0x80))
            * (mantissa < 0 ? -1 : 1);
    } else {
      *fp = 0;
    }
    *isint = FALSE;
    break;
  }
  default:
    error(ERR_TYPENUM);
  }
}

/*
** 'fileio_getstring' reads a string from from a file and returns
** the length of the string read. The string is stored at address
** 'p'. It assumes that there is enough room to store the string.
** The function can handle string in both Acorn format and this
** interpreter's. In Acorn's format, strings can be up to 255
** characters long. They are stored in the file in reverse order,
** that is, the last character of the string is first.
*/
int32 fileio_getstring(int32 handle, char *p) {
  int32 marker = 0, length = 0, n = 0;
  marker = fileio_read(handle);
  switch (marker) {
  case PRINT_SHORTSTR:  /* Reading short string in 'Acorn' format */
    length = fileio_read(handle);
    for (n=1; n<=length; n++) p[length-n] = fileio_read(handle);
    break;
  case PRINT_LONGSTR:   /* Reading long string */
    length = 0;         /* Start by reading the string length (four bytes, little endian) */
    for (n=0; n<sizeof(int32); n++) length+=fileio_read(handle)<<(n*BYTESHIFT);
    for (n=0; n<length; n++) p[n] = fileio_read(handle);
    break;
  default:
    error(ERR_TYPESTR);
  }
  return length;
}

/*
** 'fileio_bput' writes a character to a file
*/
void fileio_bput(int32 handle, int32 value) {
  if (handle==0) error(ERR_BADHANDLE);
#ifndef NONET
  if ((handle <= FIRSTHANDLE) && (fileinfo[handle].filetype==NETWORK)) {
    if(net_bput(fileinfo[handle].nethandle, value)) error(ERR_CANTWRITE);
  } else {
#endif
    _kernel_oserror *oserror;
    _kernel_swi_regs regs;
    regs.r[0] = value;
    regs.r[1] = handle;
    oserror = _kernel_swi(OS_BPut, &regs, &regs);
    if (oserror!=NIL) error(ERR_CMDFAIL, oserror->errmess);
#ifndef NONET
  }
#endif
}

/*
** 'fileio_bputstr' writes a string to a file
*/
void fileio_bputstr(int32 handle, char *string, int32 length) {
  if (handle==0) error(ERR_BADHANDLE);
#ifndef NONET
  if ((handle <= FIRSTHANDLE) && (fileinfo[handle].filetype==NETWORK)) {
    if(net_bputstr(fileinfo[handle].nethandle, string, length)) error(ERR_CANTWRITE);
  } else {
#endif
    _kernel_oserror *oserror;
    _kernel_swi_regs regs;
    regs.r[0] = 2;      /* OS_GBPB 2 = write to file at current file pointer position */
    regs.r[1] = handle;
    regs.r[2] = TOINT((int)string);
    regs.r[3] = length;
    oserror = _kernel_swi(OS_GBPB, &regs, &regs);
    if (oserror!=NIL) error(ERR_CMDFAIL, oserror->errmess);
    if (regs.r[3]!=0) error(ERR_CANTWRITE);     /* Not all the data was written to the file */
#ifndef NONET
  }
#endif
}

/*
** 'fileio_printint' writes a four byte integer to a file in binary
** preceded with 0x40 to mark it as an integer.
** The number has to be written in 'big endian' format for compatibility
** with the Acorn interpreter
*/
void fileio_printint(int32 handle, int32 value) {
  int32 n;
  char temp[sizeof(int32)];
  fileio_bput(handle, PRINT_INT);
  memmove(temp, &value, sizeof(int32));
  for (n=1; n<=sizeof(int32); n++) fileio_bput(handle, temp[sizeof(int32)-n]);
}

void fileio_printuint8(int32 handle, uint8 value) {
  fileio_bput(handle, PRINT_UINT8);
  fileio_bput(handle, value);
}

void fileio_printint64(int32 handle, int64 value) {
  int32 n;
  char temp[sizeof(int64)];
  fileio_bput(handle, PRINT_INT64);
  memmove(temp, &value, sizeof(int64));
  for (n=1; n<=sizeof(int64); n++) fileio_bput(handle, temp[sizeof(int64)-n]);
}

/*
** 'fileio_printfloat' writes an eight byte floating point value to a file
** in binary preceded with 0x88 to mark it as floating point.
*/
void fileio_printfloat(int32 handle, float64 value) {
  char temp[sizeof(float64)];
  fileio_bput(handle, PRINT_FLOAT);
  memmove(temp, &value, sizeof(float64));
  fileio_bputstr(handle, temp, sizeof(float64));
}

/*
** 'fileio_printstring' writes a string to a file. If the length of the
** string is less than 256 bytes it is written in 'Acorn' format, that
** is, preceded with 0 and a single byte length with the string in
** reverse order. If longer than 255 bytes, the interpreter uses its own
** format. The string is preceded with 0x01 and its length written in
** four bytes. The string is written in 'true' order
*/
void fileio_printstring(int32 handle, char *string, int32 length) {
  int32 n, temp;
  if (length<SHORT_STRING) {    /* Write string in Acorn format */
    fileio_bput(handle, PRINT_SHORTSTR);
    fileio_bput(handle, length);
    if (length>0) for (n=length-1; n>=0; n--) fileio_bput(handle, string[n]);
  }
  else {        /* Long string - Use interpreter's extended format */
    fileio_bput(handle, PRINT_LONGSTR);
    temp = length;
    for (n=0; n<sizeof(int32); n++) {   /* Write four byte length to file */
      fileio_bput(handle, temp & BYTEMASK);
      temp = temp>>BYTESHIFT;
    }
    fileio_bputstr(handle, string, length);
  }
}

/*
** 'fileio_getptr' returns the current value of the file pointer for
** the file 'handle'
*/
int32 fileio_getptr(int32 handle) {
  int32 pointer;
  if (handle == 0) return 0;
  pointer = _kernel_osargs(0, handle, 0);       /* OS_Args 0 = read file pointer */
  if (pointer==_kernel_ERROR) report();
  return pointer;
}

/*
** 'fileio_setptr' is used to set the current file pointer of
** the file 'handle'
*/
void fileio_setptr(int32 handle, int64 newoffset) {
  int32 result;
  result = _kernel_osargs(1, handle, newoffset);        /* OS_Args 1 = set file pointer */
  if (result==_kernel_ERROR) report();
}

/*
** 'fileio_getext' returns the size of the file with handle 'handle'
*/
int64 fileio_getext(int32 handle) {
  int32 extent;
  extent = _kernel_osargs(2, handle, 0);        /* OS_Args 2 = read file's extent (size) */
  if (extent==_kernel_ERROR) report();
  return extent;
}

/*
** 'fileio_setext' is called to alter the extent (size) of the file
** with handle 'handle'
*/
void fileio_setext(int32 handle, int32 newsize) {
  int32 result;
  result = _kernel_osargs(3, handle, newsize);  /* OS_Args 3 = alter file's extent (size) */
  if (result==_kernel_ERROR) report();
}

/*
** 'fileio_eof' returns the current end-of-file state of file 'handle',
** returning 'TRUE' if it is at end-of-file.
*/
int32 fileio_eof(int32 handle) {
  int32 result;
  result = _kernel_osargs(5, handle, 0);        /* OS_Args 5 = Check end-of-file status */
  if (result==_kernel_ERROR) report();
  return result==0 ? FALSE : TRUE;
}

/*
** 'fileio_shutdown' is called at the end of the run of the
** interpreter. This is not required under RISC OS
*/
void fileio_shutdown(void) {
}

/*
** 'init_fileio' is called to initialise the file handling. This
** step is not required under RISC OS
*/
void init_fileio(void) {
}

#else /* !TARGET_RISCOS */

/* ================================================================== */
/* ============= NetBSD/Linux/DOS versions of functions ============= */
/* ================================================================== */

/*
** Files are opened in character mode under RISC OS, Linux and NetBSD
** as there is basically no difference between binary and character
** modes with these operating systems. This is what we want as Basic
** does not distinguish between these two types of file. Unfortunately
** DOS has to be different. In character mode the DOS libraries I have
** seen to like to stick 'carriage return' characters in the output
** stream whenever they see a 'linefeed' when writing to a file. Under
** DOS files are always treated as binary.
*/
#if defined(TARGET_DOSWIN)
#define INMODE "rb"
#define OUTMODE "wb+"
#define UPMODE "rb+"
#else
#define INMODE "r"
#define OUTMODE "w+"
#define UPMODE "r+"
#endif

/*
** 'isapath' returns TRUE if the file name passed to it is a pathname, that
** is, contains directories as well as a file name, and FALSE if it consists
** of just the name of the file. 'DIRSEPS' is a string containing all the
** characters that can be used in a file name to separate directories or
** the name of the file from the directories.
** This code is meant to be operating system independent
*/
boolean isapath(char *name) {
  return strpbrk(name, DIR_SEPS)!=NIL;
}

/*
** 'map_handle' maps a Basic-style file handle to the corresponding entry
** in the 'fileinfo' table and checks that the handle is valid
*/
static int32 map_handle(int32 handle) {
  handle = FIRSTHANDLE-handle;
  if (handle<0 || handle>=MAXFILES || fileinfo[handle].filetype==CLOSED) error(ERR_BADHANDLE);
  return handle;
}

/*
** 'fileio_openin' opens a file for input
*/
int32 fileio_openin(char *name, int32 namelen) {
  FILE *thefile;
  int32 n;
  char filename [FNAMESIZE];
  char filenameb[FNAMESIZE];
  char *tfilename;
  if ((namelen < 0) || (namelen > (FNAMESIZE - 1))) error(ERR_INVALIDFNAME);
  for (n=0; n<MAXFILES && fileinfo[n].stream!=NIL; n++);        /* Find an unused handle */
  if (n>=MAXFILES) error(ERR_MAXHANDLE);
  memmove(filename, name, namelen);
  filename[namelen] = asc_NUL;
  thefile = fopen(filename, INMODE);
  if (thefile==NIL) {
    strncpy(filenameb, filename, FNAMESIZE);
    strcat(filenameb, ".bbc"); /* Append a .bbc suffix and try again */
    thefile = fopen(filenameb, INMODE);
    if (thefile==NIL) {
      if (matrixflags.translatefname == 0) {
        return 0;
      } else {
        tfilename=translatefname(filename);
        thefile = fopen(tfilename, INMODE);
        if (thefile==NIL) {
          return 0; /* Could not open file - Return null handle */
        } else {
          /* We had to translate so set flags to always translate on save */
          matrixflags.translatefname = 1;
        }
      }
    }
  }
  fileinfo[n].stream = thefile;
  fileinfo[n].filetype = OPENIN;
  fileinfo[n].eofstatus = OKAY;
  fileinfo[n].lastwaswrite = FALSE;
  return FIRSTHANDLE-n;
}

/*
** 'fileio_openout' opens file 'name' for output, creating the
** file (or recreating it if it already exists).
** Note that the file is opened with mode "w+" so that it can be
** both written to and read from. RISC OS allows files opened for
** output to be read from as well
*/
int32 fileio_openout(char *name, int32 namelen) {
  FILE *thefile;
  int32 n;
  char filename [FNAMESIZE];
  char *tfilename;
  if ((namelen < 0) || (namelen > (FNAMESIZE - 1))) error(ERR_INVALIDFNAME);
  for (n=0; n<MAXFILES && fileinfo[n].stream!=NIL; n++);        /* Find an unused handle */
  if (n>=MAXFILES) error(ERR_MAXHANDLE);
  memmove(filename, name, namelen);
  filename[namelen] = asc_NUL;
  if(matrixflags.translatefname == 1) {
    tfilename=translatefname(filename);
    thefile = fopen(tfilename, OUTMODE);
  } else {
    thefile = fopen(filename, OUTMODE);
  }
  if (thefile==NIL) error(ERR_OPENWRITE, filename);
  fileinfo[n].stream = thefile;
  fileinfo[n].filetype = OPENOUT;
  fileinfo[n].eofstatus = OKAY;
  fileinfo[n].lastwaswrite = FALSE;
  return FIRSTHANDLE-n;
}

/*
** 'fileio_openup' opens a file for update. The file must exist
** already. The file can both be read from and written to
*/
int32 fileio_openup(char *name, int32 namelen) {
  FILE *thefile;
  int32 n;
  char filename [FNAMESIZE];
  char filenameb[FNAMESIZE];
  char *tfilename;

  if ((namelen < 0) || (namelen > (FNAMESIZE - 1))) error(ERR_INVALIDFNAME);
  for (n=0; n<MAXFILES && fileinfo[n].stream!=NIL; n++);        /* Find an unused handle */
  if (n>=MAXFILES) error(ERR_MAXHANDLE);
  memmove(filename, name, namelen);
  filename[namelen] = asc_NUL;
#ifndef NONET
  /* Check, does it start "ip4:" if so use network handler to open it. */
  if (!strncmp(filename, "ip0:", 4) || !strncmp(filename, "ip4:", 4) || !strncmp(filename, "ip6:", 4)) {
    int handle;
    handle=brandynet_connect(filename+4, filename[2], 1);
    if (handle == -1) return 0;
    fileinfo[n].stream = (void *)42; /* Not used, but != NIL */
    fileinfo[n].filetype = NETWORK;
    fileinfo[n].eofstatus = OKAY;
    fileinfo[n].lastwaswrite = FALSE;
    fileinfo[n].nethandle = handle;
    return FIRSTHANDLE-n;
  } else {
#endif
    thefile = fopen(filename, UPMODE);
    if (thefile==NIL) {
      strncpy(filenameb, filename, FNAMESIZE);
      strcat(filenameb, ".bbc"); /* Append a .bbc suffix and try again */
      thefile = fopen(filenameb, UPMODE);
      if (thefile==NIL) {
        if (matrixflags.translatefname == 0) {
          return 0;
        } else {
          tfilename=translatefname(filename);
          thefile = fopen(tfilename, UPMODE);
          if (thefile==NIL) {
            return 0; /* Could not open file - Return null handle */
          } else {
            /* We had to translate so set flags to always translate on save */
            matrixflags.translatefname = 1;
          }
        }
      }
    }
    fileinfo[n].stream = thefile;
    fileinfo[n].filetype = OPENUP;
    fileinfo[n].eofstatus = OKAY;
    fileinfo[n].lastwaswrite = FALSE;
    return FIRSTHANDLE-n;
#ifndef NONET
  }
#endif
}

/*
** 'close_file' is a function used locally to close a file or network channel
*/
static void close_file(int32 handle) {
#ifndef NONET
  if ((handle <= FIRSTHANDLE) && (fileinfo[handle].filetype == NETWORK)) {
    brandynet_close(fileinfo[handle].nethandle);
    fileinfo[handle].stream = NIL;
    fileinfo[handle].filetype = CLOSED;
    fileinfo[handle].lastwaswrite = FALSE;
    fileinfo[handle].nethandle = -1;
  } else {
#endif
    fclose(fileinfo[handle].stream);
    fileinfo[handle].stream = NIL;
    fileinfo[handle].filetype = CLOSED;
    fileinfo[handle].lastwaswrite = FALSE;
#ifndef NONET
  }
#endif
}

/*
** 'fileio_close' closes the file given by 'handle' or all open files
** if 'handle' is zero
*/
void fileio_close(int32 handle) {
  int32 n;
  if (handle==0) {      /* Close all open files */
    for (n=0; n<MAXFILES; n++) {
      if (fileinfo[n].filetype!=CLOSED) close_file(n);
    }
  }
  else {        /* close one file */
    n = map_handle(handle);
    if (fileinfo[n].filetype!=CLOSED) close_file(n);
  }
}

/*
** 'fileio_bget' returns the next character from file 'handle'.
** Note that RISC OS allows you to read from a file that has been opened for
** writing. One byte can be read. The next attempt to read anything will
** result in an end-of-file error.
*/
int32 fileio_bget(int32 handle) {
  int32 ch;

  if (handle==0) error(ERR_BADHANDLE);
  handle = map_handle(handle);
#ifndef NONET
  if (fileinfo[handle].filetype == NETWORK) {
    ch=net_bget(fileinfo[handle].nethandle);
    if (ch == -2) {
      if (fileinfo[handle].eofstatus == PENDING) {
        fileinfo[handle].eofstatus = ATEOF;
        error(ERR_HITEOF);
      } else {
        fileinfo[handle].eofstatus = PENDING;
      }
    }
  } else {
#endif
    if (fileinfo[handle].eofstatus!=OKAY) {     /* If EOF is pending, flag an error */
      fileinfo[handle].eofstatus = ATEOF;
      error(ERR_HITEOF);
    }
    else if (fileinfo[handle].filetype==OPENOUT) {      /* If file is open for output, read one char */
      ch = asc_NUL;
      fileinfo[handle].eofstatus = PENDING;
    }
    if (fileinfo[handle].lastwaswrite) {                /* Ensure everything has been written to disk first */
      fflush(fileinfo[handle].stream);
      fileinfo[handle].lastwaswrite = FALSE;
    }
    ch = fgetc(fileinfo[handle].stream);        /* Read a character */
    if (ch==EOF) {
      fileinfo[handle].eofstatus = PENDING;     /* If call returns 'EOF' set 'PENDING EOF' flag */
      ch = 0;
    }
    fileinfo[handle].lastwaswrite = FALSE;
#ifndef NONET
  }
#endif
  return ch;
}

/*
** 'fileio_getdol' reads a string from a file. It saves the text read at
** 'buffer'. Any terminating line end characters are removed. Both
** 'carriage return-linefeed' and 'linefeed' style line ends are recognised.
** The function returns the number of characters read (minus line end
** characters). Note that there is no check on the size of the buffer
** so it is up to the functions that call this one to ensure that the
** buffer is large enough to hold MAXSTRING (65536) characters.
*/
int32 fileio_getdol(int32 handle, char *buffer) {
  char *p;
  int32 length;

  if (handle==0) error(ERR_BADHANDLE);
  handle = map_handle(handle);
  if (fileinfo[handle].eofstatus!=OKAY) {       /* If EOF is pending or EOF, flag an error */
    fileinfo[handle].eofstatus = ATEOF;
    error(ERR_HITEOF);
  }
  if (fileinfo[handle].lastwaswrite) {          /* Ensure everything has been written to disk first */
    fflush(fileinfo[handle].stream);
    fileinfo[handle].lastwaswrite = FALSE;
  }
  p = fgets(buffer, MAXSTRING, fileinfo[handle].stream);
  if (p==NIL) error(ERR_CANTREAD);      /* Read failed utterly */
  length = strlen(buffer);
  p = buffer+length-1;  /* Point at line end character */
  if (*p==asc_LF) {     /* Got a 'linefeed' at the end of the line */
    length--;
    if (length>0 && *(p-1)==asc_CR) length--;   /* Got a 'carriage return-linefeed' pair */
  }
  return length;
}

static int32 fileio_read(FILE *handle) {
  int32 ch;
  ch = fgetc(handle);
  if (ch==EOF) error(ERR_CANTREAD);
  return ch;
}

/*
** 'fileio_getnumber' reads a binary number from the file with
** handle 'handle'. It stores the result at the address given
** by 'ip' if integer or at 'fp' is floating point. 'isint' is
** set to TRUE if the value is an integer.
**
** Note that integers are stored in big endian format in the file
** by PRINT#. Floating point values are written in the byte order
** used by the Acorn interpreter under RISC OS. This means that
** some fiddling about is needed to make sure the bytes of the
** value are in the right order under other operating systems and
** other types of hardware. The code to do this was written by
** Darren Salt, along with the support for reading Acorn's five
** byte floating point format
*/
void fileio_getnumber(int32 handle, boolean *isint, int64 *ip, float64 *fp) {
  FILE *stream;
  int32 n, marker;
  char temp[sizeof(float64)];

  memset(temp,0,sizeof(float64));
  if (handle==0) error(ERR_BADHANDLE);
  handle = map_handle(handle);
  if (fileinfo[handle].eofstatus!=OKAY) {       /* If EOF is pending, flag an error */
    fileinfo[handle].eofstatus = ATEOF;
    error(ERR_HITEOF);
  }
  if (fileinfo[handle].lastwaswrite) {  /* Ensure everything has been written to disk first */
    fflush(fileinfo[handle].stream);
    fileinfo[handle].lastwaswrite = FALSE;
  }
  stream = fileinfo[handle].stream;
  marker = fileio_read(stream);
  switch (marker) {
  case PRINT_INT:
    *ip = 0;
    for (n=24; n>=0; n-=8) *ip |= fileio_read(stream) << n;
    *isint = TRUE;
    break;
  case PRINT_UINT8:
    *ip = fileio_read(stream);
    *isint = TRUE;
    break;
  case PRINT_INT64:
    *ip = 0;
    for (n=56; n>=0; n-=8) *ip |= (int64)fileio_read(stream) << n;
    *isint = TRUE;
    break;
  case PRINT_FLOAT:
    switch (double_type) {
    case XMIXED_ENDIAN:
      for (n=0; n<sizeof(float64); n++) temp[n] = fileio_read(stream);
      break;
    case XLITTLE_ENDIAN:
      for (n=0; n<sizeof(float64); n++) temp[n^4] = fileio_read(stream);
      break;
    case XBIG_ENDIAN:
      for (n=0; n<sizeof(float64); n++) temp[n^3] = fileio_read(stream);
      break;
    case XBIG_MIXED_ENDIAN:
      for (n=0; n<sizeof(float64); n++) temp[n^7] = fileio_read(stream);
    }
    memmove(fp, temp, sizeof(float64));
    *isint = FALSE;
    break;
  case PRINT_FLOAT5: { /* Acorn's five byte format */
    int32 exponent;
    int32 mantissa = fileio_read(stream);
    mantissa |= fileio_read(stream) << 8;
    mantissa |= fileio_read(stream) << 16;
    mantissa |= fileio_read(stream) << 24;
    exponent = fileio_read(stream);
    if (exponent || mantissa) {
      *fp = ((mantissa & 0x7FFFFFFF) / 4294967296.0 + 0.5)
            * pow (2, exponent - 0x80)
            * (mantissa < 0 ? -1 : 1);
    } else {
      *fp = 0;
    }
    *isint = FALSE;
    break;
  }
  default:
    error(ERR_TYPENUM);
  }
}

/*
** 'fileio_getstring' reads a string from from a file and returns
** the length of the string read. The string is stored at address
** 'p'. It assumes that there is enough room to store the string.
** The function can handle string in both Acorn format and this
** interpreter's. In Acorn's format, strings can be up to 255
** characters long. They are stored in the file in reverse order,
** that is, the last character of the string is first.
*/
int32 fileio_getstring(int32 handle, char *p) {
  FILE *stream;
  int32 marker, length = 0, n;

  if (handle==0) error(ERR_BADHANDLE);
  handle = map_handle(handle);
  if (fileinfo[handle].eofstatus!=OKAY) {       /* If EOF is pending, flag an error */
    fileinfo[handle].eofstatus = ATEOF;
    error(ERR_HITEOF);
  }
  if (fileinfo[handle].lastwaswrite) {          /* Ensure everything has been written to disk first */
    fflush(fileinfo[handle].stream);
    fileinfo[handle].lastwaswrite = FALSE;
  }
  stream = fileinfo[handle].stream;
  marker = fileio_read(stream);
  switch (marker) {
  case PRINT_SHORTSTR:  /* Reading short string in 'Acorn' format */
    length = fileio_read(stream);
    for (n=1; n<=length; n++) p[length-n] = fileio_read(stream);
    break;
  case PRINT_LONGSTR:   /* Reading long string */
    length = 0;         /* Start by reading the string length (four bytes, little endian) */
    for (n=0; n<sizeof(int32); n++) length+=fileio_read(stream)<<(n*BYTESHIFT);
    for (n=0; n<length; n++) p[n] = fileio_read(stream);
    break;
  default:
    error(ERR_TYPESTR);
  }
  return length;
}

static void fileio_write(FILE *stream, int32 value) {
  int32 result;
  result = fputc(value, stream);
  if (result==EOF) error(ERR_CANTWRITE);
}

/*
** 'fileio_bput' writes a character to a file
*/
void fileio_bput(int32 handle, int32 value) {
  int32 result;

  if (handle==0) error(ERR_BADHANDLE);
  handle = map_handle(handle);
#ifndef NONET
  if (fileinfo[handle].filetype==NETWORK) {
    if(net_bput(fileinfo[handle].nethandle, value)) error(ERR_CANTWRITE);
  } else {
#endif
    if (fileinfo[handle].filetype==OPENIN) error(ERR_OPENIN);
    fileinfo[handle].eofstatus = OKAY;
    result = fputc(value, fileinfo[handle].stream);
    if (result==EOF) error(ERR_CANTWRITE);
    fileinfo[handle].lastwaswrite = TRUE;
#ifndef NONET
  }
#endif
}

/*
** 'fileio_bputstr' writes a string to a file
*/
void fileio_bputstr(int32 handle, char *string, int32 length) {
  int32 result;

  if (handle==0) error(ERR_BADHANDLE);
  handle = map_handle(handle);
#ifndef NONET
  if (fileinfo[handle].filetype==NETWORK) {
    if(net_bputstr(fileinfo[handle].nethandle, string, length)) error(ERR_CANTWRITE);
  } else {
#endif
    if (fileinfo[handle].filetype==OPENIN) error(ERR_OPENIN);
    fileinfo[handle].eofstatus = OKAY;
    result = fwrite(string, sizeof(char), length, fileinfo[handle].stream);
    if (result!=length) error(ERR_CANTWRITE);
    fileinfo[handle].lastwaswrite = TRUE;
#ifndef NONET
  }
#endif
}

/*
** 'fileio_printint' writes a four byte integer to a file in binary
** preceded with 0x40 to mark it as an integer.
** The number has to be written in 'big endian' format for compatibility
** with the Acorn interpreter
*/
void fileio_printint(int32 handle, int32 value) {
  FILE *stream;
  int32 n;

  if (handle==0) error(ERR_BADHANDLE);
  handle = map_handle(handle);
  if (fileinfo[handle].filetype==OPENIN) error(ERR_OPENIN);
  fileinfo[handle].eofstatus = OKAY;
  stream = fileinfo[handle].stream;
  fileio_write(stream, PRINT_INT);
  for (n=24; n>=0; n-=8) fileio_write(stream, value >> n);
  fileinfo[handle].lastwaswrite = TRUE;
}

void fileio_printuint8(int32 handle, uint8 value) {
  FILE *stream;

  if (handle==0) error(ERR_BADHANDLE);
  handle = map_handle(handle);
  if (fileinfo[handle].filetype==OPENIN) error(ERR_OPENIN);
  fileinfo[handle].eofstatus = OKAY;
  stream = fileinfo[handle].stream;
  fileio_write(stream, PRINT_UINT8);
  fileio_write(stream, value);
  fileinfo[handle].lastwaswrite = TRUE;
}

void fileio_printint64(int32 handle, int64 value) {
  FILE *stream;
  int32 n;

  if (handle==0) error(ERR_BADHANDLE);
  handle = map_handle(handle);
  if (fileinfo[handle].filetype==OPENIN) error(ERR_OPENIN);
  fileinfo[handle].eofstatus = OKAY;
  stream = fileinfo[handle].stream;
  fileio_write(stream, PRINT_INT64);
  for (n=56; n>=0; n-=8) fileio_write(stream, value >> n);
  fileinfo[handle].lastwaswrite = TRUE;
}

/*
** 'fileio_printfloat' writes an eight byte floating point value
** to a file in binary preceded with 0x88 to mark it as eight-
** byte floating point. The number is written in the same format
** that the Acorn interpreter uses under RISC OS. This requires
** some fiddling about with the number format. This part of the
** code was written by Darren Salt
*/
void fileio_printfloat(int32 handle, float64 value) {
  FILE *stream;
  int32 n;
  char temp[sizeof(float64)];

  if (handle==0) error(ERR_BADHANDLE);
  handle = map_handle(handle);
  if (fileinfo[handle].filetype==OPENIN) error(ERR_OPENIN);
  fileinfo[handle].eofstatus = OKAY;
  stream = fileinfo[handle].stream;
  fileio_write(stream, PRINT_FLOAT);
  memmove(temp, &value, sizeof(float64));
  switch (double_type) {
  case XMIXED_ENDIAN:
    for (n=0; n<sizeof(float64); n++) fileio_write(stream, temp[n]);
    break;
  case XLITTLE_ENDIAN:
    for (n=0; n<sizeof(float64); n++) fileio_write(stream, temp[n^4]);
    break;
  case XBIG_ENDIAN:
    for (n=0; n<sizeof(float64); n++) fileio_write(stream, temp[n^3]);
    break;
  case XBIG_MIXED_ENDIAN:
    for (n=0; n<sizeof(float64); n++) fileio_write(stream, temp[n^7]);
  }
  fileinfo[handle].lastwaswrite = TRUE;
}

/*
** 'fileio_printstring' writes a string to a file. If the length of the
** string is less than 256 bytes it is written in 'Acorn' format, that
** is, preceded with 0 and a single byte length with the string in
** reverse order. If longer than 255 bytes, the interpreter uses its own
** format. The string is preceded with 0x01 and its length written in
** four bytes (little endian format). The string is written in 'true'
** character order
*/
void fileio_printstring(int32 handle, char *string, int32 length) {
  FILE *stream;
  int32 n, temp, result;

  if (handle==0) error(ERR_BADHANDLE);
  handle = map_handle(handle);
  if (fileinfo[handle].filetype==OPENIN) error(ERR_OPENIN);
  fileinfo[handle].eofstatus = OKAY;
  stream = fileinfo[handle].stream;
  if (length<SHORT_STRING) {    /* Write string in Acorn format */
    fileio_write(stream, PRINT_SHORTSTR);
    fileio_write(stream, length);
    if (length>0) for (n=length-1; n>=0; n--) fileio_write(stream, string[n]);
  }
  else {        /* Long string - Use interpreter's extended format */
    fileio_write(stream, PRINT_LONGSTR);
    temp = length;
    for (n=0; n<sizeof(int32); n++) {   /* Write four byte length to file */
      fileio_write(stream, temp & BYTEMASK);
      temp = temp>>BYTESHIFT;
    }
    result = fwrite(string, sizeof(char), length, stream);
    if (result!=length) error(ERR_CANTWRITE);
  }
  fileinfo[handle].lastwaswrite = TRUE;
}

/*
** 'fileio_setptr' is used to set the current file pointer
*/
void fileio_setptr(int32 handle, int64 newoffset) {
  int32 result;

  if (handle==0) error(ERR_BADHANDLE);
  handle = map_handle(handle);
  result = fseek(fileinfo[handle].stream, newoffset, SEEK_SET);
  if (result==-1) error(ERR_SETPTRFAIL);        /* File pointer cannot be set */
  fileinfo[handle].eofstatus = OKAY;
}

/*
** 'fileio_getptr' returns the current value of the file pointer
*/
int64 fileio_getptr(int32 handle) {
  int64 result;

  if (handle==0) return 0; /* This is what happens on RISC OS 3.71 */
  handle = map_handle(handle);
  result = ftell(fileinfo[handle].stream);
  if (result==-1) error(ERR_GETPTRFAIL);        /* File pointer cannot be read */
  return result;
}

/*
** 'fileio_getext' returns the size of a file
*/
int64 fileio_getext(int32 handle) {
  int64 position, length;
  FILE *stream;

  if (handle==0) error(ERR_BADHANDLE);
  handle = map_handle(handle);
  stream = fileinfo[handle].stream;
  position = ftell(stream);
  if (position==-1) error(ERR_GETEXTFAIL);      /* Cannot find size of file */
  fseek(stream, 0, SEEK_END);                   /* Find the end of the file */
  length = ftell(stream);
  fseek(stream, position, SEEK_SET);    /* Restore file to its original file pointer position */
  return length;
}

/*
** 'fileio_setext' allows the size of a file to be changed.
*/
void fileio_setext(int32 handle, int64 newsize) {
  int32 fh, type;
  
  handle=map_handle(handle);
  type=fileinfo[handle].filetype;
  if((type==OPENOUT) || (type==OPENUP)) {
    fh=fileno(fileinfo[handle].stream);
    if (ftruncate(fh, newsize)) {
      error(ERR_CMDFAIL);
    }
  } else {
    error(ERR_CANTWRITE);
  }
}

/*
** 'fileio_eof' returns the current end-of-file state of file
** 'handle', returning 'TRUE' if it is at end-of-file.
** The code handles two cases. It tries to emulate the RISC OS
** way of determining end of file (the current value of the
** file pointer is equal to the size of the file) but if this
** fails it uses feof(). The snag with feof() is that it does
** not return TRUE under the same conditions as the Acorn
** definition
*/
int32 fileio_eof(int32 handle) {
  long position;
  FILE *stream;
  boolean ateof;

  if (handle==0) return kbd_pending();
  handle = map_handle(handle);
#ifndef NONET
  if (fileinfo[handle].filetype == NETWORK) {
    return net_eof(fileinfo[handle].nethandle);
  } else {
#endif
  stream = fileinfo[handle].stream;
  position = ftell(stream);
  if (position==-1) return feof(stream) ? TRUE : FALSE;
  fseek(stream, 0, SEEK_END);   /* Find the end of the file */
  ateof = ftell(stream)==position;
  fseek(stream, position, SEEK_SET);
  return ateof;
#ifndef NONET
  }
#endif
}

/*
** 'fileio_shutdown' is called at the end of a run to ensure that
** all files opened by the program have been closed
*/
void fileio_shutdown(void) {
  int32 n, count;
  count = 0;
  for (n=0; n<MAXFILES; n++) {
    if (fileinfo[n].filetype!=CLOSED) {
      close_file(n);
      count++;
    }
  }
  if (count==1)
    emulate_printf("\r\nNote: one open file has been closed\r\n");
  else if (count>1) {
    emulate_printf("\r\nNote: %d open files have been closed.\r\n", count);
  }
}

/*
** 'find_floatformat' is called to work out what format the machine
** on which the interpreter is running stores eight byte floating point
** numbers. This is needed so that it can read such numbers as written
** by the Acorn interpreter under RISC OS and also write them in that
** format. It looks for the byte containing the sign and high order
** bits of the exponent of a known value.
** This is based on original code written by Darren Salt
*/
static void find_floatformat(void) {
  union {
    float64 temp1;
    byte temp2[sizeof(float64)];
  } value;
  value.temp1 = 1.0;
  if (value.temp2[3]==0x3f)
    double_type = XMIXED_ENDIAN;        /* ARM old format */
  else if (value.temp2[7]==0x3f)
    double_type = XLITTLE_ENDIAN;       /* X86 */
  else if (value.temp2[0]==0x3f)
    double_type = XBIG_ENDIAN;
  else if (value.temp2[4]==0x3f)
    double_type = XBIG_MIXED_ENDIAN;
  else {        /* Unknown format */
    double_type = XLITTLE_ENDIAN;
    error(WARN_FUNNYFLOAT);
  }
}

/*
** 'init_fileio' is called to initialise the file handling
*/
void init_fileio(void) {
  int32 n;
  for (n=0; n<MAXFILES; n++) {
    fileinfo[n].stream = NIL;
    fileinfo[n].filetype = CLOSED;
    fileinfo[n].eofstatus = ATEOF;
  }
  find_floatformat();
}

#endif
