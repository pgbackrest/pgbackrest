CC=gcc
CFLAGS=-I. -Wfatal-errors -Wall -Wextra -Wwrite-strings -Wno-clobbered -std=c99 -funroll-loops -ftree-vectorize
DESTDIR=

pgbackrest: main.o \
			common/error.o common/errorType.o common/memContext.o common/type/list.o common/type/string.o common/type/stringList.o \
			perl/exec.o
	$(CC) $(CFLAGS) -o pgbackrest main.o \
		common/error.o common/errorType.o common/memContext.o common/type/list.o common/type/string.o common/type/stringList.o \
		perl/exec.o

install: pgbackrest
	sudo install -d $(DESTDIR)/usr/bin
	sudo install -m 755 pgbackrest $(DESTDIR)/usr/bin
