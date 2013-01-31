/*
 * Copyright (c) 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005,
 *		2006, 2008, 2010, 2011
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
#include <stdio.h>
#ifdef STDC_HEADERS
#include <stdlib.h>
#endif
#include <ctype.h>

#include "global.h"
#include "anchor.h"
#include "cache.h"
#include "common.h"
#include "incop.h"
#include "path2url.h"
#include "htags.h"

/*----------------------------------------------------------------------*/
/* Parser switch							*/
/*----------------------------------------------------------------------*/
/**
 * @details
 * This is the linkage section of each parsers. <br>
 * If you want to support new language, you must define two procedures: <br>
 *	-# Initializing procedure (#init_proc).
 *		Called once first with an input file descripter.
 *	-# Executing procedure (#exec_proc).
 *		Called repeatedly until returning @NAME{EOF}. <br>
 *		It should read from above descripter and write @NAME{HTML}
 *		using output procedures in this module.
 */
struct lang_entry {
	const char *lang_name;
	void (*init_proc)(FILE *);		/**< initializing procedure */
	int (*exec_proc)(void);			/**< executing procedure */
};

/**
 * @name initializing procedures
 * For lang_entry::init_proc
 */
/** @{ */
void c_parser_init(FILE *);
void yacc_parser_init(FILE *);
void cpp_parser_init(FILE *);
void java_parser_init(FILE *);
void php_parser_init(FILE *);
void asm_parser_init(FILE *);
/** @} */

/**
 * @name executing procedures
 * For lang_entry::exec_proc
 */
/** @{ */
int c_lex(void);
int cpp_lex(void);
int java_lex(void);
int php_lex(void);
int asm_lex(void);
/** @} */

/**
 * The first entry is default language.
 */
struct lang_entry lang_switch[] = {
	/* lang_name	init_proc 		exec_proc */
	{"c",		c_parser_init,		c_lex},		/* DEFAULT */
	{"yacc",	yacc_parser_init,	c_lex},
	{"cpp",		cpp_parser_init,	cpp_lex},
	{"java",	java_parser_init,	java_lex},
	{"php",		php_parser_init,	php_lex},
	{"asm",		asm_parser_init,	asm_lex}
};
#define DEFAULT_ENTRY &lang_switch[0]

/**
 * get language entry.
 *
 * If the specified language (@a lang) is not found, it assumes the
 * default language, which is @NAME{C}.
 *
 *	@param[in]	lang	language name (@VAR{NULL} means 'not specified'.)
 *	@return		language entry
 */
static struct lang_entry *
get_lang_entry(const char *lang)
{
	int i, size = sizeof(lang_switch) / sizeof(struct lang_entry);

	/*
	 * if language not specified, it assumes default language.
	 */
	if (lang == NULL)
		return DEFAULT_ENTRY;
	for (i = 0; i < size; i++)
		if (!strcmp(lang, lang_switch[i].lang_name))
			return &lang_switch[i];
	/*
	 * if specified language not found, it assumes default language.
	 */
	return DEFAULT_ENTRY;
}
/*----------------------------------------------------------------------*/
/* Input/Output								*/
/*----------------------------------------------------------------------*/
/*
 * Input/Output descriptor.
 */
static FILEOP *fileop_out;
static FILEOP *fileop_in;
static FILE *out;
static FILE *in;

STATIC_STRBUF(outbuf);
static const char *curpfile;
static int warned;
static int last_lineno;

/**
 * Put a character to HTML as is.
 *
 * @note You should use this function to put a control character. <br>
 *
 * @attention
 * No escaping of @CODE{'\<'}, @CODE{'\>'} and @CODE{'\&'} is performed.
 *
 * @see put_char()
 */
void
echoc(int c)
{
        outbuf->push_back(c);
}
/**
 * Put a string to HTML as is.
 *
 * @note You should use this function to put a control sequence.
 *
 * @attention
 * No escaping of @CODE{'\<'}, @CODE{'\>'} and @CODE{'\&'} is performed.
 *
 * @see put_string()
 */
void
echos(const char *s)
{
        outbuf->append(s);
}
/*----------------------------------------------------------------------*/
/* HTML output								*/
/*----------------------------------------------------------------------*/
/**
 * Quote character with HTML's way.
 *
 * (Fixes @CODE{'\<'}, @CODE{'\>'} and @CODE{'\&'} for HTML).
 */
