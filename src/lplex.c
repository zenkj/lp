#include <stdio.h>
// for open
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

// for read
#include <unistd.h>

// for malloc and free
#include <stdlib.h>

// for strnlen, strcpy, memcpy
#include <string.h>

// for assert
#include <assert.h>

// for strtol and strtod
#include <errno.h>

#include "lplex.h"

// ---------------------- common definition ---------------------
#define EOS  -1    // end of source
#define NCH  0     // no char
#define BUFLEN 256
#define PATHMAX 65535
#define SRCMAX (10*1024*1024) 

typedef struct {
    const char *str;
    int type;
} Reserve;
static Reserve reserves[] = {
    {"and", TOKEN_AND},
    {"break", TOKEN_BREAK},
    {"case", TOKEN_CASE},
    {"continue", TOKEN_CONTINUE},
    {"do", TOKEN_DO},
    {"else", TOKEN_ELSE},
    {"elseif", TOKEN_ELSEIF},
    {"end", TOKEN_END},
    {"false", TOKEN_FALSE},
    {"float", TOKEN_FLOAT},
    {"for", TOKEN_FOR},
    {"fun", TOKEN_FUN},
    {"if", TOKEN_IF},
    {"in", TOKEN_IN},
    {"int", TOKEN_INTEGER},
    {"lp", TOKEN_LP},
    {"nil", TOKEN_NIL},
    {"of", TOKEN_OF},
    {"or", TOKEN_OR},
    {"regex", TOKEN_REGEX},
    {"return", TOKEN_RETURN},
    {"string", TOKEN_STRING},
    {"then", TOKEN_THEN},
    {"true", TOKEN_TRUE},
    {"while", TOKEN_WHILE},
};

typedef int (*Reader)(void *data, char*buf, int len);
typedef void (*Closer)(void *data);

typedef struct Source {
    int row;             // current row number
    int col;             // current column number
    int curr;            // current character
    char *srcname;       // source (file) name
    char buf[BUFLEN];    // cache of current source data
    int len;             // source length in the buffer
    int ptr;             // pointer to the next char in buf
    Reader read;
    Closer close;
    void *data;

    Token curr_token;
    Token next_token;
} Source;

static void clear_token(Token *t, int freebigstr) {
    char *bigstr = t->bigstr;
    t->type = TOKEN_NTOKEN;
    t->beginrow = 0;
    t->begincol = 0;
    t->endrow = 0;
    t->endcol = 0;
    t->i = 0;
    t->f = 0.0;
    t->strlen = 0;
    t->buflen = TOKEN_BUFMAX;
    t->buf[0] = 0;
    t->bigstr = NULL;

    if (freebigstr && bigstr != NULL)
        free(bigstr);
}

static void move_token(Token *src, Token *dest) {
    clear_token(dest, 1);
    memcpy(dest, src, sizeof(Token));
    clear_token(src, 0);
}

static int token_strlen(Token *t) {
    return t->strlen;
}

static const char *token_str(Token *t) {
    if (t->bigstr != NULL)
        return t->bigstr;
    return t->buf;
}

static void set_token_begin(Source *src, Token *t) {
    t->begincol = src->col;
    t->beginrow = src->row;
}

static void set_token_end(Source *src, Token *t) {
    t->endcol = src->col;
    t->endrow = src->row;
}

static int set_token_str(Token *t, const char *str) {
    int len = strnlen(str, SRCMAX);
    if (len == SRCMAX) {
        printf("too long string\n");
        return 0;
    }

    if (t->buflen == TOKEN_BUFMAX) {
        // no bigstr
        assert(t->bigstr == NULL);
        if (len <= TOKEN_BUFMAX) {
            strcpy(t->buf, str);
            t->strlen = len;
        } else {
            char *s = strdup(str);
            if (s == 0) {
                printf("no enough memory\n");
                return 0;
            }
            t->bigstr = s;
            t->buf[0] = 0;
            t->buflen = len;
            t->strlen = len;
        }
    } else {
        // has bigstr
        assert(t->bigstr != NULL);
        if (len <= TOKEN_BUFMAX) {
            strcpy(t->buf, str);
            t->strlen = len;
            t->buflen = TOKEN_BUFMAX;
            free(t->bigstr);
            t->bigstr = NULL;
        } else {
            if (len <= t->buflen) {
                strcpy(t->bigstr, str);
                t->strlen = len;
            } else {
                char *s = realloc(t->bigstr, len+1);
                if (s == NULL) {
                    printf("no enough memory\n");
                    return 0;
                }
                strcpy(s, str);
                t->bigstr = s;
                t->buflen = len;
                t->strlen = len;
            }
        }
    }
    return 1;
}

