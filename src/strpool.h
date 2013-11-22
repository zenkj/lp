#ifndef STRPOOL_H
#define STRPOOL_H

/*
 * simple string pool.
 * 1. can add string, query string by id
 * 2. cannot remove string, so no need to manage the storage
 *
 */
#define NOT_SP_ID -1

typedef struct {
    int charcount; // current used count in the pool
    int charsize;  // size of the pool
    char *pool;    // the pool to store the strings
    int idxcount;  // string count
    int idxsize;   // index size
    int *indexes;  // string position list in the pool, in order
} lp_strpool;

// create a string pool
lp_strpool *strpool_init();

// destroy a string pool
void strpool_destroy(lp_strpool* sp);

// add a string into the string pool, return the id of the string
// if the string is already in the pool, just return the id of it
int strpool_add(lp_strpool* sp, const char* str);
#define strpool_getid(sp, str) strpool_add((sp), (str))

// do not implement remove-string in this simple version
// remove a string from the pool, if it exists in the pool
//void strpool_remove(lp_strpool* sp, const char* str);
// remove a string by its id, if it exists in the pool
//void strpool_removeid(lp_strpool* sp, int id);

// get a string from the pool, according to its id
const char* strpool_get(lp_strpool* sp, int id);

// for test
void strpool_print(lp_strpool *sp);

#endif //STRPOOL_H