static const char *
HTML_quoting(int c)
{
	if (c == '<')
		return quote_little;
	else if (c == '>')
		return quote_great;
	else if (c == '&')
		return quote_amp;
	return NULL;
}
/**
 * fill_anchor: fill anchor into file name
 *
 *       @param[in]      root   \$root or index page
 *       @param[in]      path   \$path name
 *       @return              hypertext file name string
 */
const char *
fill_anchor(const char *root, const char *path)
{
	char buf[MAXBUFLEN];
	strlimcpy(buf, path, sizeof(buf));

	char* p;
	for (p = buf; *p; p++)
		if (*p == sep)
			*p = '\0';
	char* limit = p;

	STATIC_STRBUF(sb);
	sb->clear();
	if (root != NULL)
		strbuf_sprintf(sb, "%sroot%s/", gen_href_begin_simple(root), gen_href_end());
	for (p = buf; p < limit; p += strlen(p) + 1) {
		const char *path = buf;
		const char *unit = p;
		const char *next = p + strlen(p) + 1;

		if (next > limit) {
			sb->append(unit);
			break;
		}
		if (p > buf)
			*(p - 1) = sep;
		sb->append(gen_href_begin("../files", path2fid(path), HTML, NULL));
		sb->append(unit);
		sb->append(gen_href_end());
		sb->push_back('/');
	}
        return sb->c_str();
}

/**
 * link_format: make hypertext from anchor array.
 *
 *	@param[in]	ref	(previous, next, first, last, top, bottom) <br>
 *		-1: top, -2: bottom, other: line number
 *	@return	HTML
 */
const char *
link_format(int ref[A_SIZE])
{
	STATIC_STRBUF(sb);
	sb->clear();

	const char **label = Iflag ? anchor_comment : anchor_label;
	const char **icons = anchor_icons;

	for (int i = 0; i < A_LIMIT; i++) {
		if (i == A_INDEX) {
			sb->append(gen_href_begin("..", "mains", normal_suffix, NULL));
		} else if (i == A_HELP) {
			sb->append(gen_href_begin("..", "help", normal_suffix, NULL));
		} else if (ref[i]) {
			char tmp[32], *key = tmp;

			if (ref[i] == -1)
				key = "TOP";
			else if (ref[i] == -2)
				key = "BOTTOM";
			else
				snprintf(tmp, sizeof(tmp), "%d", ref[i]);
			sb->append(gen_href_begin(NULL, NULL, NULL, key));
		}
		if (Iflag) {
			char tmp[MAXPATHLEN];
			snprintf(tmp, sizeof(tmp), "%s%s", (i != A_INDEX && i != A_HELP && ref[i] == 0) ? "n_" : "", icons[i]);
			sb->append(gen_image(PARENT, tmp, label[i]));
		} else {
			strbuf_sprintf(sb, "[%s]", label[i]);
		}
		if (i == A_INDEX || i == A_HELP || ref[i] != 0)
			sb->append(gen_href_end());
	}
        return sb->c_str();
}
/**
 * fixed_guide_link_format: make fixed guide
 *
 *	@param[in]	ref	(previous, next, first, last, top, bottom) <br>
 *		-1: top, -2: bottom, other: line number
 *	@param[in]	anchors
 *	@return	HTML
 */