static int append_char_to_token(Token *t, int ch) {
    char *ptr = NULL;
    assert (t->strlen <= t->buflen);
    assert ((t->buflen == TOKEN_BUFMAX && t->bigstr == NULL) ||
            (t->buflen > TOKEN_BUFMAX && t->bigstr != NULL));

    if (t->buflen == TOKEN_BUFMAX) {
        ptr = t->buf;
    } else {
        ptr = t->bigstr;
    }

    assert(ptr != NULL);

    if (t->strlen == t->buflen) {
        char *newptr = NULL;
        if (ptr == t->buf) {
            newptr = (char*)malloc(TOKEN_BUFMAX*2+1);
            if (newptr == NULL) {
                return 0;
            }
            strcpy(newptr, ptr);
            *ptr = (char) 0;
            t->buflen = TOKEN_BUFMAX*2;
            t->bigstr = newptr;
        } else {
            newptr = realloc(ptr, t->buflen * 2 + 1);
            if (newptr == NULL) {
                return 0;
            }
            t->buflen *= 2;
            t->bigstr = newptr;
        }
        ptr = newptr;
    }

    ptr[t->strlen++] = (char)ch;
    ptr[t->strlen] = (char)0;

    return 1;
}

//--------------- file source -----------------------------
static int file_reader(void *data, char* buf, int len) {
    int fd = (int)data;
    ssize_t size = read(fd, buf, len);
    return (int)size;
}
static void file_closer(void *data) {
    int fd = (int)data;
    close(fd);
}

void *file_source(const char *filepath) {
    Source *src;
    char *mypath;
    int fd;

    if (filepath == NULL || strnlen(filepath, PATHMAX) == 0) {
        printf("invalid filepath\n");
        return NULL;
    }


    mypath = strdup(filepath);
    if (mypath == NULL) {
        printf("no enough memory\n");
        return NULL;
    }
    
    fd = open(filepath, O_RDONLY);
    if (fd == -1) {
        free(mypath);
        printf("can't open source file %s to read\n", filepath);
        return NULL;
    }
    
    src = (Source *)malloc(sizeof(Source));
    if (src == NULL) {
        free(mypath);
        close(fd);
        printf("no enough memory\n");
        return NULL;
    }

    src->row = 1;
    src->col = 0;
    src->curr = NCH;
    src->srcname = mypath;
    mypath = NULL;
    src->len = 0;
    src->ptr = 0;
    src->read = file_reader;
    src->close = file_closer;
    src->data = (void*)fd;

    clear_token(&src->curr_token, 0);
    clear_token(&src->next_token, 0);

    return src;
}

//--------------- string source -----------------------------
typedef struct {
    int ptr;
    int len;
    const char *str;
} StringSource;

static int string_reader(void *data, char* buf, int len) {
    StringSource *ss = (StringSource*)data;
    int left = ss->len - ss->ptr;
    int tocpy;
    if (left <= 0) {
        tocpy = 0;
    } else if (left <= len) {
        tocpy = left;
    } else {
        tocpy = len;
    }

    if (tocpy > 0) {
        memcpy(buf, &ss->str[ss->ptr], tocpy);
        ss->ptr += tocpy;
    }

    return tocpy;
}

static void string_closer(void *data) {
    free(data);
}

void *string_source(const char *str) {
    Source *src;
    StringSource *ss;
    int len;

    if (src == NULL) {
        printf("invalid string\n");
        return NULL;
    }

    len = strnlen(str, SRCMAX);
    if (len >= SRCMAX) {
        printf("too long source string\n");
        return NULL;
    }

    src = (Source*)malloc(sizeof(Source));
    if (src == NULL) {
        printf("no enough memory\n");
        return NULL;
    }

    ss = (StringSource*)malloc(sizeof(StringSource));
    if (ss == NULL) {
        free(src);
        printf("no enough memory\n");
        return NULL;
    }
    ss->ptr = 0;
    ss->len = len;
    ss->str = str;

    src->row = 1;
    src->col = 0;
    src->curr = NCH;
    src->srcname = "<string>";
    src->len = 0;
    src->ptr = 0;
    src->read = string_reader;
    src->close = string_closer;
    src->data = (void*)ss;

    clear_token (&src->curr_token, 0);
    clear_token (&src->next_token, 0);


    return src;
}


