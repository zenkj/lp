#include "strpool.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#define INIT_POOL_SIZE 4096
#define INIT_IDX_SIZE  256

lp_strpool *strpool_init() {
    char *pool;
    int *indexes;
    lp_strpool *sp;

    pool = (char*)malloc(INIT_POOL_SIZE);
    if (pool == NULL) {
        fprintf(stderr, "no enough memory");
        return NULL;
    }
    indexes = (int*)malloc(INIT_IDX_SIZE * sizeof(int));
    if (indexes == NULL) {
        free(pool);
        fprintf(stderr, "no enough memory");
        return NULL;
    }

    sp = (lp_strpool*)malloc(sizeof(lp_strpool));
    if (sp == NULL) {
        free(pool);
        free(indexes);
        fprintf(stderr, "no enough memory");
        return NULL;
    }

    sp->charcount = 0;
    sp->charsize  = INIT_POOL_SIZE;
    sp->pool      = pool;
    sp->idxcount  = 0;
    sp->idxsize   = INIT_IDX_SIZE;
    sp->indexes   = indexes;

    return sp;
}

void strpool_destroy(lp_strpool* sp) {
    if (sp == NULL)
        return;
    free(sp->indexes);
    free(sp->pool);
    free(sp);
}

// if the result >= 0, then it's the position of 'str' in pool
// if the result < 0, then -(result+1) is the order of 'str' in
// the index list
static int search(lp_strpool *sp, const char* str) {
    int i = 0;
    int j = sp->idxcount - 1;
    char *pool = sp->pool;
    int *indexes = sp->indexes;
    while (i <= j) {
        int m = (i + j) / 2;
        char *b = &pool[indexes[m]];
        int r = strcmp(b, str);
        if (r == 0) {
            return indexes[m];
        } else if (r < 0) {
            i = m + 1;
        } else {
            j = m - 1;
        }
    }
    // (i+j+1)/2 is the position for the string,
    // -1 is used to differenciate with the found position 0
    return -((i+j+1)/2)  - 1;
}

int strpool_add(lp_strpool *sp, const char* str) {
    int len;
    int pos;
    int idxpos;
    int newcharcount;
    int newidxcount;
    assert(sp != NULL);
    assert(str != NULL);

    pos = search(sp, str);

    if (pos >= 0) {
        return pos;
    }

    idxpos = -(pos + 1);

    len = strlen(str);
    pos = sp->charcount;

    newcharcount = pos + len + 1;
    if (newcharcount >= sp->charsize) {
        int newsize = newcharcount * 2;
        char *pool = (char*)realloc(sp->pool, newsize);
        if (pool == NULL) {
            fprintf(stderr, "no enough memory");
            return NOT_SP_ID;
        }
        sp->pool = pool;
        sp->charsize = newsize;
    }

    newidxcount = sp->idxcount + 1;
    if (newidxcount >= sp->idxsize) {
        int newsize = newidxcount * 2;
        int *indexes = (int*)realloc(sp->indexes, newsize*sizeof(int));
        if (indexes == NULL) {
            fprintf(stderr, "no enough memory");
            return NOT_SP_ID;
        }
        sp->indexes = indexes;
        sp->idxsize = newsize;
    }

    strcpy(sp->pool + sp->charcount, str);
    sp->charcount = newcharcount;

    if (idxpos < sp->idxcount) {
        memmove((void*)(sp->indexes+idxpos+1), (void*)(sp->indexes+idxpos), sizeof(int));
    }
    sp->indexes[idxpos] = pos;
    sp->idxcount = newidxcount;
    return pos;
}

void strpool_print(lp_strpool *sp) {
    int i;
    for (i=0; i<sp->idxcount; i++) {
        printf("%s\n", sp->pool + sp->indexes[i]);
    }
}

int main(int argc, char*argv[]) {
    lp_strpool *sp = strpool_init();

    strpool_add(sp, "cadsfeasfd");
    strpool_add(sp, "afsadf");
    strpool_add(sp, "f1111");
    strpool_add(sp, "dr222");
    strpool_add(sp, "iddd");
    strpool_add(sp, "eaa");


    printf("%d\n", search(sp, argv[1]));
    strpool_print(sp);
    strpool_destroy(sp);
    return 0;
}