const char *
fixed_guide_link_format(int ref[A_LIMIT], const char *anchors)
{
	STATIC_STRBUF(sb);
	sb->clear();

	sb->append("<!-- beginning of fixed guide -->\n");
	sb->append(guide_begin);
	sb->push_back('\n');
	for (int i = 0; i < A_LIMIT; i++) {
		if (i == A_PREV || i == A_NEXT)
			continue;
		sb->append(guide_unit_begin);
		switch (i) {
		case A_FIRST:
		case A_LAST:
			if (ref[i] == 0)
				sb->append(gen_href_begin(NULL, NULL, NULL, (i == A_FIRST) ? "TOP" : "BOTTOM"));
			else {
				char lineno[32];
				snprintf(lineno, sizeof(lineno), "%d", ref[i]);
				sb->append(gen_href_begin(NULL, NULL, NULL, lineno));
			}
			break;
		case A_TOP:
			sb->append(gen_href_begin(NULL, NULL, NULL, "TOP"));
			break;
		case A_BOTTOM:
			sb->append(gen_href_begin(NULL, NULL, NULL, "BOTTOM"));
			break;
		case A_INDEX:
			sb->append(gen_href_begin("..", "mains", normal_suffix, NULL));
			break;
		case A_HELP:
			sb->append(gen_href_begin("..", "help", normal_suffix, NULL));
			break;
		default:
			die("fixed_guide_link_format: something is wrong.(%d)", i);
			break;
		}
		if (Iflag)
			sb->append(gen_image(PARENT, anchor_icons[i], anchor_label[i]));
		else
			strbuf_sprintf(sb, "[%s]", anchor_label[i]);
		sb->append(gen_href_end());
		sb->append(guide_unit_end);
		sb->push_back('\n');
	}
	sb->append(guide_path_begin);
	sb->append(anchors);
	sb->append(guide_path_end);
	sb->push_back('\n');
	sb->append(guide_end);
	sb->push_back('\n');
	sb->append("<!-- end of fixed guide -->\n");

	return sb->c_str();
}
/**
 * generate_guide: generate guide string for definition line.
 *
 *	@param[in]	lineno	line number
 *	@return		guide string
 */
const char *
generate_guide(int lineno)
{
	int i = 0;
	if (definition_header == RIGHT_HEADER)
		i = 4;
	else if (nflag)
		i = ncol + 1;

	STATIC_STRBUF(sb);
	sb->clear();
	if (i > 0)
		for (; i > 0; i--)
			sb->push_back(' ');
	strbuf_sprintf(sb, "%s/* ", comment_begin);
	sb->append(link_format(anchor_getlinks(lineno)));
	if (show_position)
		strbuf_sprintf(sb, "%s%s[+%d %s]%s",
			quote_space, position_begin, lineno, curpfile, position_end);
	strbuf_sprintf(sb, " */%s", comment_end);

	return sb->c_str();
}
/**
 * tooltip: generate tooltip string
 *
 *	@param[in]	type	@CODE{'I'}: 'Included from' <br>
 *			@CODE{'R'}: 'Defined at' <br>
 *			@CODE{'Y'}: 'Used at' <br>
 *			@CODE{'D'}, @CODE{'M'}: 'Referred from'
 *	@param[in]	lno	line number
 *	@param[in]	opt	
 *	@return		tooltip string
 */
const char *
tooltip(int type, int lno, const char *opt)
{
	STATIC_STRBUF(sb);
	sb->clear();

	if (lno > 0) {
		if (type == 'I')
			sb->append("Included from");
		else if (type == 'R')
			sb->append("Defined at");
		else if (type == 'Y')
			sb->append("Used at");
		else
			sb->append("Referred from");
		sb->push_back(' ');
		strbuf_putn(sb, lno);
		if (opt) {
			sb->append(" in ");
			sb->append(opt);
		}
	} else {
		sb->append("Multiple ");
		if (type == 'I')
			sb->append("included from");
		else if (type == 'R')
			sb->append("defined in");
		else if (type == 'Y')
			sb->append("used in");
		else
			sb->append("referred from");
		sb->push_back(' ');
		sb->append(opt);
		sb->push_back(' ');
		sb->append("places");
	}
	sb->push_back('.');
	return sb->c_str();
}
/**
 * put_anchor: output HTML anchor.
 *
 *	@param[in]	name	tag
 *	@param[in]	type	tag type. @CODE{'R'}: @NAME{GTAGS} <br>
 *			@CODE{'Y'}: @NAME{GSYMS} <br>
 *			@CODE{'D'}, @CODE{'M'}, @CODE{'T'}: @NAME{GRTAGS}
 *	@param[in]	lineno	current line no
 */
