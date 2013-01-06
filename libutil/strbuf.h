/*
 * Copyright (c) 1997, 1998, 1999, 2000, 2002, 2005, 2006, 2010
 *	Tama Communications Corporation
 *
 * This file is part of GNU GLOBAL.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _STRBUF_H
#define _STRBUF_H

#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <stdarg.h>

/** @file */

#ifndef __attribute__
/* This feature is available in gcc versions 2.5 and later.  */
# if __GNUC__ < 2 || (__GNUC__ == 2 && __GNUC_MINOR__ < 5) || __STRICT_ANSI__
#  define __attribute__(x)
# endif
/* The __-protected variants of `format' and `printf' attributes
   are accepted by gcc versions 2.6.4 (effectively 2.7) and later.  */
# if __GNUC__ < 2 || (__GNUC__ == 2 && __GNUC_MINOR__ < 7)
#  define __format__ format
#  define __printf__ printf
# endif
#endif

#define INITIALSIZE 80
#define EXPANDSIZE 80

/**
 * @name Defines for strbuf_fgets()
 * See: strbuf_fgets()
 */
/** @{ */
			/** append next record to existing data. */
#define STRBUF_APPEND		1
			/** remove last @CODE{'\\n'} and/or @CODE{'\\r'}, if exists. */
#define STRBUF_NOCRLF		2
			/** skip lines which start with @CODE{'\#'} */
#define STRBUF_SHARPSKIP	4
/** @} */

struct STRBUF {
  char	*sbuf;
  char	*endp;
  char	*curp;
  int	 sbufsize;

  /// open string buffer.
  /// \param[in]	init	initial buffer size <br>
  /// \return	sb	#STRBUF structure
  explicit STRBUF(int init = INITIALSIZE);
  ~STRBUF();

  /// \todo Would like to return a const char* here, but many clients
  /// written under the assumption that they can modify the buffer they
  /// get back here (which is okay if they don't write past the end or
  /// overwrite the trailing null character).
  inline char* c_str() const {
    *curp = 0;
    return sbuf;
  }

  inline size_t length() const {
    return (curp - sbuf);
  }

  inline size_t capacity() const {
    return sbufsize;
  }

  inline bool empty() const {
    return length() == 0;
  }

  inline void clear() {
    curp = sbuf;
  }

  inline void resize(size_t new_length) {
    if (new_length < length()) {
      curp = sbuf + new_length;
    }
  }

  void reserve(size_t new_capacity);
};

/**
 * STATIC_STRBUF(sb):
 *
 * This macro is used for static string buffer which is suitable for
 * work area and(or) return value of function. The area allocated once
 * is repeatedly used though the area is never released. <br>
 *
 * @attention
 * You should call STRBUF::clear() every time before using. <br>
 * You must @STRONG{not} call delete for it.
 *
 * @par Usage:
 * @code
 *      function(...) {
 *              STATIC_STRBUF(sb);
 *              sb->clear();
 *              ...
 *		strbuf_puts(sb, "xxxxx");
 *              ...
 *              return sb->c_str();
 *      }
 * @endcode
 *
 * @remark Not all uses of STATIC_STRBUF perform a call to clear, since they
 * rely on the static instance containing the results from the previous usage; these
 * places should perhaps just declare a static STRBUF directly.
 */
#define STATIC_STRBUF(sb) static STRBUF sb##_instance; STRBUF* sb = & sb##_instance

void strbuf_nputs(STRBUF *, const char *, int);
void strbuf_nputc(STRBUF *, int, int);
void strbuf_puts(STRBUF *, const char *);
void strbuf_puts_withterm(STRBUF *, const char *, int);
void strbuf_puts_nl(STRBUF *, const char *);
void strbuf_putn(STRBUF *, int);
int strbuf_unputc(STRBUF *, int);
void strbuf_trim(STRBUF *);
char *strbuf_fgets(STRBUF *, FILE *, int);
void strbuf_sprintf(STRBUF *, const char *, ...)
	__attribute__ ((__format__ (__printf__, 2, 3)));
void strbuf_vsprintf(STRBUF *, const char *, va_list)
	__attribute__ ((__format__ (__printf__, 2, 0)));

inline void strbuf_putc(STRBUF* sb, char c) {
  sb->reserve(sb->length() + 1);
  *sb->curp++ = c;		     
}

inline void strbuf_puts0(STRBUF* sb, const char* s) {
  strbuf_puts(sb, s);
  strbuf_putc(sb, '\0');
}

#endif /* ! _STRBUF_H */
