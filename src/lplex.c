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

#include "lplex.h"

// ---------------------- common definition ---------------------
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
    char *bitstr = t->bigstr;
    t->type = TOKEN_NTOKEN;
    t->beginrow = 0;
    t->begincol = 0;
    t->endrow = 0;
    t->endcol = 0;
    t->i = 0;
    t->d = 0.0;
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
    }

    ptr[t->strlen++] = (char)ch;
    ptr[t-strlen] = (char)0;

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
    if (fp == -1) {
        free(mypath);
        printf("can't open source file %s to read\n", filepath);
        return NULL;
    }
    
    src = (Source *)malloc(sizeof(Source));
    if (src == NULL) {
        free(mypath);
        fclose(fp);
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
    if (ss == null) {
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


//--------------- consume various tokens  -----------
// all these verious consume_xxx functions will consume
// corresponding items, set col/row, move to the end of
// the item(not the next char of the item).
// at the entry of these consume_xxx function, the current
// character is the end of the last item.

#define STRTYPE_TOEOL   0 // string till end of line
#define STRTYPE_SINGLE  1 // single-line string quoted by '
#define STRTYPE_DOUBLE  2 // single-line string quoted by "
#define STRTYPE_SINGLEM 3 // multi-line string quoted by '''
#define STRTYPE_DOUBLEM 4 // multi-line string quoted by """

#define is_newline_char (ch) ((ch) == '\n' || (ch) == '\r')

static int consume_newline(Source *src) {
    int curr = next(src);
    int next = peek(src);
    assert(is_newline_char(curr));

    if (is_newline_char(next)) {
        if (curr != next) {
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
            while (true) {
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
            while (true) {
                curr = peek(src);
                switch (curr) {
                    case EOS: case '\n': case '\r':
                        set_token_str(dest, "no close \"'\"");
                        dest->type = TOKEN_ERROR;
                        return 0;
                    case '\'':
                        next(src);
                        return 1;
                    case '\\':
                        next(src);
                        curr = peek(src);
                        if (curr == '\'') {
                            next(src);
                        } else if (is_newline_char(curr)) {
                            consume_newline(curr);
                            next = '\n';
                        } else {
                            curr = '\\';
                        }
                    default:
                        next(src);
                        break;
                }
                append_char_to_token(dest, curr);
            }
            break;
        case STRTYPE_DOUBLE:
            while (true) {
                curr = peek(src);
                switch (curr) {
                    case EOS: case '\n': case '\r':
                        set_token_str(dest, "no close '\"'");
                        dest->type = TOKEN_ERROR;
                        return 0;
                    case '"':
                        next(src);
                        return 1;
                    case '\\':
                        next(src);
                        curr = peek(src);
                        switch (curr) {
                            case EOS:
                                curr = '\\';
                                break;
                            case '"':
                                next(src);
                                break;
                            case 'n':
                                curr = '\n';
                                next(src);
                                break;
                            case 'r':
                                curr = '\r';
                                next(src);
                                break;
                            case '\\':
                                next(src);
                                break;
                            case 't':
                                curr = '\t';
                                next(src);
                                break;
                            case '\n': case '\r':
                                consume_newline(curr);
                                curr = '\n';
                            default:
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
            while (true) {
                curr = peek(src);
                switch (curr) {
                    case EOS:
                        set_token_str(dest, "no close \"'''\"");
                        dest->type = TOKEN_ERROR;
                        return 0;
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
                    default:
                        next(src);
                        break;
                }
                append_char_to_token(dest, curr);
            }
            break;
        case STRTYPE_DOUBLEM:
            while (true) {
                curr = peek(src);
                switch (curr) {
                    case EOS:
                        set_token_str(dest, "no close '\"\"\"'");
                        dest->type = TOKEN_ERROR;
                        return 0;
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
    int next;
    int result;

    dest->type = TOKEN_STRING;
    set_token_begin(src, dest);
    switch (curr) {
        case '\'':
            next = peek(src);
            switch (next) {
                case EOS: case '\n': case '\r':
                    set_token_str(dest, "no close \"'\"");
                    dest->type = TOKEN_ERROR;
                    set_token_end(src, dest);
                    return 0;
                case '\'':
                    curr = next(src);
                    next = peek(src);
                    if (next == '\'') {
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
            next = peek(src);
            switch (next) {
                case EOS: case '\n': case '\r':
                    set_token_str(dest, "no close '\"'");
                    dest->type = TOKEN_ERROR;
                    set_token_end(src, dest);
                    return 0;
                case '"':
                    curr = next(src);
                    next = peek(src);
                    if (next == '"') {
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

    while (true) {
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
            dest->type = TOKEN_ERROR;
            set_token_str(dest, "invalid section specification");
            set_token_end(src, dest);
            return 0;
    }

    return 0;
}

//--------------- token -----------------------------

// return 1: ok
//        0: error
static int next_token_to(Source *src, Token *dest) {
    int curr;

    clear_token(dest);

    while(true) {
        curr = peek(src);
        switch(curr) {
            case EOS:
                next(src);
                dest->type = TOKEN_EOS;
                return 1;
            case '#':
                return consume_comment(src, dest);
            case '@':
                return consume_section(src, dest);
            case '"': case '\'':
                return consume_string(sec, dest);
            case '0':
                // TODO
                break;
            case '1': case '2': case '3': case '4': case '5':
            case '6': case '7': case '8': case '9': 
                break;
            '\r': '\n':
                consume_newline(src);
                break;
            case ':':
                break;
            case '.':
                break;
            case '+':
                break;
            case '-':
                break;
            case '*':
                break;
            case '/':
                break;
            case '%':
                break;
            case '(':
                break;
            case ')':
                break;
            case '{':
                break;
            case '}':
                break;
            case '[':
                break;
            case ']':
                break;
            case '=':
                break;
            case '!':
                break;
            case '|':
                break;
            case ',':
                break;
            case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
            case 'g': case 'h': case 'i': case 'j': case 'k': case 'l':
            case 'm': case 'n': case 'o': case 'p': case 'q': case 'r':
            case 's': case 't': case 'u': case 'v': case 'w': case 'x':
            case 'y': case 'z': 
            case '_':
                break;
            case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
            case 'G': case 'H': case 'I': case 'J': case 'K': case 'L':
            case 'M': case 'N': case 'O': case 'P': case 'Q': case 'R':
            case 'S': case 'T': case 'U': case 'V': case 'W': case 'X':
            case 'Y': case 'Z': 
                break;
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


