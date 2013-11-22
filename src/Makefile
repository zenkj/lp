SRC = $(wildcard *.c)
OBJ = $(SRC:%.c=%.o)
lp: $(OBJ)
	gcc -o $@ $^

$(OBJ):%.o:%.c
	gcc -c -g -o $@ $<

sp: strpool.c strpool.h
	gcc -g -o $@ $<

clean:
	rm -f lp *.o
