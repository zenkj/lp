#ifndef __LPLEX_H__
#define __LPLEX_H__

#define TOKEN_ERROR      -2 // error
#define TOKEN_NTOKEN     -1 // not a token
#define TOKEN_EOS        500
#define TOKEN_SECTION    502 //#1~#9, #<, #>, #=
#define TOKEN_IDENTIFIER 504
#define TOKEN_BOOL       505
#define TOKEN_INTEGER    506
#define TOKEN_FLOAT      507
#define TOKEN_STRING     508 //string or 'xxx' or "xxx" or '''xxx''' or """xxx"""
#define TOKEN_REGEX      509 //r'xxx' or r"xxx"
#define TOKEN_KW_IF         510 //if
#define TOKEN_KW_THEN       511 //then
#define TOKEN_KW_ELSE       512 //else
#define TOKEN_KW_ELSEIF     513 //elseif
#define TOKEN_KW_END        514 //end
#define TOKEN_KW_FOR        515 //for
#define TOKEN_KW_IN         516 //if
#define TOKEN_KW_DO         517 //do
#define TOKEN_KW_WHILE      518 //while
#define TOKEN_KW_BREAK      519 //break
#define TOKEN_KW_CONTINUE   520 //continue
#define TOKEN_KW_RETURN     521 //return
#define TOKEN_KW_AND        522 //and
#define TOKEN_KW_OR         523 //or
#define TOKEN_KW_CASE       524 //case
#define TOKEN_KW_OF         525 //of
#define TOKEN_KW_LP         526 //lp
#define TOKEN_KW_FUN        527 //fun
#define TOKEN_KW_TRUE       528 //true
#define TOKEN_KW_FALSE      529 //false
#define TOKEN_KW_NIL        530 //nil
#define TOKEN_KW_TRY        531 //try
#define TOKEN_KW_CATCH      532 //catch
#define TOKEN_KW_AFTER      533 //after
#define TOKEN_KW_FINALLY    534 //finally
#define TOKEN_KW_IMPORT     535 //import
#define TOKEN_KW_EXPORT     536 //export
#define TOKEN_KW_LOCAL      537 //local local in current section(including inner sections)
#define TOKEN_KW_INT        538 //int
#define TOKEN_KW_FLOAT      539 //float
#define TOKEN_KW_STRING     540 //string
#define TOKEN_KW_REGEX      541 //regex
#define TOKEN_KW_BOOL       542 //bool

#define TOKEN_EXPONENT   628 //**
#define TOKEN_EQ         629 //==
#define TOKEN_NEQ        630 //!=
#define TOKEN_GE         631 //>=
#define TOKEN_LE         632 //<=
#define TOKEN_RANGE      633 //..

#define STRTYPE_SINGLE  1 // string quoted by '
#define STRTYPE_DOUBLE  2 // string quoted by "

#define TOKEN_BUFMAX     512
typedef struct Token {
    int type;
    int beginrow;
    int begincol;
    int endrow;
    int endcol;
    long i;
    double f;
    int  strlen; // actual string length
    int  buflen; // actual buffer length
    char buf[TOKEN_BUFMAX+1];
    char *bigstr;
} Token;


void *file_source(const char *filepath);
void *string_source(const char *str);
void close_source(void *src);
Token *curr_token(void *src);
Token *peek_token1(void *src);
Token *peek_token2(void *src);
Token *peek_token3(void *src);
Token *next_token(void *src);
void print_token(Token *t, char *buf, int len);

#endif // __LPLEX_H__
