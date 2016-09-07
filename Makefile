C_SRC	= alloc.c error.c input.c lex.c output.c str.c \
		  symbol.c type.c cic.c vector.c
OBJS	= $(C_SRC:.c=.o)
CC		= clang
CFLAGS	= -g -std=c11 -D_CIC

all: $(OBJS)
	$(CC) -o cic $(CFLAGS) $(OBJS)

clean:
	rm -f *.o cic
