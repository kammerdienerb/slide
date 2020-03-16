#ifndef __PARSE_H__
#define __PARSE_H__

#include "internal.h"
#include "array.h"

typedef struct {
    const char *path;
    char       *buff,
               *cursor;
    int         current_line;
} parse_info_t;

#define WORD_MAX (256)


parse_info_t parse_info_make(const char *path);
void         parse_info_free(parse_info_t *info);
void         trim_whitespace(parse_info_t *info);
void         parse_error_l(parse_info_t *info, int line, const char *fmt, ...);
void         parse_error(parse_info_t *info, const char *fmt, ...);
int          optional_word(parse_info_t *info, char *out);
int          optional_keyword(parse_info_t *info, const char* s);
long int     optional_int(parse_info_t *info, long int *out);
int          expect_word(parse_info_t *info, char *out);
int          expect_keyword(parse_info_t *info, const char *s);
int          expect_int(parse_info_t *info, long int *out);

array_t sh_split(char *s);

#endif
