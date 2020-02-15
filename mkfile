program=gitstats
<|printf sources=; ls -C *.c
objects=${sources:%.c=%.o}

CC=c99
PREFIX=$HOME/.local
INCS=
LIBS=-lgit2
CFLAGS=\
	-g \
	-std=c99 \
	-pedantic \
	-Wall -Werror \
	-D_DEFAULT_SOURCE \
	-D_BSD_SOURCE \
	-D_POSIX_C_SOURCE=2
LDFLAGS=

$program: $objects
	$CC $LDFLAGS $LIBS -o $target $objects

%.o:	%.c
	$CC -c $CFLAGS $INCS $stem.c

install:V: $program
	mkdir -p $PREFIX/bin
	cp -f $program $PREFIX/bin
	chmod 755 $PREFIX/bin/$program