//--------------- close source -----------------------------
void close_source(void *data) {
    Source *src = (Source *)data;
    if (src->srcname != NULL) {
        free(src->srcname);
        src->srcname = NULL;
    }

    if (src->close != NULL) {
        src->close(src->data);
    }

    free(src);
}

//--------------- scan source -----------------------------
static void refill(Source *src) {
    int len;
    src->ptr = 0;
    len = src->read(src->data, src->buf, BUFLEN);
    if (len <= 0) {
        src->len = 0;
        src->buf[src->len++] = EOS;
        return;
    } else {
        src->len = len;
    }
}

static int curr(Source *src) {
//    if (src->curr == NCH) {
//        return next(src);
//    }
    return src->curr;
}

static int next(Source *src) {
    // if already at the end, no need to get next
    if (src->curr == EOS)
        return EOS;

    if (src->ptr >= src->len) {
        refill(src);
    }
    assert(src->ptr < src->len);
    src->curr = src->buf[src->ptr++];
    src->col ++;
    return src->curr;
}

static int peek(Source *src) {
    // if already at the end, no need not try to get next
    if (src->curr == EOS)
        return EOS;

    if (src->ptr >= src->len) {
        refill(src);
    }
    assert(src->ptr < src->len);
    return src->buf[src->ptr];
}


//--------------- consume various tokens  -----------
// all these verious consume_xxx functions will consume
// corresponding items, set col/row, move to the end of
// the item(not the next char of the item).
//
// Common Interface Specification:
// 1. at the entry of these consume_xxx functions, the current
//    character is the end of the last item.
// 2. at the exit of these functions, the current character is
//    the end of the current item.

#define STRTYPE_TOEOL   0 // string till end of line
#define STRTYPE_SINGLE  1 // single-line string quoted by '
#define STRTYPE_DOUBLE  2 // single-line string quoted by "
#define STRTYPE_SINGLEM 3 // multi-line string quoted by '''
#define STRTYPE_DOUBLEM 4 // multi-line string quoted by """

#define is_newline_char(ch) ((ch) == '\n' || (ch) == '\r')
#define is_hex_char(ch) (((ch) >= '0' && (ch) <= '9') || ((ch) >= 'a' && (ch) <= 'f') || ((ch) >= 'A' && (ch) <= 'F'))
#define is_oct_char(ch) ((ch) >= '0' && (ch) <= '7')
#define is_dec_char(ch) ((ch) >= '0' && (ch) <= '9')
#define is_id_char(ch) (((ch) >= '0' && (ch) <= '9') || ((ch) >= 'a' && (ch) <= 'z') || ((ch) >= 'A' && (ch) <= 'Z') || (ch) == '_')
#define is_id_first_char(ch) (((ch) >= 'a' && (ch) <= 'z') || ((ch) >= 'A' && (ch) <= 'Z') || (ch) == '_')


static int error_token(Token *t, const char *msg) {
    t->type = TOKEN_ERROR;
    set_token_str(t, msg);
    return 0;
}

static int consume_newline(Source *src) {
    int curr = next(src);
    int n;
    assert(is_newline_char(curr));

    n = peek(src);

    if (is_newline_char(n)) {
        if (curr != n) {
            next(src);
        }
    }

    src->row ++;
    src->col = 0;

    return 0;
}

