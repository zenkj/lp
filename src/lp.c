#include <stdio.h>
#include "lplex.h"
#define BUFLEN 1024
int main(int argc, char* argv[]) {
    void *src = file_source(argv[1]);
    Token *t;
    char buf[BUFLEN+1];

    if (src == NULL) {
        printf("invalid file name\n");
        return 1;
    }
    while (1) {
        t = next_token(src);
        print_token(t, buf, BUFLEN);
        printf("%s\n", buf);
        if (t->type == TOKEN_EOS)
            return 0;
        if (t->type == TOKEN_ERROR)
            return 2;
    }
    return 0;
}
