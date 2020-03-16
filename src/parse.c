#include "parse.h"

parse_info_t parse_info_make(const char *path) {
    parse_info_t  info;
    FILE         *f;
    size_t        buff_size,
                  n_read;

    f = fopen(path, "r");
    if (f == NULL) {
        ERR("Could not open layout file '%s'.\n", path);
    }

    info.path         = path;
    info.current_line = 1;

    /*
     * Get the size of the file and allocate the buffer.
     */
    fseek(f, 0, SEEK_END);
    buff_size = ftell(f);
    rewind(f);

    buff_size  += 1;
    info.cursor = info.buff = malloc(buff_size);

    n_read      = fread(info.buff, 1, buff_size - 1, f);

    if (n_read != (buff_size - 1)) {
        ERR("encountered a problem attempting to read the contents of '%s' "
            "-- read %llu of %llu bytes\n", path, n_read, buff_size);
    }

    /*
     * NULL term.
     */
    info.buff[buff_size - 1] = 0;

    fclose(f);

    return info;
}

void parse_info_free(parse_info_t *info) { free(info->buff); }

void trim_whitespace(parse_info_t *info) {
    char c;

    while ((c = *info->cursor)) {
        if (isspace(c)) {
            if (c == '\n' && *(info->cursor + 1)) {
                info->current_line += 1;
            }
        } else {
            break;
        }

        info->cursor += 1;
    }
}

static void vparse_error_l(parse_info_t *info, int line, const char *fmt, va_list args) {
    fprintf(stderr, "[slide]: PARSE ERROR in '%s' :: line %d\n"
                    "         ", info->path, line);
    vfprintf(stderr, fmt, args);
    exit(1);
}

void parse_error_l(parse_info_t *info, int line, const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    vparse_error_l(info, line, fmt, args);
    va_end(args);
}

void parse_error(parse_info_t *info, const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    vparse_error_l(info, info->current_line, fmt, args);
    va_end(args);
}

int optional_word(parse_info_t *info, char *out) {
    char  c;
    char  word_buff[WORD_MAX];
    char *buff_p;
    int   len;
    int   line;

    len    = 0;
    buff_p = word_buff;

    while ((c = *info->cursor) && !isspace(c)) {
        *(buff_p++)   = c;
        info->cursor += 1;
        len          += 1;

        if (len == WORD_MAX - 1) {
            *buff_p = 0;
            parse_error(info, "word '%s' is too long -- max word length is %d\n", word_buff, WORD_MAX - 1);
        }
    }

    *buff_p = 0;

    if (out && len) {
        memcpy(out, word_buff, len + 1);
    }

    if (!len) {
        return 0;
    }

    line = info->current_line;

    trim_whitespace(info);

    return line;
}

int optional_keyword(parse_info_t *info, const char* s) {
    char  c;
    int   len;
    char *s_p,
         *cursor_save;
    int   line;

    len         = 0;
    s_p         = (char*)s;
    cursor_save = info->cursor;

    while (*info->cursor && *s_p && (*s_p == *info->cursor)) {
        len += 1;
        s_p += 1;
        info->cursor += 1;
    }

    if (*info->cursor &&
        !isspace(*info->cursor) && *info->cursor != '#') {

        info->cursor = cursor_save;
        return 0;
    }

    if (len != strlen(s)) {
        info->cursor = cursor_save;
        return 0;
    }

    line = info->current_line;

    trim_whitespace(info);

    return line;
}

long int optional_int(parse_info_t *info, long int *out) {
    long int i;
    char     buff[WORD_MAX];
    int      line;

    if (!*info->cursor || sscanf(info->cursor, "%ld", &i) == 0) {
        return 0;
    }

    sprintf(buff, "%ld", i);

    info->cursor += strlen(buff);

    if (out)    { *out = i; }

    line = info->current_line;

    trim_whitespace(info);

    return line;
}

int expect_word(parse_info_t *info, char *out) {
    int line;

    if (!(line = optional_word(info, out))) {
        parse_error(info, "expected a word\n");
    }

    return line;
}

int expect_keyword(parse_info_t *info, const char *s) {
    int line;

    if (!(line = optional_keyword(info, s))) {
        parse_error(info, "expected '%s'\n", s);
    }

    return line;
}

int expect_int(parse_info_t *info, long int *out) {
    int line;

    if (!(line = optional_int(info, out))) {
        parse_error(info, "expected an integer\n");
    }

    return line;
}

array_t sh_split(char *s) {
    array_t  r;
    char    *copy,
            *sub,
            *sub_p;
    char     c, prev;
    int      len,
             start,
             end,
             q,
             sub_len,
             i;

    r     = array_make(char*);
    copy  = strdup(s);
    len   = strlen(copy);
    start = 0;
    end   = 0;
    prev  = 0;

    while (start < len && isspace(copy[start])) { start += 1; }

    while (start < len) {
        c   = copy[start];
        q   = 0;
        end = start;

        if (c == '#' && prev != '\\') {
            break;
        } else if (c == '"') {
            start += 1;
            prev   = copy[end];
            while (end + 1 < len
            &&    (copy[end + 1] != '"' || prev == '\\')) {
                end += 1;
                prev = copy[end];
            }
            q = 1;
        } else if (c == '\'') {
            start += 1;
            prev   = copy[end];
            while (end + 1 < len
            &&    (copy[end + 1] != '\'' || prev == '\\')) {
                end += 1;
                prev = copy[end];
            }
            q = 1;
        } else {
            while (end + 1 < len
            &&     !isspace(copy[end + 1])) {
                end += 1;
            }
        }

        sub_len = end - start + 1;
        if (q && sub_len == 0 && start == len) {
            sub    = malloc(2);
            sub[0] = copy[end];
            sub[1] = 0;
        } else {
            sub   = malloc(sub_len + 1);
            sub_p = sub;
            for (i = 0; i < sub_len; i += 1) {
                c = copy[start + i];
                if (c == '\\'
                &&  i < sub_len - 1
                &&  (copy[start + i + 1] == '"'
                || copy[start + i + 1] == '\''
                || copy[start + i + 1] == '#')) {
                    continue;
                }
                *sub_p = c;
                sub_p += 1;
            }
            *sub_p = 0;
        }

        array_push(r, sub);

        end  += q;
        start = end + 1;

        while (start < len && isspace(copy[start])) { start += 1; }
    }

    free(copy);

    return r;
}