// the string begin char(s)(' or " or ''' or """) are already consumed
static int gather_str(Source *src, Token *dest, int type) {
    int curr;
    switch (type) {
        case STRTYPE_TOEOL:
            while (1) {
                curr = peek(src);
                switch (curr) {
                    case EOS:
                        next(src);
                        return 1;
                    case '\n': case '\r':
                        consume_newline(src);
                        return 1;
                    case '\\':
                        next(src);
                        curr = peek(src);
                        if (is_newline_char(curr)) {
                            consume_newline(src);
                            curr = '\n';
                        } else curr = '\\';
                        break;
                    default:
                        next(src);
                        break;
                }
                append_char_to_token(dest, curr);
            }
            break;

        case STRTYPE_SINGLE:
            while (1) {
                curr = peek(src);
                switch (curr) {
                    case EOS: case '\n': case '\r':
                        return error_token(dest, "no close \"'\"");
                    case '\'':
                        next(src);
                        return 1;
                    case '\\':
                        next(src);
                        curr = peek(src);
                        if (curr == '\'') {
                            next(src);
                        } else if (is_newline_char(curr)) {
                            consume_newline(src);
                            curr = '\n';
                        } else {
                            curr = '\\';
                        }
                        break;
                    default:
                        next(src);
                        break;
                }
                append_char_to_token(dest, curr);
            }
            break;
        case STRTYPE_DOUBLE:
            while (1) {
                curr = peek(src);
                switch (curr) {
                    case EOS: case '\n': case '\r':
                        return error_token(dest, "no close '\"'");
                    case '"':
                        next(src);
                        return 1;
                    case '\\':
                        next(src);
                        curr = peek(src);
                        switch (curr) {
                            case EOS: curr = '\\'; break;
                            case 'n': curr = '\n'; next(src); break;
                            case 'r': curr = '\r'; next(src); break;
                            case 't': curr = '\t'; next(src); break;
                            case '\n': case '\r':
                                consume_newline(src);
                                curr = '\n';
                                break;
                            default: // \" and \\ is the same as normal chars
                                next(src);
                                break;
                        }
                        break;
                    default:
                        next(src);
                        break;
                }
                append_char_to_token(dest, curr);
            }
            break;
        case STRTYPE_SINGLEM:
            while (1) {
                curr = peek(src);
                switch (curr) {
                    case EOS:
                        return error_token(dest, "no close \"'''\"");
                    case '\'':
                        next(src);
                        curr = peek(src);
                        if (curr == '\'') {
                            next(src);
                            curr = peek(src);
                            if (curr == '\'') {
                                next(src);
                                return 1;
                            } else {
                                append_char_to_token(dest, '\'');
                                curr = '\'';
                            }
                        } else curr = '\'';
                        break;
                    case '\r': case '\n':
                        consume_newline(src);
                        curr = '\n';
                        break;
                    default:
                        next(src);
                        break;
                }
                append_char_to_token(dest, curr);
            }
            break;
        case STRTYPE_DOUBLEM:
            while (1) {
                curr = peek(src);
                switch (curr) {
                    case EOS:
                        return error_token(dest, "no close '\"\"\"'");
                    case '"':
                        next(src);
                        curr = peek(src);
                        if (curr == '"') {
                            next(src);
                            curr = peek(src);
                            if (curr == '"') {
                                next(src);
                                return 1;
                            } else {
                                append_char_to_token(dest, '"');
                                curr = '"';
                            }
                        } else curr = '"';
                        break;
                    case '\r': case '\n':
                        consume_newline(src);
                        curr = '\n';
                        break;
                    default:
                        next(src);
                        break;
                }
                append_char_to_token(dest, curr);
            }
            break;
        default:
            printf("invalid string type\n");
    }

    // something must be wrong, because all success handling have already returned
    return 0;
}

static int consume_string(Source *src, Token *dest) {
    int curr = next(src);
    int n;
    int result;

    dest->type = TOKEN_STRING;
    set_token_begin(src, dest);
    switch (curr) {
        case '\'':
            n = peek(src);
            switch (n) {
                case EOS: case '\n': case '\r':
                    set_token_end(src, dest);
                    return error_token(dest, "no close \"'\"");
                case '\'':
                    curr = next(src);
                    n = peek(src);
                    if (n == '\'') {
                        next(src);
                        result = gather_str(src, dest, STRTYPE_SINGLEM);
                        set_token_end(src, dest);
                        return result;
                    } else {
                        set_token_str(dest, "");
                        set_token_end(src, dest);
                        return 1;
                    }
                    break;
                default:
                    result = gather_str(src, dest, STRTYPE_SINGLE);
                    set_token_end(src, dest);
                    return result;
            }
            break;
        case '"':
            n = peek(src);
            switch (n) {
                case EOS: case '\n': case '\r':
                    set_token_end(src, dest);
                    return error_token(dest, "no close '\"'");
                case '"':
                    curr = next(src);
                    n = peek(src);
                    if (n == '"') {
                        next(src);
                        result = gather_str(src, dest, STRTYPE_DOUBLEM);
                        set_token_end(src, dest);
                        return result;
                    } else {
                        set_token_str(dest, "");
                        set_token_end(src, dest);
                        return 1;
                    }
                    break;
                default:
                    result = gather_str(src, dest, STRTYPE_DOUBLE);
                    set_token_end(src, dest);
                    return result;
            }
            break;
        default:
            dest->type = TOKEN_ERROR;
            return 0;
    }

    return 0;
}

