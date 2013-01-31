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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <ctype.h>
#ifdef STDC_HEADERS
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "checkalloc.h"
#include "die.h"
#include "strbuf.h"

#include <algorithm>

#ifndef isblank
#define isblank(c)	((c) == ' ' || (c) == '\t')
#endif
/**
 @file

String buffer: usage and memory status

					[xxx]: string buffer <br>
					'v': current pointer

@code
Function call                           Memory status
----------------------------------------------------------
                                        (not exist)
                                         v
sb = new STRBUF;                        []
                                          v
sb->push_back('a');                     [a]
                                          v
const char *s = sb->c_str();            [a\0]           s == "a"
                                            v
sb->append("bc");                       [abc]
                                            v
const char *s = sb->c_str();            [abc\0]         s == "abc"
                                            v
size_t len = sb->length();              [abc\0]         len == 3
                                         v
sb->clear();                            [abc\0]
                                         v
size_t len = sb->length();              [abc\0]         len == 0
                                           v
sb->append("XY");                       [XYc\0]
                                           v
const char *s = sb->c_str();            [XY\0]          s == "XY"

fp = fopen("/etc/passwd", "r");                                             v
char *s = strbuf_fgets(sb, fp, 0)       [root:*:0:0:Charlie &:/root:/bin/csh\0]
fclose(fp)				s == "root:*:0:0:Charlie &:/root:/bin/csh"

delete sb;                              (not exist)
@endcode
*/

/**
 * strbuf_puts_withterm: Put string until the terminator
 *
 *	@param[in]	sb	string buffer
 *	@param[in]	s	string
 *	@param[in]	c	terminator
 *	@return		pointer to the terminator
 */
void
strbuf_puts_withterm(STRBUF *sb, const char *s, int c)
{
	while (*s && *s != c) {
		sb->reserve(sb->length() + 1);
		*sb->curp++ = *s++;
	}
}
/**
 * strbuf_puts_nl: Put string with a new line
 *
 *	@param[in]	sb	string buffer
 *	@param[in]	s	string
 */
void
strbuf_puts_nl(STRBUF *sb, const char *s)
{
	while (*s) {
		sb->reserve(sb->length() + 1);
		*sb->curp++ = *s++;
	}

	sb->reserve(sb->length() + 1);
	*sb->curp++ = '\n';
}
/**
 * strbuf_putn: put digit string at the last of buffer.
 *
 *	@param[in]	sb	#STRBUF structure
 *	@param[in]	n	number
 */
void
strbuf_putn(STRBUF *sb, int n)
{
	if (n == 0) {
		sb->push_back('0');
	} else {
		char num[128];
		int i = 0;

		while (n) {
			if (i >= sizeof(num))
				die("Too big integer value.");
			num[i++] = n % 10 + '0';
			n = n / 10;
		}
		while (--i >= 0)
			sb->push_back(num[i]);
	}
}
/**
 * strbuf_unputc: remove specified char from the last of buffer
 *
 *	@param[in]	sb	#STRBUF structure
 *	@param[in]	c	character
 *	@return		0: do nothing, 1: removed
 */
int
strbuf_unputc(STRBUF *sb, int c)
{
	if (sb->curp > sb->sbuf && *(sb->curp - 1) == c) {
		sb->curp--;
		return 1;
	}
	return 0;
}
/**
 * strbuf_trim: trim following blanks.
 *
 *	@param[in]	sb	#STRBUF structure
 */
void
strbuf_trim(STRBUF *sb)
{
	char *p = sb->curp;

	while (p > sb->sbuf && isblank(*(p - 1)))
		*--p = 0;
	sb->curp = p;
}
/**
 * strbuf_fgets: read whole record into string buffer
 *
 *	@param[out]	sb	string buffer
 *	@param[in]	ip	input stream
 *	@param[in]	flags	flags <br>
 *			#STRBUF_NOCRLF:	remove last @CODE{'\\n'} and/or @CODE{'\\r'} if exist. <br>
 *			#STRBUF_APPEND:	append next record to existing data <br>
 *			#STRBUF_SHARPSKIP: skip lines which start with @CODE{'\#'}
 *	@return		record buffer (@VAR{NULL} at end of file)
 *
 * Returned buffer has whole record. <br>
 * The buffer end with @CODE{'\\0'}. If #STRBUF_NOCRLF is set then buffer doesn't
 * include @CODE{'\\r'} and @CODE{'\\n'}.
 */
