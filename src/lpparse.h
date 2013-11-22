#ifndef LPPARSE_H
#define LPPARSE_H
typedef struct {
    int count;
    char 
} lp_qualified_name;
// lp_type
typedef struct lpt_struct_ {

} lpt_struct;

typedef struct lpt_function_ {

} lpt_function;

typedef struct lpt_closure_ {

} lpt_closure;

// lp_value
#define LPV_BOOLEAN
#define LPV_INTEGER
#define LPV_FLOAT
#define LPV_STRING
#define LPV_REGEX
#define LPV_FUNCTION
#define LPV_LP
#define LPV_OBJECT
#define LPV_BINARY

struct lpv_function_;
struct lpv_lp_;
struct lpv_object_;
struct lpv_binary_;

typedef struct {
    int type;
    union {
        long i;
        double f;
        char *s;
        struct lpv_function_ *fn;
        struct lpv_lp_       *lp;
        struct lpv_object_   *o;
        struct lpv_binary_   *b;
    } u;
} lp_value;

typedef struct lpv_object_ {
    lpt_struct *type;
    int item_count;
    lp_value items[];
} lpv_object;

typedef struct lpv_binary_ {
    lpt_binary *type;
    int byte_count;
    unsigned char bytes[];
} lpv_binary;

typedef struct lpv_function_ {
    lpt_struct *owner;
    lpt_any    *result;
    int arg_count;
    
} lpv_function;

// ast definition
// statement definition
#define STATEMENT_TYPEDEF 0
#define STATEMENT_FUNDEF 1
#define STATEMENT_VARDEF 2
#define STATEMENT_IF     3
#define STATEMENT_WHILE 4
#define STATEMENT_CASE 5
#define STATEMENT_FOR 6
#define STATEMENT_IMPORT 7
#define STATEMENT_COMMON_MEMBERS \
    int type


typedef union {
    int b;
    long i;
    double f;
    char *s;
} ast_value;

typedef struct {
    char *name;
    ast_type *type;
    ast_value def;
} ast_vardef;

typedef struct {
} ast_struct_item;

typedef struct {
    STATEMENT_COMMON_MEMBERS;
    int is_export;
    int is_local;
    char *name;
    int item_count;
    ast_struct_item *items[1];
} ast_structdef;

typedef struct {
    STATEMENT_COMMON_MEMBERS;
} ast_statement;
typedef struct {
    char *title;
    int level;
    char *doc;
    int statement_count;
    ast_statement *statements[1];
} ast_section;
typedef struct {
    int section_count;
    ast_section sections[1];
} ast_module;
#endif //LPPARSE_H