void
put_anchor(char *name, int type, int lineno)
{
	const char *line;
	int db;

	if (type == 'R')
		db = GTAGS;
	else if (type == 'Y')
		db = GSYMS;
	else	/* 'D', 'M' or 'T' */
		db = GRTAGS;
	line = cache_get(db, name);
	if (line == NULL) {
		if ((type == 'R' || type == 'Y') && wflag) {
			warning("%s %d %s(%c) found but not defined.",
				curpfile, lineno, name, type);
			if (colorize_warned_line)
				warned = 1;
		}
		outbuf->append(name);
	} else {
		/*
		 * About the format of 'line', please see the head comment of cache.c.
		 */
		if (*line == ' ') {
			const char *fid = line + 1;
			const char *count = nextstring(fid);
			const char *dir, *file, *suffix = NULL;

			if (dynamic) {
				STATIC_STRBUF(sb);
				sb->clear();

				sb->append(action);
				sb->push_back('?');
				sb->append("pattern=");
				sb->append(name);
				sb->append(quote_amp);
				if (Sflag) {
					sb->append("id=");
					sb->append(sitekey);
					sb->append(quote_amp);
				}
				sb->append("type=");
				if (db == GTAGS)
					sb->append("definitions");
				else if (db == GRTAGS)
					sb->append("reference");
				else
					sb->append("symbol");
				file = sb->c_str();
				dir = (*action == '/') ? NULL : "..";
			} else {
				if (type == 'R')
					dir = upperdir(DEFS);
				else if (type == 'Y')
					dir = upperdir(SYMS);
				else	/* 'D', 'M' or 'T' */
					dir = upperdir(REFS);
				file = fid;
				suffix = HTML;
			}
			outbuf->append(gen_href_begin_with_title(dir, file, suffix, NULL, tooltip(type, -1, count)));
			outbuf->append(name);
			outbuf->append(gen_href_end());
		} else {
			const char *lno = line;
			const char *fid = nextstring(line);
			const char *path = gpath_fid2path(fid, NULL);

			path += 2;              /* remove './' */
			/*
			 * Don't make a link which refers to itself.
			 * Being used only once means that it is a self link.
			 */
			if (db == GSYMS) {
				outbuf->append(name);
				return;
			}
			outbuf->append(gen_href_begin_with_title(upperdir(SRCS), fid, HTML, lno, tooltip(type, atoi(lno), path)));
			outbuf->append(name);
			outbuf->append(gen_href_end());
		}
	}
}
/**
 * put_anchor_force: output HTML anchor without warning.
 *
 *	@param[in]	name	tag
 *	@param[in]	length
 *	@param[in]	lineno	current line no
 *
 * @remark The tag type is fixed at 'R' (@NAME{GTAGS})
 */
void
put_anchor_force(char *name, int length, int lineno)
{
	int saveflag = wflag;

	STATIC_STRBUF(sb);
	sb->clear();
	sb->append(name, length);
	wflag = 0;
	put_anchor(sb->c_str(), 'R', lineno);
	wflag = saveflag;
}
/**
 * put_include_anchor: output HTML anchor.
 *
 *	@param[in]	inc	inc structure
 *	@param[in]	path	path name for display
 */
void
put_include_anchor(struct data *inc, const char *path)
{
	if (inc->count == 1)
		outbuf->append(gen_href_begin(NULL, path2fid(inc->contents->c_str()), HTML, NULL));
	else {
		char id[32];
		snprintf(id, sizeof(id), "%d", inc->id);
		outbuf->append(gen_href_begin(upperdir(INCS), id, HTML, NULL));
	}
	outbuf->append(path);
	outbuf->append(gen_href_end());
}
/**
 * put_include_anchor_direct: output HTML anchor.
 *
 *	@param[in]	file	normalized path
 *	@param[in]	path	path name for display
 */
void
put_include_anchor_direct(const char *file, const char *path)
{
	outbuf->append(gen_href_begin(NULL, path2fid(file), HTML, NULL));
	outbuf->append(path);
	outbuf->append(gen_href_end());
}
/**
 * Put a reserved word (@CODE{if}, @CODE{while}, ...)
 */
void
put_reserved_word(const char *word)
{
	outbuf->append(reserved_begin);
	outbuf->append(word);
	outbuf->append(reserved_end);
}
/**
 * Put a macro (@CODE{\#define}, @CODE{\#undef}, ...) 
 */
void
put_macro(const char *word)
{
	outbuf->append(sharp_begin);
	outbuf->append(word);
	outbuf->append(sharp_end);
}
/**
 * Print warning message when unknown preprocessing directive is found.
 */
