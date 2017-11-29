CC=gcc
CFLAGS=-I. -Wfatal-errors -Wall -Wextra -Wwrite-strings -Wno-clobbered -std=c99 -funroll-loops -ftree-vectorize
DESTDIR=

pgbackrest: \
		common/error.o \
		common/errorType.o \
		common/memContext.o \
		common/type/list.o \
		common/type/string.o \
		common/type/stringList.o \
		config/config.o \
		config/define.o \
		config/parse.o \
		perl/exec.o \
		main.o
	$(CC) $(CFLAGS) -o pgbackrest \
		common/error.o \
		common/errorType.o \
		common/memContext.o \
		common/type/list.o \
		common/type/string.o \
		common/type/stringList.o \
		config/config.o \
		config/define.o \
		config/parse.o \
		perl/exec.o \
		main.o

install: pgbackrest
	sudo install -d $(DESTDIR)/usr/bin
	sudo install -m 755 pgbackrest $(DESTDIR)/usr/bin
