#ifndef __LPLEX_H__
#define __LPLEX_H__

#define TOKEN_NTOKEN     -1 // not a token
#define TOKEN_EOS        0
#define TOKEN_COMMENT    1 //# 
#define TOKEN_SEGMENT    2 //#=
#define TOKEN_DOWNSEG    3 //#>
#define TOKEN_UPSEG      4 //#<
#define TOKEN_IDENTIFIER 5
#define TOKEN_INTEGER    6
#define TOKEN_FLOAT      7
#define TOKEN_STRING     8
#define TOKEN_REGEX      9
#define TOKEN_IF         10
#define TOKEN_THEN       11
#define TOKEN_ELSE       12
#define TOKEN_ELSEIF     13
#define TOKEN_END        14
#define TOKEN_FOR        15
#define TOKEN_IN         16
#define TOKEN_DO         17
#define TOKEN_WHILE      18
#define TOKEN_BREAK      19
#define TOKEN_CONTINUE   20
#define TOKEN_RETURN     21

typedef struct Token {
    int type;
    int beginrow;
    int begincol;
    int endrow;
    int endcol;
    long i;
    double d;
    char buf[512];
    char *bigstr;
} Token;


void *file_source(const char *filepath);
void *string_source(const char *str);
void close_source(void *src);
Token *curr_token(void *src);
Token *peek_token(void *src);
Token *next_token(void *src);

#end // __LPLEX_H__