void
unknown_preprocessing_directive(const char *word, int lineno)
{
	word = strtrim(word, TRIM_ALL, NULL);
	warning("unknown preprocessing directive '%s'. [+%d %s]", word, lineno, curpfile);
	if (colorize_warned_line)
		warned = 1;
}
/**
 * Print warning message when unexpected eof.
 */
void
unexpected_eof(int lineno)
{
	warning("unexpected eof. [+%d %s]", lineno, curpfile);
	if (colorize_warned_line)
		warned = 1;
}
/**
 * Print warning message when unknown @NAME{yacc} directive is found.
 */
void
unknown_yacc_directive(const char *word, int lineno)
{
	warning("unknown yacc directive '%s'. [+%d %s]", word, lineno, curpfile);
	if (colorize_warned_line)
		warned = 1;
}
/**
 * Print warning message when unmatched brace is found.
 */
void
missing_left(const char *word, int lineno)
{
	warning("missing left '%s'. [+%d %s]", word, lineno, curpfile);
	if (colorize_warned_line)
		warned = 1;
}
/**
 * Put a character with HTML quoting.
 *
 * @note If you want to put @CODE{'\<'}, @CODE{'\>'} or @CODE{'\&'}, you
 * 		should echoc() instead, as this function escapes (fixes) those
 *		characters for HTML.
 */
void
put_char(int c)
{
	const char *quoted = HTML_quoting(c);

	if (quoted)
		outbuf->append(quoted);
	else
		outbuf->push_back(c);
}
/**
 * Put a string with HTML quoting.
 *
 * @note If you want to put HTML tag itself, you should echos() instead,
 *		as this function escapes (fixes) the characters @CODE{'\<'},
 *		@CODE{'\>'} and @CODE{'\&'} for HTML.
 */
void
put_string(const char *s)
{
	for (; *s; s++)
		put_char(*s);
}
/**
 * Put brace (@c '{', @c '}')
 */
void
put_brace(const char *text)
{
	outbuf->append(brace_begin);
	outbuf->append(text);
	outbuf->append(brace_end);
}

/**
 * @name common procedure for line control.
 */
/** @{ */
static char lineno_format[32];
static const char *guide = NULL;
/** @} */

/**
 * Begin of line processing.
 */
void
put_begin_of_line(int lineno)
{
        if (definition_header != NO_HEADER) {
                if (define_line(lineno))
                        guide = generate_guide(lineno);
                else
                        guide = NULL;
        }
        if (guide && definition_header == BEFORE_HEADER) {
		fputs_nl(guide, out);
                guide = NULL;
        }
}
/**
 * End of line processing.
 *
 *	@param[in]	lineno	current line number
 *	@par Globals used (input):
 *		@NAME{outbuf}, HTML line image
 *
 * The @EMPH{outbuf} (string buffer) has HTML image of the line. <br>
 * This function flush and clear it.
 */
void
put_end_of_line(int lineno)
{
	fputs(gen_name_number(lineno), out);
        if (nflag)
                fprintf(out, lineno_format, lineno);
	if (warned)
		fputs(warned_line_begin, out);

	/* flush output buffer */
	fputs(outbuf->c_str(), out);
	outbuf->clear();

	if (warned)
		fputs(warned_line_end, out);
	if (guide == NULL)
		fputc('\n', out);
	else {
		if (definition_header == RIGHT_HEADER)
			fputs(guide, out);
		fputc('\n', out);
		if (definition_header == AFTER_HEADER) {
			fputs_nl(guide, out);
		}
		guide = NULL;
	}
	warned = 0;

	/* save for the other job in this module */
	last_lineno = lineno;
}
/**
 * Encode URL.
 *
 *	@param[out]	sb	encoded URL
 *	@param[in]	url	URL
 */