static int consume_comment(Source *src, Token *dest) {
    int curr = next(src);
    assert(curr == '#');

    while (1) {
        curr = peek(src);
        switch (curr) {
            case EOS:
                next(src);
                return 1;
            case '\n': case '\r':
                consume_newline(src);
                return 1;
            case '\\':
                next(src);
                curr = peek(src);
                if (is_newline_char(curr)) {
                    consume_newline(src);
                }
                break;
            default:
                next(src);
                break;
        }
    }
    return 0;
}

static int consume_section(Source *src, Token *dest) {
    int curr = next(src);
    int result;
    assert(curr == '@');

    set_token_begin(src, dest);
    curr = peek(src);
    switch(curr) {
        case '=': case '>': case '<':
        case '1': case '2': case '3': case '4': case '5':
        case '6': case '7': case '8': case '9':
            next(src);
            dest->type = TOKEN_SECTION;
            dest->i = curr;
            result = gather_str(src, dest, STRTYPE_TOEOL);
            set_token_end(src, dest);
            return result;
        default:
            next(src);
            set_token_end(src, dest);
            return error_token(dest, "invalid section specification");
    }

    return 0;
}

static int str2int(Token *dest, int base) {
    long l;
    const char *beginptr = token_str(dest);
    char *endptr;
    if (token_strlen(dest) == 0) {
        dest->type = TOKEN_ERROR;
        set_token_str(dest, "malformed number");
        return 0;
    }
    errno = 0;
    l = strtol(beginptr, &endptr, base);

    if ((errno != 0) || endptr == beginptr || *endptr != '\0') {
        return error_token(dest, "invalid number format");
    }

    dest->i = l;
    return 1;
}

static int str2float(Token *dest) {
    double l;
    const char *beginptr = token_str(dest);
    char *endptr;
    if (token_strlen(dest) == 0) {
        return error_token(dest, "malformed number");
    }
    errno = 0;
    l = strtod(beginptr, &endptr);

    if ((errno != 0) || endptr == beginptr || *endptr != '\0') {
        return error_token(dest, "invalid number format");
    }

    dest->f = l;
    return 1;
}

static int gather_hex_int(Source *src, Token *dest) {
    int curr;
    while (1) {
        curr = peek(src);
        if (is_hex_char(curr)) {
            next(src);
            append_char_to_token(dest, curr);
        } else if (is_id_char(curr)) {
            next(src);
            return error_token(dest, "malformed number");
        } else {
            break;
        }
    }

    return str2int(dest, 16);
}

static int gather_bin_int(Source *src, Token *dest) {
    int curr;
    while (1) {
        curr = peek(src);
        if (curr == '0' || curr == '1') {
            next(src);
            append_char_to_token(dest, curr);
        } else if (is_id_char(curr)) {
            next(src);
            return error_token(dest, "malformed number");
        } else {
            break;
        }
    }

    return str2int(dest, 2);
}

static int gather_oct_int(Source *src, Token *dest) {
    int curr;
    while (1) {
        curr = peek(src);
        if (is_oct_char(curr)) {
            next(src);
            append_char_to_token(dest, curr);
        } else if (is_id_char(curr)) {
            next(src);
            return error_token(dest, "malformed number");
        } else {
            break;
        }
    }

    return str2int(dest, 8);
}

static int gather_dec(Source *src, Token *dest) {
    int hasptr = 0; // has xiaoshudian
    int hasexp = 0; // has exponent
    int curr;
    
    while (1) {
        curr = peek(src);
        if (is_dec_char(curr)) {
            next(src);
            append_char_to_token(dest, curr);
        } else if (curr == '.') {
            next(src);
            if (hasptr || hasexp) {
                // cannot has more than one pointer
                // cannot has pointer in exponent
                return error_token(dest, "malformed number");
            } else {
                hasptr = 1;
            }
            append_char_to_token(dest, curr);
        } else if (curr == 'e' || curr == 'E') {
            next(src);
            if (hasexp) {
                // cannot have more than one exponent
                return error_token(dest, "malformed number");
            }
            hasexp = 1;
            append_char_to_token(dest, curr);

            curr = peek(src);
            if (curr == '+' || curr == '-' || is_dec_char(curr)) {
                next(src);
                append_char_to_token(dest, curr);
            } else {
                return error_token(dest, "malformed number");
            }
        } else if (is_id_char(curr)) {
            next(src);
            return error_token(dest, "malformed number");
        } else {
            break;
        }
    }

    if (hasptr || hasexp) {
        //float
        dest->type = TOKEN_FLOAT;
        return str2float(dest);
    } else {
        // integer
        dest->type = TOKEN_INTEGER;
        return str2int(dest, 10);
    }
}

