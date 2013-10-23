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

static int consume_newline(Source *src) {
    int curr = curr(src);
    int next = peek(src);
    assert(curr == '\n' || curr == '\r');

    if (next == '\r' || next == '\n') {
        if (curr != next) {
            next(src);
        }
    }

    src->row ++;
    src->col = 0;

    return 0;
}



//--------------- token -----------------------------
Token *next_token(void *data) {
    Source *src;
    int curr;
    src = (Source*)data;

loop:
    curr = next(src);
    switch(curr) {
        case EOS:
            break;
        case '#':
            break;
        case '"':
            break;
        case '\'':
            break;
        case '0':
            break;
        case '1': case '2': case '3': case '4': case '5':
        case '6': case '7': case '8': case '9': 
            break;
        '\r': '\n':
            consume_newline(src);
            goto loop;
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


