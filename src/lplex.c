//1 --------------------- import ---------------------------
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

//1 ---------------------- common definition ---------------------
#define EOS  -1    // end of source
#define NCH  0     // no char
#define BUFLEN 256
#define PATHMAX 65535
#define SRCMAX (10*1024*1024) 

typedef int (*Reader)(void *data, char*buf, int len);
typedef void (*Closer)(void *data);

typedef struct Source {
    int row;             // current row number
    int col;             // current column number
    int curr;            // current character
    int next1;           // next character
    int next2;           // next to next character
    int next3;           // next to next to next character
    char *srcname;       // source (file) name
    char buf[BUFLEN];    // cache of current source data
    int len;             // source length in the buffer
    int ptr;             // pointer to the next char in buf
    Reader read;
    Closer close;
    void *data;

    Token curr_token;
    Token next_token1;
    Token next_token2;
    Token next_token3;
} Source;


//1 ----------------- token manipulation ------------------------------
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

static void set_token_type(Token *t, int type) {
    t->type = type;
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

//1 --------------------- reserve words manipulation -------------------------
typedef struct {
    const char *str;
    int type;
} Reserve;

static Reserve reserves[] = {
    {"after", TOKEN_KW_AFTER},
    {"and", TOKEN_KW_AND},
    {"break", TOKEN_KW_BREAK},
    {"case", TOKEN_KW_CASE},
    {"catch", TOKEN_KW_CATCH},
    {"continue", TOKEN_KW_CONTINUE},
    {"do", TOKEN_KW_DO},
    {"else", TOKEN_KW_ELSE},
    {"elseif", TOKEN_KW_ELSEIF},
    {"end", TOKEN_KW_END},
    {"export", TOKEN_KW_EXPORT},
    {"false", TOKEN_KW_FALSE},
    {"finally", TOKEN_KW_FINALLY},
    {"float", TOKEN_KW_FLOAT},
    {"for", TOKEN_KW_FOR},
    {"fun", TOKEN_KW_FUN},
    {"if", TOKEN_KW_IF},
    {"import", TOKEN_KW_IMPORT},
    {"in", TOKEN_KW_IN},
    {"int", TOKEN_KW_INTEGER},
    {"local", TOKEN_KW_LOCAL},
    {"lp", TOKEN_KW_LP},
    {"nil", TOKEN_KW_NIL},
    {"of", TOKEN_KW_OF},
    {"or", TOKEN_KW_OR},
    {"return", TOKEN_KW_RETURN},
    {"string", TOKEN_KW_STRING},
    {"then", TOKEN_KW_THEN},
    {"true", TOKEN_KW_TRUE},
    {"try", TOKEN_KW_TRY},
    {"while", TOKEN_KW_WHILE},
};

static int search_reserve(const char* str) {
    int begin = 0;
    int end = sizeof(reserves) / sizeof(reserves[0]) - 1;
    int m;
    int res;
    while (begin <= end) {
        m = (begin + end) / 2;
        res = strcmp(reserves[m].str, str);
        if (res == 0) {
            return reserves[m].type;
        } else if (res > 0) {
            end = m - 1;
        } else {
            begin = m + 1;
        }
    }
    return TOKEN_IDENTIFIER;
}

// check whether the collected identifier is a reserved word
static int check_reserve(Token *t) {
    const char *str = token_str(t);
    int type;

    assert(t->type == TOKEN_IDENTIFIER);

    if (t->i == 1) {
        type = search_reserve(str);
        set_token_type(t, type);
    } else {
        int i;
        const char *ptr = str;
        for (i=0; i < t->i; i++) {
            int len = strlen(ptr);
            type = search_reserve(ptr);
            if (type != TOKEN_IDENTIFIER) {
                set_token_type(t, TOKEN_ERROR);
                set_token_str(t, "reserve word can't be used as identifier");
                return 0;
            }
            ptr += len+1;
        }
    }
    return 1;
}


//1 -------------- program source manipulation --------------
//2 --------------- file source -----------------------------
static int file_reader(void *data, char* buf, int len) {
    int fd = (int)(long)data;
    ssize_t size = read(fd, buf, len);
    return (int)size;
}
static void file_closer(void *data) {
    int fd = (int)(long)data;
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
    src->next1 = NCH;
    src->next2 = NCH;
    src->next3 = NCH;
    src->srcname = mypath;
    mypath = NULL;
    src->len = 0;
    src->ptr = 0;
    src->read = file_reader;
    src->close = file_closer;
    src->data = (void*)(long)fd;

    clear_token(&src->curr_token, 0);
    clear_token(&src->next_token1, 0);
    clear_token(&src->next_token2, 0);
    clear_token(&src->next_token3, 0);

    return src;
}

//2 --------------- string source -----------------------------
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
    src->next1 = NCH;
    src->next2 = NCH;
    src->next3 = NCH;
    src->srcname = "<string>";
    src->len = 0;
    src->ptr = 0;
    src->read = string_reader;
    src->close = string_closer;
    src->data = (void*)ss;

    clear_token (&src->curr_token, 0);
    clear_token (&src->next_token1, 0);
    clear_token (&src->next_token2, 0);
    clear_token (&src->next_token3, 0);


    return src;
}