char *
strbuf_fgets(STRBUF *sb, FILE *ip, int flags)
{
	if (!(flags & STRBUF_APPEND))
		sb->clear();

	if (sb->curp >= sb->endp)
		sb->reserve(sb->length() + EXPANDSIZE);	/* expand buffer */

	for (;;) {
		if (!fgets(sb->curp, sb->endp - sb->curp, ip)) {
			if (sb->curp == sb->sbuf)
				return NULL;
			break;
		}
		if (flags & STRBUF_SHARPSKIP && *(sb->curp) == '#')
			continue;
		sb->curp += strlen(sb->curp);
		if (sb->curp > sb->sbuf && *(sb->curp - 1) == '\n')
			break;
		else if (feof(ip)) {
			return sb->sbuf;
		}
		sb->reserve(sb->length() + EXPANDSIZE);
	}
	if (flags & STRBUF_NOCRLF) {
		if (*(sb->curp - 1) == '\n')
			*(--sb->curp) = 0;
		if (sb->curp > sb->sbuf && *(sb->curp - 1) == '\r')
			*(--sb->curp) = 0;
	}
	return sb->sbuf;
}
/**
 * strbuf_sprintf: do @NAME{sprintf} into string buffer.
 *
 *	@param[in]	sb	#STRBUF structure
 *	@param[in]	s	similar to @NAME{sprintf()} <br>
 *			Currently the following format is supported. <br>
 *			@CODE{\%s, \%d, \%\<number\>d, \%\<number\>s, \%-\<number\>d, \%-\<number\>s}
 */
void
strbuf_sprintf(STRBUF *sb, const char *s, ...)
{
	va_list ap;

	va_start(ap, s);
	strbuf_vsprintf(sb, s, ap);
	va_end(ap);
}
/**
 * strbuf_vsprintf: do @NAME{vsprintf} into string buffer.
 *
 *	@param[in]	sb	#STRBUF structure
 *	@param[in]	s	similar to @NAME{vsprintf()} <br>
 *			Currently the following format is supported. <br>
 *			@CODE{\%s, \%d, \%\<number\>d, \%\<number\>s, \%-\<number\>d, \%-\<number\>s}
 *	@param[in]	ap	 <br>
 */
void
strbuf_vsprintf(STRBUF *sb, const char *s, va_list ap)
{
	for (; *s; s++) {
		/*
		 * Put the before part of '%'.
		 */
		{
			const char *p;
			for (p = s; *p && *p != '%'; p++)
				;
			if (p > s) {
				sb->append(s, p - s);
				s = p;
			}
		}
		if (*s == '\0')
			break;
		if (*s == '%') {
			int c = (unsigned char)*++s;
			/*
			 * '%%' means '%'.
			 */
			if (c == '%') {
				sb->push_back(c);
			}
			/*
			 * If the optional number is specified then
			 * we forward the job to snprintf(3).
			 * o %<number>d
			 * o %<number>s
			 * o %-<number>d
			 * o %-<number>s
			 */
			else if (isdigit(c) || (c == '-' && isdigit((unsigned char)*(s + 1)))) {
				char format[32], buf[1024];
				int i = 0;

				format[i++] = '%';
				if (c == '-')
					format[i++] = *s++;
				while (isdigit((unsigned char)*s))
					format[i++] = *s++;
				format[i++] = c = *s;
				format[i] = '\0';
				if (c == 'd' || c == 'x')
					snprintf(buf, sizeof(buf), format, va_arg(ap, int));
				else if (c == 's')
					snprintf(buf, sizeof(buf), format, va_arg(ap, char *));
				else
					die("Unsupported control character '%c'.", c);
				sb->append(buf);
			} else if (c == 's') {
				sb->append(va_arg(ap, char *));
			} else if (c == 'd') {
				strbuf_putn(sb, va_arg(ap, int));
			} else {
				die("Unsupported control character '%c'.", c);
			}
		}
	}
}

STRBUF::STRBUF(int init) :
  sbuf(NULL), endp(NULL), curp(NULL),
  sbufsize(init > 0 ? init : INITIALSIZE)
{
  sbuf = new char[sbufsize+1];
  curp = sbuf;
  endp = sbuf + sbufsize;
}

STRBUF::~STRBUF() {
  delete [] sbuf;
}

void STRBUF::append(const char* s) {
  while (*s) {
    reserve(length()+1);
    *curp++ = *s++;
  }
}

void STRBUF::append(const char* s, size_t len) {
  reserve(length() + len);
  while (len-- > 0) {
    *curp++ = *s++;
  }
}

void STRBUF::append(size_t n, char c)
{
  reserve(length() + n);
  while (n-- > 0) {
    *curp++ = c;
  }
}

void STRBUF::reserve(size_t new_capacity) {
  if (capacity() >= new_capacity) {
    return;
  } else if ((new_capacity - capacity()) < EXPANDSIZE) {
    new_capacity = capacity() + EXPANDSIZE;
  }

  size_t count = length();
  char* newbuf = new char[new_capacity+1];
  std::copy(sbuf, sbuf+count, newbuf);

  delete[] sbuf;
  sbufsize = new_capacity;
  sbuf = newbuf;

  curp = sbuf+count;
  endp = sbuf+sbufsize;
}