static int consume_normal_number(Source *src, Token *dest) {
    int curr = next(src);
    int result;
    assert(is_dec_char(curr) && curr != '0');
    dest->type = TOKEN_INTEGER;
    set_token_begin(src, dest);

    append_char_to_token(dest, curr);
    result = gather_dec(src, dest);
    set_token_end(src, dest);
    return result;
}

static int consume_special_number(Source *src, Token *dest) {
    int curr = next(src);
    int result;
    assert(curr == '0');

    dest->type = TOKEN_INTEGER;
    set_token_begin(src, dest);
    
    curr = peek(src);
    switch (curr) {
        case 'x': case 'X': // do not support hexdecimal non-integer
            next(src);
            result = gather_hex_int(src, dest);
            set_token_end(src, dest);
            return result;
        case 'b': case 'B': // do not support binary non-integer
            next(src);
            result = gather_bin_int(src, dest);
            set_token_end(src, dest);
            return result;
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': // do not support octal non-integer
            result = gather_oct_int(src, dest);
            set_token_end(src, dest);
            return result;
        case '.':
            // it's normal number(float)
            append_char_to_token(dest, '0');
            result = gather_dec(src, dest);
            set_token_end(src, dest);
            return result;
        default:
            if (is_id_first_char(curr) || curr == '8' || curr == '9') {
                next(src);
                set_token_str(dest, "invalid number format");
                dest->type = TOKEN_ERROR;
                set_token_end(src, dest);
                return 0;
            } else {
                dest->i = 0;
                set_token_end(src, dest);
                return 1;
            }
    }
}

static int check_reserve(Token *t) {
    int begin = 0;
    int end = sizeof(reserves) / sizeof(reserves[0]) - 1;
    int m;
    const char *str = token_str(t);
    int res;

    while (begin <= end) {
        m = (begin + end) / 2;
        res = strcmp(reserves[m].str, str);
        if (res == 0) {
            t->type = reserves[m].type;
            return 1;
        } else if (res > 0) {
            end = m - 1;
        } else {
            begin = m + 1;
        }
    }
    return 1;
}

static int consume_id(Source *src, Token *dest) {
    int curr = next(src);
    int result;

    assert(is_id_first_char(curr));

    set_token_begin(src, dest);
    dest->type = TOKEN_IDENTIFIER;
    append_char_to_token(dest, curr);

    while (1) {
        curr = peek(src);
        if (is_id_char(curr)) {
            next(src);
            append_char_to_token(dest, curr);
        } else {
            break;
        }
    }
    set_token_end(src, dest);

    return check_reserve(dest);
}

static int consume_default(Source *src, Token *dest) {
    int curr = peek(src);

    if (is_id_first_char(curr)) {
        return consume_id(src, dest);
    } else {
        next(src);
        set_token_begin(src, dest);
        set_token_end(src, dest);
        dest->type = TOKEN_ERROR;
        set_token_str(dest, "invalid character");
        return 0;
    }
}

//--------------- token -----------------------------