//2 --------------- close source -----------------------------
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

//2 --------------- scan source -----------------------------
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

    if (src->next1 != NCH) {
        src->curr = src->next1;
        src->next1 = src->next2;
        src->next2 = src->next3;
        src->next3 = NCH;
    } else {
        assert(src->next1 == NCH);
        assert(src->next2 == NCH);
        assert(src->next3 == NCH);
        if (src->ptr >= src->len) {
            refill(src);
        }
        assert(src->ptr < src->len);
        src->curr = src->buf[src->ptr++];
    }
    src->col ++;
    return src->curr;
}

static int peek1(Source *src) {
    // if already at the end, no need to try to get next
    if (src->curr == EOS)
        return EOS;

    if (src->next1 != NCH) {
        return src->next1;
    } else {
        assert(src->next2 == NCH && src->next3 == NCH);
        if (src->ptr >= src->len) {
            refill(src);
        }
        assert(src->ptr < src->len);
        src->next1 = src->buf[src->ptr++];
        return src->next1;
    }
}

static int peek2(Source *src) {
    // if already at the end, no need to try to get next
    if (src->curr == EOS || src->next1 == EOS)
        return EOS;

    assert(src->next1 != NCH);
    if (src->next2 != NCH) {
        return src->next2;
    } else {
        assert(src->next3 == NCH);
        if (src->ptr >= src->len) {
            refill(src);
        }
        assert(src->ptr < src->len);
        src->next2 = src->buf[src->ptr++];
        return src->next2;
    }
}

static int peek3(Source *src) {
    // if already at the end, no need to try to get next
    if (src->curr == EOS || src->next1 == EOS | src->next2 == EOS)
        return EOS;

    assert(src->next1 != NCH);
    assert(src->next2 != NCH);
    if (src->next3 != NCH) {
        return src->next3;
    } else {
        if (src->ptr >= src->len) {
            refill(src);
        }
        assert(src->ptr < src->len);
        src->next3 = src->buf[src->ptr++];
        return src->next3;
    }
}


// --------------- consume various tokens  -----------
// all these verious consume_xxx functions will consume
// corresponding items, set col/row, move to the end of
// the item(not the next char of the item).
//
// Common Interface Specification:
// 1. at the entry of these consume_xxx functions, the current
//    character is the end of the last token.
// 2. at the exit of these functions, the current character is
//    the end of the current(just consumed) token.

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

    n = peek1(src);

    if (is_newline_char(n)) {
        if (curr != n) {
            next(src);
        }
    }

    src->row ++;
    src->col = 0;

    return 0;
}

