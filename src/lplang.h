#ifndef LPLANG_H
#define LPLANG_H

#define STMT_STRUCT  0
#define STMT_IF      1
#define STMT_FOR     2
#define STMT_WHILE   3
#define STMT_CASE    4
#define STMT_TRY     5
#define STMT_ASSIGN  6
#define STMT_FUN     7
#define STATEMENT_COMMON int type

typedef struct {
    STATEMENT_COMMON;
} lp_statement;

typedef struct {
    STATEMENT_COMMON;
    char *name;
} lp_struct_stmt;

typedef struct {
    STATEMENT_COMMON;
} lp_if_stmt;

typedef struct {
    STATEMENT_COMMON;
} lp_for_stmt;

typedef struct {
    STATEMENT_COMMON;
} lp_while_stmt;

typedef struct {
    STATEMENT_COMMON;
} lp_try_stmt;

typedef struct {
    STATEMENT_COMMON;
    char *name;
} lp_assign_stmt;

typedef struct {
    STATEMENT_COMMON;
    char *name;

} lp_fun_stmt;

typedef struct {
    int            depth;
    char          *title;
    char          *doc;
    int            statement_count;
    lp_statement **statements;
} lp_section;
typedef struct {
    int        section_count;
    lp_section sections[1];
} lp_module;

#endif //LPLANG_H