static void
encode(STRBUF *sb, const char *url)
{
	int c;

	while ((c = (unsigned char)*url++) != '\0') {
		if (isurlchar(c)) {
			sb->push_back(c);
		} else {
			sb->push_back('%');
			sb->push_back("0123456789abcdef"[c >> 4]);
			sb->push_back("0123456789abcdef"[c & 0x0f]);
		}
	}
}
/**
 * get_cvs_module: return @NAME{CVS} module of source file.
 *
 *	@param[in]	file		source path
 *	@param[out]	basename	If @a basename is not @CODE{NULL}, store pointer to <br>
 *				the last component of source path.
 *	@return		!=NULL : relative path from repository top <br>
 *			==NULL : CVS/Repository is not readable.
 */
static const char *
get_cvs_module(const char *file, const char **basename)
{
	STATIC_STRBUF(dir);
	dir->clear();
	const char* p = locatestring(file, "/", MATCH_LAST);
	if (p != NULL) {
		dir->append(file, p - file);
		p++;
	} else {
		dir->push_back('.');
		p = file;
	}
	if (basename != NULL)
		*basename = p;

	STATIC_STRBUF(module);
	static char prev_dir[MAXPATHLEN];
	if (strcmp(dir->c_str(), prev_dir) != 0) {
		strlimcpy(prev_dir, dir->c_str(), sizeof(prev_dir));
		module->clear();
		dir->append("/CVS/Repository");
		FILE* ip = fopen(dir->c_str(), "r");
		if (ip != NULL) {
			strbuf_fgets(module, ip, STRBUF_NOCRLF);
			fclose(ip);
		}
	}
	if (!module->empty())
		return module->c_str();
	return NULL;
}
/**
 *
 * src2html: convert source code into HTML
 *
 *       @param[in]      src   source file     - Read from
 *       @param[in]      html  HTML file       - Write to
 *       @param[in]      notsource 1: isn't source, 0: source.
 */
