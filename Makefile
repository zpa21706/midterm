PROG=	ls
OBJS=	ls.o cmp.o print.o
CC?=	cc
CFLAGS+=	-Wall -Wextra -Werror -pedantic -g -D_DEFAULT_SOURCE

all: ${PROG}

${PROG}: ${OBJS}
	${CC} ${CFLAGS} -o ${PROG} ${OBJS} ${LDFLAGS}

ls.o: ls.c ls.h
cmp.o: cmp.c ls.h
print.o: print.c ls.h

.c.o:
	${CC} ${CFLAGS} -c $<

clean:
	rm -f ${PROG} ${OBJS} core *.core

.PHONY: all clean
