/*
 * Copyright (c) 2005, 2006, 2010 Tama Communications Corporation
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
#ifndef _PATHCONVERT_H_
#define _PATHCONVERT_H_

#include <stdio.h>
#include "gparam.h"
#include "strbuf.h"

struct CONVERT {
  FILE		*op;
  int		 type;		/**< #PATH_ABSOLUTE, #PATH_RELATIVE */
  int		 format;	/**< defined in @FILE{format.h} */
  STRBUF	*abspath;
  char		 basedir[MAXPATHLEN];
  int		 start_point;
  int		 db;		/**< for @NAME{gtags-cscope} */

  /// open convert filter
  /// \param[in]	type	#PATH_ABSOLUTE, #PATH_RELATIVE, #PATH_THROUGH
  /// \param[in]	format	tag record format
  /// \param[in]	root	root directory of source tree
  /// \param[in]	cwd	current directory
  /// \param[in]	dbpath	dbpath directory
  /// \param[in]	op	output file
  /// \param[in]	db	tag type (#GTAGS, #GRTAGS, #GSYMS, #GPATH, #NOTAGS) <br>
  ///			only for @NAME{cscope} format
  CONVERT(int type, int format, const char *root, const char *cwd, const char *dbpath, FILE *op, int db);
  ~CONVERT();
};

void set_encode_chars(const unsigned char *);
void set_print0(void);
char *decode_path(const char *);
void convert_put(CONVERT *, const char *);
void convert_put_path(CONVERT *, const char *);
void convert_put_using(CONVERT *, const char *, const char *, int, const char *, const char *);

#endif /* ! _PATHCONVERT_H_ */