void
src2html(const char *src, const char *html, int notsource)
{
	char indexlink[128];

	/*
	 * setup lineno format.
	 */
	snprintf(lineno_format, sizeof(lineno_format), "%%%dd ", ncol);

	fileop_in  = open_input_file(src);
	in = get_descripter(fileop_in);
        curpfile = src;
        warned = 0;

	fileop_out = open_output_file(html, cflag);
	out = get_descripter(fileop_out);
	outbuf->clear();

	snprintf(indexlink, sizeof(indexlink), "../mains.%s", normal_suffix);
	fputs_nl(gen_page_begin(src, SUBDIR), out);
	fputs_nl(body_begin, out);
	/*
         * print fixed guide
         */
	if (fixed_guide)
		fputs(fixed_guide_link_format(anchor_getlinks(0), fill_anchor(NULL, src)), out);
	/*
         * print the header
         */
	if (insert_header)
		fputs(gen_insert_header(SUBDIR), out);
	fputs(gen_name_string("TOP"), out);
	fputs(header_begin, out);
	fputs(fill_anchor(indexlink, src), out);
	if (cvsweb_url) {
		const char *module, *basename;

		STATIC_STRBUF(sb);
		sb->clear();

		sb->append(cvsweb_url);
		if (use_cvs_module
		 && (module = get_cvs_module(src, &basename)) != NULL) {
			encode(sb, module);
			sb->push_back('/');
			encode(sb, basename);
		} else {
			encode(sb, src);
		}
		if (cvsweb_cvsroot) {
			sb->append("?cvsroot=");
			sb->append(cvsweb_cvsroot);
		}
		fputs(quote_space, out);
		fputs(gen_href_begin_simple(sb->c_str()), out);
		fputs(cvslink_begin, out);
		fputs("[CVS]", out);
		fputs(cvslink_end, out);
		fputs_nl(gen_href_end(), out);
		/* doesn't close string buffer */
	}
	fputs_nl(header_end, out);
	fputs(comment_begin, out);
	fputs("/* ", out);

	fputs(link_format(anchor_getlinks(0)), out);
	if (show_position)
		fprintf(out, "%s%s[+1 %s]%s", quote_space, position_begin, src, position_end);
	fputs(" */", out);
	fputs_nl(comment_end, out);
	fputs_nl(hr, out);
        /*
         * It is not source file.
         */
        if (notsource) {
		STRBUF sb;
		const char *_;

		fputs_nl(verbatim_begin, out);
		last_lineno = 0;
		while ((_ = strbuf_fgets(&sb, in, STRBUF_NOCRLF)) != NULL) {
			fputs(gen_name_number(++last_lineno), out);
			detab_replacing(out, _, HTML_quoting);
		}
		fputs_nl(verbatim_end, out);
        }
	/*
	 * It's source code.
	 */
	else {
		const char *basename;
		struct data *incref;
		struct anchor *ancref;

                /*
                 * INCLUDED FROM index.
                 */
		basename = locatestring(src, "/", MATCH_LAST);
		if (basename)
			basename++;
		else
			basename = src;
		incref = get_included(basename);
		if (incref) {
			char s_id[32];
			const char *dir, *file, *suffix, *key, *title;

			fputs(header_begin, out);
			if (incref->ref_count > 1) {
				char s_count[32];

				snprintf(s_count, sizeof(s_count), "%d", incref->ref_count);
				snprintf(s_id, sizeof(s_id), "%d", incref->id);
				dir = upperdir(INCREFS);
				file = s_id;
				suffix = HTML;
				key = NULL;
				title = tooltip('I', -1, s_count);
			} else {
				const char *p = incref->ref_contents->c_str();
				const char *lno = strmake(p, " ");
				const char *filename;

				p = locatestring(p, " ", MATCH_FIRST);
				if (p == NULL)
					die("internal error.(incref->ref_contents)");
				filename = p + 1;
				if (filename[0] == '.' && filename[1] == '/')
					filename += 2;
				dir = NULL;
				file = path2fid(filename);
				suffix = HTML;
				key = lno;
				title = tooltip('I', atoi(lno), filename);
			}
			fputs(gen_href_begin_with_title(dir, file, suffix, key, title), out);
			fputs(title_included_from, out);
			fputs(gen_href_end(), out);
			fputs_nl(header_end, out);
			fputs_nl(hr, out);
		}
		/*
		 * DEFINITIONS index.
		 */
		STATIC_STRBUF(define_index);
		define_index->clear();

		for (ancref = anchor_first(); ancref; ancref = anchor_next()) {
			if (ancref->type == 'D') {
				char tmp[32];
				snprintf(tmp, sizeof(tmp), "%d", ancref->lineno);
				define_index->append(item_begin);
				define_index->append(gen_href_begin_with_title(NULL, NULL, NULL, tmp, tooltip('R', ancref->lineno, NULL)));
				define_index->append(gettag(ancref));
				define_index->append(gen_href_end());
				*define_index << item_end << '\n';
			}
		}
		if (!define_index->empty()) {
			fputs(header_begin, out);
			fputs(title_define_index, out);
			fputs_nl(header_end, out);
			fputs_nl("This source file includes following definitions.", out);
			fputs_nl(list_begin, out);
			fputs(define_index->c_str(), out);
			fputs_nl(list_end, out);
			fputs_nl(hr, out);
		}
		/*
		 * print source code
		 */
		fputs_nl(verbatim_begin, out);
		{
			const char *suffix = locatestring(src, ".", MATCH_LAST);
			const char *lang = NULL;
			struct lang_entry *ent;

			/*
			 * Decide language.
			 */
			if (suffix)
				lang = decide_lang(suffix);
			/*
			 * Select parser.
			 * If lang == NULL then default parser is selected.
			 */
			ent = get_lang_entry(lang);
			/*
			 * Initialize parser.
			 */
			ent->init_proc(in);
			/*
			 * Execute parser.
			 * Exec_proc() is called repeatedly until returning EOF.
			 */
			while (ent->exec_proc())
				;
		}
		fputs_nl(verbatim_end, out);
	}
	fputs_nl(hr, out);
	fputs_nl(gen_name_string("BOTTOM"), out);
	fputs(comment_begin, out);
	fputs("/* ", out);
	fputs(link_format(anchor_getlinks(-1)), out);
	if (show_position)
		fprintf(out, "%s%s[+%d %s]%s", quote_space, position_begin, last_lineno, src, position_end);
	fputs(" */", out);
	fputs_nl(comment_end, out);
	if (insert_footer) {
		fputs(br, out);
		fputs(gen_insert_footer(SUBDIR), out);
	}
	fputs_nl(body_end, out);
	fputs_nl(gen_page_end(), out);
	if (!notsource)
		anchor_unload();
	close_file(fileop_out);
	close_file(fileop_in);
}