// return 1: ok
//        0: error
static int next_token_to(Source *src, Token *dest) {
    int curr;

    clear_token(dest, 1);

    while(1) {
        curr = peek(src);
        switch(curr) {
            case EOS:
                next(src);
                dest->type = TOKEN_EOS;
                return 1;
            case '#':
                consume_comment(src, dest);
                break;
            case '@':
                return consume_section(src, dest);
            case '"': case '\'':
                return consume_string(src, dest);
            case '0':
                return consume_special_number(src, dest);
            case '1': case '2': case '3': case '4': case '5':
            case '6': case '7': case '8': case '9': 
                return consume_normal_number(src, dest);
                break;
            case '\r': case '\n':
                consume_newline(src);
                break;
            case '\t': case ' ':
                next(src);
                break;
            case ':': case ';':
            case '+': case '-':
            case '/': case '%':
            case '(': case ')':
            case '{': case '}':
            case '[': case ']':
            case '|': case ',':
                next(src);
                set_token_begin(src, dest);
                set_token_end(src, dest);
                dest->type = curr;
                return 1;
            case '*':
                next(src);
                set_token_begin(src, dest);
                curr = peek(src);
                if (curr == '*') {
                    next(src);
                    set_token_end(src, dest);
                    dest->type = TOKEN_EXPONENT;
                } else {
                    set_token_end(src, dest);
                    dest->type = '*';
                }
                return 1;
            case '=':
                next(src);
                set_token_begin(src, dest);
                curr = peek(src);
                if (curr == '=') {
                    next(src);
                    set_token_end(src, dest);
                    dest->type = TOKEN_EQ;
                } else {
                    set_token_end(src, dest);
                    dest->type = '=';
                }
                return 1;
            case '!':
                next(src);
                set_token_begin(src, dest);
                curr = peek(src);
                if (curr == '=') {
                    next(src);
                    set_token_end(src, dest);
                    dest->type = TOKEN_NEQ;
                } else {
                    set_token_end(src, dest);
                    dest->type = '!';
                }
                return 1;
            case '>':
                next(src);
                set_token_begin(src, dest);
                curr = peek(src);
                if (curr == '=') {
                    next(src);
                    set_token_end(src, dest);
                    dest->type = TOKEN_GE;
                } else {
                    set_token_end(src, dest);
                    dest->type = '>';
                }
                return 1;
            case '<':
                next(src);
                set_token_begin(src, dest);
                curr = peek(src);
                if (curr == '=') {
                    next(src);
                    set_token_end(src, dest);
                    dest->type = TOKEN_LE;
                } else {
                    set_token_end(src, dest);
                    dest->type = '<';
                }
                return 1;
            case '.':
                next(src);
                set_token_begin(src, dest);
                curr = peek(src);
                if (curr == '.') {
                    next(src);
                    set_token_end(src, dest);
                    dest->type = TOKEN_RANGE;
                } else {
                    set_token_end(src, dest);
                    dest->type = '.';
                }
                return 1;
            default:
                return consume_default(src, dest);
        }
    }
}

Token *curr_token(void *data) {
    Source *src;
    src = (Source*)data;
    return &src->curr_token;
}

Token *next_token(void *data) {
    Source *src;
    src = (Source*)data;

    // if already at the end or error occurred, no need to retry
    if (src->curr_token.type == TOKEN_EOS || src->curr_token.type == TOKEN_ERROR)
        return &src->curr_token;

    if (src->next_token.type != TOKEN_NTOKEN) {
        move_token(&src->next_token, &src->curr_token);
        return &src->curr_token;
    }

    next_token_to(src, &src->curr_token);
    return &src->curr_token;
}

Token *peek_token(void *data) {
    Source *src;
    src = (Source*)data;

    // if already at the end or error occurred, no need to retry
    if (src->curr_token.type == TOKEN_EOS || src->curr_token.type == TOKEN_ERROR)
        return &src->curr_token;

    if (src->next_token.type != TOKEN_NTOKEN) {
        return &src->next_token;
    }

    next_token_to(src, &src->next_token);
    return &src->next_token;
}

// ------------------------- print token ---------------------------------
static void pt(Token *t, char *buf, const char *msg) {
    sprintf(buf, "%3d,%3d - %3d,%3d: %s", t->beginrow, t->begincol, t->endrow, t->endcol, msg);
}
static void ptc(Token *t, char *buf) {
    sprintf(buf, "%3d,%3d - %3d,%3d: %c", t->beginrow, t->begincol, t->endrow, t->endcol, t->type);
}
static void ptu(Token *t, char *buf) {
    sprintf(buf, "%3d,%3d - %3d,%3d: unknown%d)", t->beginrow, t->begincol, t->endrow, t->endcol, t->type);
}
static void pti(Token *t, char *buf, const char *msg) {
    sprintf(buf, "%3d,%3d - %3d,%3d: %s %ld", t->beginrow, t->begincol, t->endrow, t->endcol, msg, t->i);
}
static void ptf(Token *t, char *buf, const char *msg) {
    sprintf(buf, "%3d,%3d - %3d,%3d: %s %lf", t->beginrow, t->begincol, t->endrow, t->endcol, msg, t->f);
}
static void pts(Token *t, char *buf, int len, const char *msg) {
    sprintf(buf, "%3d,%3d - %3d,%3d: %s ", t->beginrow, t->begincol, t->endrow, t->endcol, msg);
    if (token_strlen(t) + strlen(buf) < len)
        strcat(buf, token_str(t));
    else strcat(buf, "...");
}
static void ptcs(Token *t, char *buf, int len, const char *msg) {
    sprintf(buf, "%3d,%3d - %3d,%3d: %s %c ", t->beginrow, t->begincol, t->endrow, t->endcol, msg, t->i);
    if (token_strlen(t) + strlen(buf) < len)
        strcat(buf, token_str(t));
    else strcat(buf, "...");
}
static void ptis(Token *t, char *buf, int len, const char *msg) {
    sprintf(buf, "%3d,%3d - %3d,%3d: %s %ld ", t->beginrow, t->begincol, t->endrow, t->endcol, msg, t->i);
    if (token_strlen(t) + strlen(buf) < len)
        strcat(buf, token_str(t));
    else strcat(buf, "...");
}