// consume string functions. if the token is provided, the string content
// will be gathered. the string begin char(s)(' or " or ''' or """) are
// already consumed.
// 1. consume the string till end of the current line
static int consume_str_till_eol(Source *src, Token *dest) {
    int curr;
    while (1) {
        curr = peek1(src);
        switch (curr) {
            case EOS:
                next(src);
                return 1;
            case '\n': case '\r':
                consume_newline(src);
                return 1;
            case '\\':
                next(src);
                if (is_newline_char(peek1(src))) {
                    consume_newline(src);
                    curr = '\n';
                }
                break;
            default:
                next(src);
                break;
        }
        if (dest != NULL) {
            append_char_to_token(dest, curr);
        }
    }
    return 0;
}
// consume the string started by '
static int consume_str_till_single(Source *src, Token *dest) {
    int curr;
    while (1) {
        curr = peek1(src);
        switch (curr) {
            case EOS: case '\n': case '\r':
                if (dest != NULL) {
                    return error_token(dest, "no close \"'\"");
                } else {
                    return 0;
                }
            case '\'':
                next(src);
                return 1;
            case '\\':
                next(src);
                curr = peek1(src);
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
        if (dest != NULL) {
            append_char_to_token(dest, curr);
        }
    }

    return 0;
}
// consume the string started by "
static int consume_str_till_double(Source *src, Token *dest) {
    int curr;
    while (1) {
        curr = peek1(src);
        switch (curr) {
            case EOS: case '\n': case '\r':
                if (dest != NULL) {
                    return error_token(dest, "no close '\"'");
                } else {
                    return 0;
                }
            case '"':
                next(src);
                return 1;
            case '\\':
                next(src);
                curr = peek1(src);
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
        if (dest != NULL) {
            append_char_to_token(dest, curr);
        }
    }

    return 0;
}

// consume the multi-line string started by '''
static int consume_str_till_singlem(Source *src, Token *dest) {
    int curr;
    while (1) {
        curr = peek1(src);
        switch (curr) {
            case EOS:
                if (dest != NULL) {
                    return error_token(dest, "no close \"'''\"");
                } else {
                    return 0;
                }
            case '\'':
                next(src);
                if (peek1(src) == '\'' && peek2(src) == '\'') {
                    next(src); next(src);
                    return 1;
                }
                break;
            case '\r': case '\n':
                consume_newline(src);
                curr = '\n';
                break;
            default:
                next(src);
                break;
        }
        if (dest != NULL) {
            append_char_to_token(dest, curr);
        }
    }

    return 0;
}

// consume the multi-line string started by """
static int consume_str_till_doublem(Source *src, Token *dest) {
    int curr;
    while (1) {
        curr = peek1(src);
        switch (curr) {
            case EOS:
                if (dest != NULL) {
                    return error_token(dest, "no close '\"\"\"'");
                } else {
                    return 0;
                }
            case '"':
                next(src);
                if (peek1(src) == '"' && peek2(src) == '"') {
                    next(src); next(src);
                    return 1;
                }
                break;
            case '\r': case '\n':
                consume_newline(src);
                curr = '\n';
                break;
            default:
                next(src);
                break;
        }
        if (dest != NULL) {
            append_char_to_token(dest, curr);
        }
    }

    return 0;
}

static int consume_string(Source *src, Token *dest) {
    int curr = next(src);
    int n;
    int result;

    set_token_type(dest, TOKEN_STRING);
    set_token_begin(src, dest);
    switch (curr) {
        case '\'':
            if (peek1(src) == '\'' && peek2(src) == '\'') {
                next(src); next(src);
                result = consume_str_till_singlem(src, dest);
                set_token_end(src, dest);
                return result;
            }
            result = consume_str_till_single(src, dest);
            set_token_end(src, dest);
            return result;
        case '"':
            if (peek1(src) == '"' && peek2(src) == '"') {
                next(src); next(src);
                result = consume_str_till_doublem(src, dest);
                set_token_end(src, dest);
                return result;
            }
            result = consume_str_till_double(src, dest);
            set_token_end(src, dest);
            return result;
        default:
            set_token_type(dest, TOKEN_ERROR);
            return 0;
    }

    return 0;
}

// support single line comment(#xxx) and multi-line comment(#"""xxx""" or #'''xxx''')
static int consume_comment(Source *src) {
    int curr = next(src);
    int result;
    assert(curr == '#');

    if (peek1(src) == '\'' && peek2(src) == '\'' && peek3(src) == '\'') {
        next(src); next(src); next(src);
        return consume_str_till_singlem(src, NULL);
    } else if (peek1(src) == '"' && peek2(src) == '"' && peek3(src) == '"') {
        next(src); next(src); next(src);
        return consume_str_till_doublem(src, NULL);
    } else {
        return consume_str_till_eol(src, NULL);
    }
}

static int consume_section(Source *src, Token *dest) {
    int curr = next(src);
    int result;
    assert(curr == '@');

    set_token_begin(src, dest);
    curr = peek1(src);
    switch(curr) {
        case '=': case '>': case '<':
        case '1': case '2': case '3': case '4': case '5':
        case '6': case '7': case '8': case '9':
            next(src);
            set_token_type(dest, TOKEN_SECTION);
            dest->i = curr;
            result = consume_str_till_eol(src, dest);
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
        return error_token(dest, "malformed number");
    }
    errno = 0;
    l = strtol(beginptr, &endptr, base);

    if ((errno != 0) || *endptr != '\0') {
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

    if ((errno != 0) || *endptr != '\0') {
        return error_token(dest, "invalid number format");
    }

    dest->f = l;
    return 1;
}

static int gather_hex_int(Source *src, Token *dest) {
    int curr;
    while (1) {
        curr = peek1(src);
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
        curr = peek1(src);
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
        curr = peek1(src);
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
        curr = peek1(src);
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

            curr = peek1(src);
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
        set_token_type(dest, TOKEN_FLOAT);
        return str2float(dest);
    } else {
        // integer
        set_token_type(dest, TOKEN_INTEGER);
        return str2int(dest, 10);
    }
}

static int consume_normal_number(Source *src, Token *dest) {
    int curr = next(src);
    int result;
    assert(is_dec_char(curr) && curr != '0');
    set_token_type(dest, TOKEN_INTEGER);
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

    set_token_type(dest, TOKEN_INTEGER);
    set_token_begin(src, dest);
    
    curr = peek1(src);
    switch (curr) {
        case 'x': case 'X': // only support hexdecimal integer(not hexdecimal float)
            next(src);
            result = gather_hex_int(src, dest);
            set_token_end(src, dest);
            return result;
        case 'b': case 'B': // only support binary integer(not binary float)
            next(src);
            result = gather_bin_int(src, dest);
            set_token_end(src, dest);
            return result;
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': // only support octal integer (not octal float)
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
                set_token_type(dest, TOKEN_ERROR);
                set_token_end(src, dest);
                return 0;
            } else {
                dest->i = 0;
                set_token_end(src, dest);
                return 1;
            }
    }
}


static int consume_id(Source *src, Token *dest) {
    int curr;
    int begin = 1;

    set_token_type(dest, TOKEN_IDENTIFIER);
    if (peek1(src) == '.') {
        append_char_to_token(dest, '.');
    }

    while (1) {
        // get one char
        curr = peek1(src);

        if (is_id_char(curr)) {
            next(src);
            append_char_to_token(dest, curr);
        } else if (curr == '.' && is_id_first_char(peek2(src))) {
            next(src);
            append_char_to_token(dest, 0);
            dest->i ++;
        } else {
            dest->i ++;
            set_token_end(src, dest);
            return check_reserve(dest);
        }

        // set begin position
        if (begin) {
            set_token_begin(src, dest);
            begin = 0;
        }
    }

    return 0;
}

static int consume_default(Source *src, Token *dest) {
    int curr = peek1(src);

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

// new line is encountered, check whether add semicolon according
// to the last token type 'ttype'
// THE NEXT TOKEN IS NOT CHECKED FOR WHETHER ADD SEMICOLON
static int try_add_semi(Source *src, Token *dest, int ttype) {
    switch (ttype) {
        case TOKEN_IDENTIFIER:
        case TOKEN_INTEGER:
        case TOKEN_FLOAT:
        case TOKEN_STRING:
        case TOKEN_REGEX:
        case TOKEN_KW_END:
        case TOKEN_KW_BREAK:
        case TOKEN_CONTINUE:
        case TOKEN_RETURN:
        case TOKEN_KW_TRUE:
        case TOKEN_FALSE:
        case TOKEN_NIL:
        case ')':
        case '}':
        case ']':
            set_token_begin(src, dest);
            set_token_end(src, dest);
            set_token_type(dest, ';');
            return 1;
        default:
            return 0;
    }
}

// Main Routine.
// peek the current program source, determin what type the next token probably is,
// and call the corrisponding consume_xxx function to get the next token.
// return 1: ok
//        0: error
static int consume(Source *src, Token *dest, Token *last) {
    int curr;

    // get last token type before clear_token, because dest may be the
    // last token.
    int currtype = last->type;

    clear_token(dest, 1);

    // this loop is used to bypass the comments/whitespaces
    while(1) {
        curr = peek1(src);
        switch(curr) {
            case EOS:
                if (try_add_semi(src, dest, currtype) == 0) {
                    next(src);
                    set_token_begin(src, dest);
                    set_token_end(src, dest);
                    set_token_type(dest, TOKEN_EOS);
                }
                return 1;
            case '#':
                set_token_begin(src, dest);
                if (consume_comment(src) == 0) {
                    set_token_end(src, dest);
                    set_token_type(dest, TOKEN_ERROR);
                    set_token_str(dest, "unfinished multiline comment"); //<- the only reason
                    return 0;
                }
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
                if (try_add_semi(src, dest, currtype) == 1) {
                    return 1;
                } else {
                    consume_newline(src);
                }
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
                curr = peek1(src);
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
                curr = peek1(src);
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
                curr = peek1(src);
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
                curr = peek1(src);
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
                curr = peek1(src);
                if (curr == '=') {
                    next(src);
                    set_token_end(src, dest);
                    dest->type = TOKEN_LE;
                } else {
                    set_token_end(src, dest);
                    dest->type = '<';
                }
                return 1;
            case '.': // .., .a.b.c
                curr = peek2(src);
                if (curr == '.') {
                    next(src);
                    set_token_begin(src, dest);
                    next(src);
                    set_token_end(src, dest);
                    dest->type = TOKEN_RANGE;
                    return 1;
                } else if (is_id_first_char(curr)) {
                    return consume_id(src, dest);
                //} else if (is_dec_char(curr)) {
                // do not support float number format .123
                } else {
                    next(src);
                    set_token_begin(src, dest);
                    set_token_end(src, dest);
                    set_token_type(dest, TOKEN_ERROR);
                    set_token_str(dest, "invalid '.' usage");
                    return 0;
                }
                break;
            case 'r': {
                int result;
                curr = peek2(src);
                if (curr == '"') {
                    // regular expression r"xxx"
                    next(src);
                    set_token_begin(src, dest);
                    set_token_type(dest, TOKEN_REGEX);
                    dest->i = STRTYPE_DOUBLE;
                    next(src);
                    result = consume_str_till_double(src, dest);
                    set_token_end(src, dest);
                    return result;
                } else if (curr == '\'') {
                    // regular expression r'xxx'
                    next(src);
                    set_token_begin(src, dest);
                    set_token_type(dest, TOKEN_REGEX);
                    dest->i = STRTYPE_SINGLE;
                    next(src);
                    result = consume_str_till_single(src, dest);
                    set_token_end(src, dest);
                    return result;
                } else {
                    // normal identifier
                    return consume_id(src, dest);
                }
                break;
            }
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

    if (src->next_token1.type != TOKEN_NTOKEN) {
        move_token(&src->next_token1, &src->curr_token);
        move_token(&src->next_token2, &src->next_token1);
        move_token(&src->next_token3, &src->next_token2);
        return &src->curr_token;
    }

    consume(src, &src->curr_token, &src->curr_token);
    return &src->curr_token;
}

Token *peek_token1(void *data) {
    Source *src;
    src = (Source*)data;

    // if already at the end or error occurred, no need to retry
    if (src->curr_token.type == TOKEN_EOS || src->curr_token.type == TOKEN_ERROR)
        return &src->curr_token;

    if (src->next_token1.type != TOKEN_NTOKEN) {
        return &src->next_token1;
    }

    consume(src, &src->next_token1, &src->curr_token);
    return &src->next_token1;
}

Token *peek_token2(void *data) {
    Source *src;
    src = (Source*)data;

    // if already at the end or error occurred, no need to retry
    if (src->curr_token.type == TOKEN_EOS || src->curr_token.type == TOKEN_ERROR)
        return &src->curr_token;
    if (src->next_token1.type == TOKEN_EOS || src->next_token1.type == TOKEN_ERROR)
        return &src->next_token1;

    assert(src->next_token1.type != TOKEN_NTOKEN);
    if (src->next_token2.type != TOKEN_NTOKEN) {
        return &src->next_token2;
    }

    assert(src->next_token3.type == TOKEN_NTOKEN);

    consume(src, &src->next_token2, &src->next_token1);
    return &src->next_token2;
}

Token *peek_token3(void *data) {
    Source *src;
    src = (Source*)data;

    // if already at the end or error occurred, no need to retry
    if (src->curr_token.type == TOKEN_EOS || src->curr_token.type == TOKEN_ERROR)
        return &src->curr_token;
    if (src->next_token1.type == TOKEN_EOS || src->next_token1.type == TOKEN_ERROR)
        return &src->next_token1;
    if (src->next_token2.type == TOKEN_EOS || src->next_token2.type == TOKEN_ERROR)
        return &src->next_token2;

    assert(src->next_token1.type != TOKEN_NTOKEN);
    assert(src->next_token2.type != TOKEN_NTOKEN);
    if (src->next_token3.type != TOKEN_NTOKEN) {
        return &src->next_token3;
    }

    consume(src, &src->next_token3, &src->next_token2);
    return &src->next_token3;
}

//1 ------------------------- print token ---------------------------------
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
    sprintf(buf, "%3d,%3d - %3d,%3d: %s %c ", t->beginrow, t->begincol, t->endrow, t->endcol, msg, (int)t->i);
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
static void ptss(Token *t, char *buf, int len, const char *msg) {
    int n = snprintf(buf, len, "%3d,%3d - %3d,%3d: %s: ", t->beginrow, t->begincol, t->endrow, t->endcol, msg);
    char *out = buf + n;
    if (n + token_strlen(t) < len) {
        const char *ptr = token_str(t);
        int i;
        for (i=0; i<t->i; i++) {
            int l = strlen(ptr);
            sprintf(out, "%s:", ptr);
            out += l+1;
            ptr += l+1; 
        }
    }
    *out = 0;
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
            ptss(t, buf, len, "id");
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
            ptis(t, buf, len, "regex");
            break;
        case TOKEN_KW_IF:
            pt(t, buf, "if");
            break;
        case TOKEN_KW_THEN:
            pt(t, buf, "then");
            break;
        case TOKEN_ELSE:
            pt(t, buf, "else");
            break;
        case TOKEN_ELSEIF:
            pt(t, buf, "elseif");
            break;
        case TOKEN_KW_END:
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
        case TOKEN_KW_WHILE:
            pt(t, buf, "while");
            break;
        case TOKEN_KW_BREAK:
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
        case TOKEN_KW_TRUE:
            pt(t, buf, "true");
            break;
        case TOKEN_FALSE:
            pt(t, buf, "false");
            break;
        case TOKEN_KW_TRY:
            pt(t, buf, "try");
            break;
        case TOKEN_CATCH:
            pt(t, buf, "catch");
            break;
        case TOKEN_AFTER:
            pt(t, buf, "after");
            break;
        case TOKEN_FINALLY:
            pt(t, buf, "finally");
            break;
        case TOKEN_EXPORT:
            pt(t, buf, "export");
            break;
        case TOKEN_IMPORT:
            pt(t, buf, "import");
            break;
        case TOKEN_LOCAL:
            pt(t, buf, "local");
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
