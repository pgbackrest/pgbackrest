CC=gcc
CFLAGS=-I. -Wfatal-errors -Wall -Wextra -Wwrite-strings -Wno-clobbered -std=c99 -O2 -funroll-loops -ftree-vectorize
DESTDIR=

pgbackrest: \
		command/archive/push/push.o \
		command/help/help.o \
		command/command.o \
		common/error.o \
		common/exit.o \
		common/ini.o \
		common/log.o \
		common/memContext.o \
		common/regExp.o \
		common/time.o \
		common/type/buffer.o \
		common/type/keyValue.o \
		common/type/list.o \
		common/type/string.o \
		common/type/stringList.o \
		common/type/variant.o \
		common/type/variantList.o \
		common/wait.o \
		config/config.o \
		config/define.o \
		config/load.o \
		config/parse.o \
		perl/config.o \
		perl/exec.o \
		storage/helper.o \
		storage/storage.o \
		main.o
	$(CC) $(CFLAGS) -o pgbackrest \
		command/archive/push/push.o \
		command/help/help.o \
		command/command.o \
		common/error.o \
		common/exit.o \
		common/ini.o \
		common/log.o \
		common/memContext.o \
		common/regExp.o \
		common/time.o \
		common/type/buffer.o \
		common/type/keyValue.o \
		common/type/list.o \
		common/type/string.o \
		common/type/stringList.o \
		common/type/variant.o \
		common/type/variantList.o \
		common/wait.o \
		config/config.o \
		config/define.o \
		config/load.o \
		config/parse.o \
		perl/config.o \
		perl/exec.o \
		storage/helper.o \
		storage/storage.o \
		main.o

install: pgbackrest
	sudo install -d $(DESTDIR)/usr/bin
	sudo install -m 755 pgbackrest $(DESTDIR)/usr/bin