static void ptfs(Token *t, char *buf, int len, const char *msg) {
    sprintf(buf, "%3d,%3d - %3d,%3d: %s %lf ", t->beginrow, t->begincol, t->endrow, t->endcol, msg, t->f);
    if (token_strlen(t) + strlen(buf) < len)
        strcat(buf, token_str(t));
    else strcat(buf, "...");
}

void print_token(Token *t, char *buf, int len) {
    int i = 0;
    int type;
    const char *str;
    assert(t != NULL);
    assert(buf != NULL);
    assert(len > 256);
    type = t->type;
    if (type >= 32 && type <= 127) {
        // printable type
        ptc(t, buf);
        return;
    }
    switch (t->type) {
        case TOKEN_ERROR:
            pts(t, buf, len, "error");
            break;
        case TOKEN_NTOKEN:
            pt(t, buf, "no-token");
            break;
        case TOKEN_EOS:
            pt(t, buf, "EOS");
            break;
        case TOKEN_SECTION:
            ptcs(t, buf, len, "section");
            break;
        case TOKEN_IDENTIFIER:
            pts(t, buf, len, "id");
            break;
        case TOKEN_INTEGER:
            pti(t, buf, "integer");
            break;
        case TOKEN_FLOAT:
            ptf(t, buf, "float");
            break;
        case TOKEN_STRING:
            pts(t, buf, len, "string");
            break;
        case TOKEN_REGEX:
            pts(t, buf, len, "regex");
            break;
        case TOKEN_IF:
            pt(t, buf, "if");
            break;
        case TOKEN_THEN:
            pt(t, buf, "then");
            break;
        case TOKEN_ELSE:
            pt(t, buf, "else");
            break;
        case TOKEN_ELSEIF:
            pt(t, buf, "elseif");
            break;
        case TOKEN_END:
            pt(t, buf, "end");
            break;
        case TOKEN_FOR:
            pt(t, buf, "for");
            break;
        case TOKEN_IN:
            pt(t, buf, "in");
            break;
        case TOKEN_DO:
            pt(t, buf, "do");
            break;
        case TOKEN_WHILE:
            pt(t, buf, "while");
            break;
        case TOKEN_BREAK:
            pt(t, buf, "break");
            break;
        case TOKEN_CONTINUE:
            pt(t, buf, "continue");
            break;
        case TOKEN_RETURN:
            pt(t, buf, "return");
            break;
        case TOKEN_AND:
            pt(t, buf, "and");
            break;
        case TOKEN_OR:
            pt(t, buf, "or");
            break;
        case TOKEN_CASE:
            pt(t, buf, "case");
            break;
        case TOKEN_OF:
            pt(t, buf, "of");
            break;
        case TOKEN_LP:
            pt(t, buf, "lp");
            break;
        case TOKEN_FUN:
            pt(t, buf, "fun");
            break;
        case TOKEN_TRUE:
            pt(t, buf, "true");
            break;
        case TOKEN_FALSE:
            pt(t, buf, "false");
            break;
        case TOKEN_NIL:
            pt(t, buf, "nil");
            break;
        case TOKEN_EXPONENT:
            pt(t, buf, "**");
            break;
        case TOKEN_EQ:
            pt(t, buf, "==");
            break;
        case TOKEN_NEQ:
            pt(t, buf, "!=");
            break;
        case TOKEN_GE:
            pt(t, buf, ">=");
            break;
        case TOKEN_LE:
            pt(t, buf, "<=");
            break;
        case TOKEN_RANGE:
            pt(t, buf, "..");
            break;
        default:
            ptu(t, buf);
    }
}

