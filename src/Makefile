####################################################################################################################################
# pgBackRest Makefile
####################################################################################################################################

####################################################################################################################################
# Compile options
####################################################################################################################################
CC=gcc

# Compile using C99 standard
CSTD = -std=c99

# Compile optimizations
COPT = -O2

# Locations of header files
CINCLUDE = -I. -I../libc

# Compile warnings
CWARN = -Wfatal-errors -Wall -Wextra -Wwrite-strings -Wswitch-enum -Wconversion -Wformat=2 -Wformat-nonliteral \
		-Wno-clobbered -Wno-missing-field-initializers -Wstrict-prototypes -Wpointer-arith

# Automatically generate Perl compile options for the local system
CPERL = `perl -MExtUtils::Embed -e ccopts`

# Debug options
CDEBUG = -DNDEBUG

# Extra compile options to be set by caller
CEXTRA =

# Concatenate options for easy usage
CFLAGS = $(CINCLUDE) $(CSTD) $(COPT) $(CWARN) $(CPERL) $(CDEBUG) $(CEXTRA)

####################################################################################################################################
# Link options
####################################################################################################################################
# Automatically generate Perl linker options for the local system
LDPERL = `perl -MExtUtils::Embed -e ldopts`

# Extra linker options to be set by caller
LDEXTRA =

# Concatenate options for easy usage
LDFLAGS = -lcrypto $(LDPERL) $(LDEXTRA)

####################################################################################################################################
# Install options
####################################################################################################################################
# Modify destination install directory
DESTDIR =

####################################################################################################################################
# List of required source files.  main.c should always be listed last and the rest in alpha order.
####################################################################################################################################
SRCS = \
	command/archive/common.c \
	command/archive/get/get.c \
	command/archive/push/push.c \
	command/help/help.c \
	command/command.c \
	common/debug.c \
	common/encode.c \
	common/encode/base64.c \
	common/error.c \
	common/exit.c \
	common/fork.c \
	common/io/bufferRead.c \
	common/io/bufferWrite.c \
	common/io/filter/filter.c \
	common/io/filter/group.c \
	common/io/handle.c \
	common/io/io.c \
	common/io/read.c \
	common/io/write.c \
	common/ini.c \
	common/lock.c \
	common/log.c \
	common/memContext.c \
	common/regExp.c \
	common/stackTrace.c \
	common/time.c \
	common/type/buffer.c \
	common/type/convert.c \
	common/type/keyValue.c \
	common/type/list.c \
	common/type/string.c \
	common/type/stringList.c \
	common/type/variant.c \
	common/type/variantList.c \
	common/wait.c \
	config/config.c \
	config/define.c \
	config/load.c \
	config/parse.c \
	crypto/cipherBlock.c \
	crypto/hash.c \
	crypto/crypto.c \
	crypto/random.c \
	perl/config.c \
	perl/exec.c \
	postgres/info.c \
	postgres/pageChecksum.c \
	storage/driver/posix/driver.c \
	storage/driver/posix/driverFile.c \
	storage/driver/posix/driverRead.c \
	storage/driver/posix/driverWrite.c \
	storage/fileRead.c \
	storage/fileWrite.c \
	storage/helper.c \
	storage/storage.c \
	main.c

# Create obj list from source list
OBJS=$(SRCS:.c=.o)

####################################################################################################################################
# Compile and link
####################################################################################################################################
pgbackrest: $(OBJS)
	$(CC) -o pgbackrest $(OBJS) $(LDFLAGS)

####################################################################################################################################
# Installation.  DESTDIR can be used to modify the install location.
####################################################################################################################################
install: pgbackrest
	install -d $(DESTDIR)/usr/bin
	install -m 755 pgbackrest $(DESTDIR)/usr/bin

####################################################################################################################################
# Compile rules
####################################################################################################################################
command/archive/common.o: command/archive/common.c command/archive/common.h common/assert.h common/debug.h common/error.auto.h common/error.h common/io/filter/filter.h common/io/filter/group.h common/io/read.h common/io/write.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h common/wait.h postgres/version.h storage/driver/posix/driverRead.h storage/driver/posix/driverWrite.h storage/fileRead.h storage/fileWrite.h storage/helper.h storage/info.h storage/storage.h version.h
	$(CC) $(CFLAGS) -c command/archive/common.c -o command/archive/common.o

command/archive/get/get.o: command/archive/get/get.c command/archive/common.h command/command.h common/assert.h common/debug.h common/error.auto.h common/error.h common/fork.h common/io/filter/filter.h common/io/filter/group.h common/io/read.h common/io/write.h common/lock.h common/log.h common/logLevel.h common/memContext.h common/regExp.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h common/wait.h config/config.auto.h config/config.h config/define.auto.h config/define.h config/load.h perl/exec.h postgres/info.h storage/driver/posix/driverRead.h storage/driver/posix/driverWrite.h storage/fileRead.h storage/fileWrite.h storage/helper.h storage/info.h storage/storage.h version.h
	$(CC) $(CFLAGS) -c command/archive/get/get.c -o command/archive/get/get.o

command/archive/push/push.o: command/archive/push/push.c command/archive/common.h command/command.h common/debug.h common/error.auto.h common/error.h common/fork.h common/io/filter/filter.h common/io/filter/group.h common/io/read.h common/io/write.h common/lock.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h common/wait.h config/config.auto.h config/config.h config/define.auto.h config/define.h config/load.h perl/exec.h storage/driver/posix/driverRead.h storage/driver/posix/driverWrite.h storage/fileRead.h storage/fileWrite.h storage/helper.h storage/info.h storage/storage.h version.h
	$(CC) $(CFLAGS) -c command/archive/push/push.c -o command/archive/push/push.o

command/command.o: command/command.c common/assert.h common/debug.h common/error.auto.h common/error.h common/lock.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/time.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h config/config.auto.h config/config.h config/define.auto.h config/define.h version.h
	$(CC) $(CFLAGS) -c command/command.c -o command/command.o

command/help/help.o: command/help/help.c common/assert.h common/debug.h common/error.auto.h common/error.h common/io/handle.h common/lock.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h config/config.auto.h config/config.h config/define.auto.h config/define.h version.h
	$(CC) $(CFLAGS) -c command/help/help.c -o command/help/help.o

common/debug.o: common/debug.c common/debug.h common/logLevel.h common/stackTrace.h common/type/convert.h
	$(CC) $(CFLAGS) -c common/debug.c -o common/debug.o

common/encode.o: common/encode.c common/debug.h common/encode.h common/encode/base64.h common/error.auto.h common/error.h common/logLevel.h common/stackTrace.h common/type/convert.h
	$(CC) $(CFLAGS) -c common/encode.c -o common/encode.o

common/encode/base64.o: common/encode/base64.c common/debug.h common/encode/base64.h common/error.auto.h common/error.h common/logLevel.h common/stackTrace.h common/type/convert.h
	$(CC) $(CFLAGS) -c common/encode/base64.c -o common/encode/base64.o

common/error.o: common/error.c common/error.auto.c common/error.auto.h common/error.h common/logLevel.h common/stackTrace.h
	$(CC) $(CFLAGS) -c common/error.c -o common/error.o

common/exit.o: common/exit.c command/command.h common/debug.h common/error.auto.h common/error.h common/exit.h common/lock.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h config/config.auto.h config/config.h config/define.auto.h config/define.h perl/exec.h
	$(CC) $(CFLAGS) -c common/exit.c -o common/exit.o

common/fork.o: common/fork.c common/debug.h common/error.auto.h common/error.h common/log.h common/logLevel.h common/stackTrace.h common/type/convert.h
	$(CC) $(CFLAGS) -c common/fork.c -o common/fork.o

common/ini.o: common/ini.c common/debug.h common/error.auto.h common/error.h common/ini.h common/io/filter/filter.h common/io/filter/group.h common/io/read.h common/io/write.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h storage/driver/posix/driverRead.h storage/driver/posix/driverWrite.h storage/fileRead.h storage/fileWrite.h storage/helper.h storage/info.h storage/storage.h version.h
	$(CC) $(CFLAGS) -c common/ini.c -o common/ini.o

common/io/bufferRead.o: common/io/bufferRead.c common/assert.h common/debug.h common/error.auto.h common/error.h common/io/bufferRead.h common/io/filter/filter.h common/io/filter/group.h common/io/read.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/variant.h common/type/variantList.h
	$(CC) $(CFLAGS) -c common/io/bufferRead.c -o common/io/bufferRead.o

common/io/bufferWrite.o: common/io/bufferWrite.c common/assert.h common/debug.h common/error.auto.h common/error.h common/io/bufferWrite.h common/io/filter/filter.h common/io/filter/group.h common/io/write.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/variant.h common/type/variantList.h
	$(CC) $(CFLAGS) -c common/io/bufferWrite.c -o common/io/bufferWrite.o

common/io/filter/filter.o: common/io/filter/filter.c common/assert.h common/debug.h common/error.auto.h common/error.h common/io/filter/filter.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/variant.h common/type/variantList.h
	$(CC) $(CFLAGS) -c common/io/filter/filter.c -o common/io/filter/filter.o

common/io/filter/group.o: common/io/filter/group.c common/assert.h common/debug.h common/error.auto.h common/error.h common/io/filter/filter.h common/io/filter/group.h common/io/io.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/list.h common/type/string.h common/type/variant.h common/type/variantList.h
	$(CC) $(CFLAGS) -c common/io/filter/group.c -o common/io/filter/group.o

common/io/handle.o: common/io/handle.c common/debug.h common/error.auto.h common/error.h common/io/handle.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/string.h
	$(CC) $(CFLAGS) -c common/io/handle.c -o common/io/handle.o

common/io/io.o: common/io/io.c common/assert.h common/debug.h common/error.auto.h common/error.h common/io/io.h common/log.h common/logLevel.h common/stackTrace.h common/type/convert.h
	$(CC) $(CFLAGS) -c common/io/io.c -o common/io/io.o

common/io/read.o: common/io/read.c common/debug.h common/error.auto.h common/error.h common/io/filter/filter.h common/io/filter/group.h common/io/io.h common/io/read.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/variant.h common/type/variantList.h
	$(CC) $(CFLAGS) -c common/io/read.c -o common/io/read.o

common/io/write.o: common/io/write.c common/debug.h common/error.auto.h common/error.h common/io/filter/filter.h common/io/filter/group.h common/io/write.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/variant.h common/type/variantList.h
	$(CC) $(CFLAGS) -c common/io/write.c -o common/io/write.o

common/lock.o: common/lock.c common/assert.h common/debug.h common/error.auto.h common/error.h common/io/filter/filter.h common/io/filter/group.h common/io/handle.h common/io/read.h common/io/write.h common/lock.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h common/wait.h storage/driver/posix/driverRead.h storage/driver/posix/driverWrite.h storage/fileRead.h storage/fileWrite.h storage/helper.h storage/info.h storage/storage.h version.h
	$(CC) $(CFLAGS) -c common/lock.c -o common/lock.o

common/log.o: common/log.c common/assert.h common/debug.h common/error.auto.h common/error.h common/log.h common/logLevel.h common/stackTrace.h common/time.h common/type/convert.h
	$(CC) $(CFLAGS) -c common/log.c -o common/log.o

common/memContext.o: common/memContext.c common/assert.h common/debug.h common/error.auto.h common/error.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/convert.h
	$(CC) $(CFLAGS) -c common/memContext.c -o common/memContext.o

common/regExp.o: common/regExp.c common/debug.h common/error.auto.h common/error.h common/logLevel.h common/memContext.h common/regExp.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/string.h
	$(CC) $(CFLAGS) -c common/regExp.c -o common/regExp.o

common/stackTrace.o: common/stackTrace.c common/assert.h common/error.auto.h common/error.h common/logLevel.h common/stackTrace.h
	$(CC) $(CFLAGS) -c common/stackTrace.c -o common/stackTrace.o

common/time.o: common/time.c common/debug.h common/logLevel.h common/stackTrace.h common/time.h common/type/convert.h
	$(CC) $(CFLAGS) -c common/time.c -o common/time.o

common/type/buffer.o: common/type/buffer.c common/assert.h common/debug.h common/error.auto.h common/error.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/string.h
	$(CC) $(CFLAGS) -c common/type/buffer.c -o common/type/buffer.o

common/type/convert.o: common/type/convert.c common/assert.h common/debug.h common/error.auto.h common/error.h common/logLevel.h common/stackTrace.h common/type/convert.h
	$(CC) $(CFLAGS) -c common/type/convert.c -o common/type/convert.o

common/type/keyValue.o: common/type/keyValue.c common/debug.h common/error.auto.h common/error.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/list.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h
	$(CC) $(CFLAGS) -c common/type/keyValue.c -o common/type/keyValue.o

common/type/list.o: common/type/list.c common/debug.h common/error.auto.h common/error.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/list.h common/type/string.h
	$(CC) $(CFLAGS) -c common/type/list.c -o common/type/list.o

common/type/string.o: common/type/string.c common/assert.h common/debug.h common/error.auto.h common/error.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/string.h
	$(CC) $(CFLAGS) -c common/type/string.c -o common/type/string.o

common/type/stringList.o: common/type/stringList.c common/assert.h common/debug.h common/error.auto.h common/error.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/list.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h
	$(CC) $(CFLAGS) -c common/type/stringList.c -o common/type/stringList.o

common/type/variant.o: common/type/variant.c common/assert.h common/debug.h common/error.auto.h common/error.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/variant.h common/type/variantList.h
	$(CC) $(CFLAGS) -c common/type/variant.c -o common/type/variant.o

common/type/variantList.o: common/type/variantList.c common/debug.h common/error.auto.h common/error.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/list.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h
	$(CC) $(CFLAGS) -c common/type/variantList.c -o common/type/variantList.o

common/wait.o: common/wait.c common/debug.h common/error.auto.h common/error.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/time.h common/type/convert.h common/wait.h
	$(CC) $(CFLAGS) -c common/wait.c -o common/wait.o

config/config.o: config/config.c common/assert.h common/debug.h common/error.auto.h common/error.h common/lock.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h config/config.auto.c config/config.auto.h config/config.h config/define.auto.h config/define.h
	$(CC) $(CFLAGS) -c config/config.c -o config/config.o

config/define.o: config/define.c common/assert.h common/debug.h common/error.auto.h common/error.h common/logLevel.h common/stackTrace.h common/type/convert.h config/define.auto.c config/define.auto.h config/define.h
	$(CC) $(CFLAGS) -c config/define.c -o config/define.o

config/load.o: config/load.c command/command.h common/debug.h common/error.auto.h common/error.h common/io/io.h common/lock.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h config/config.auto.h config/config.h config/define.auto.h config/define.h config/load.h config/parse.h
	$(CC) $(CFLAGS) -c config/load.c -o config/load.o

config/parse.o: config/parse.c common/assert.h common/debug.h common/error.auto.h common/error.h common/ini.h common/io/filter/filter.h common/io/filter/group.h common/io/read.h common/io/write.h common/lock.h common/log.h common/logLevel.h common/memContext.h common/regExp.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h config/config.auto.h config/config.h config/define.auto.h config/define.h config/parse.auto.c config/parse.h storage/driver/posix/driverRead.h storage/driver/posix/driverWrite.h storage/fileRead.h storage/fileWrite.h storage/helper.h storage/info.h storage/storage.h version.h
	$(CC) $(CFLAGS) -c config/parse.c -o config/parse.o

crypto/cipherBlock.o: crypto/cipherBlock.c common/debug.h common/error.auto.h common/error.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/convert.h crypto/cipher.h crypto/cipherBlock.h crypto/crypto.h crypto/random.h
	$(CC) $(CFLAGS) -c crypto/cipherBlock.c -o crypto/cipherBlock.o

crypto/crypto.o: crypto/crypto.c common/debug.h common/log.h common/logLevel.h common/stackTrace.h common/type/convert.h crypto/crypto.h
	$(CC) $(CFLAGS) -c crypto/crypto.c -o crypto/crypto.o

crypto/hash.o: crypto/hash.c common/debug.h common/error.auto.h common/error.h common/io/filter/filter.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/variant.h common/type/variantList.h crypto/crypto.h crypto/hash.h
	$(CC) $(CFLAGS) -c crypto/hash.c -o crypto/hash.o

crypto/random.o: crypto/random.c common/debug.h common/error.auto.h common/error.h common/log.h common/logLevel.h common/stackTrace.h common/type/convert.h crypto/random.h
	$(CC) $(CFLAGS) -c crypto/random.c -o crypto/random.o

info/info.o: info/info.c common/debug.h common/error.auto.h common/error.h common/ini.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h crypto/hash.h info/info.h storage/driver/posix/driverRead.h storage/driver/posix/driverWrite.h storage/fileRead.h storage/fileWrite.h storage/helper.h storage/info.h storage/storage.h version.h
	$(CC) $(CFLAGS) -c info/info.c -o info/info.o

info/infoArchive.o: info/infoArchive.c common/debug.h common/error.auto.h common/error.h common/ini.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h info/infoArchive.h info/infoPg.h storage/driver/posix/driverRead.h storage/driver/posix/driverWrite.h storage/fileRead.h storage/fileWrite.h storage/helper.h storage/info.h storage/storage.h version.h
	$(CC) $(CFLAGS) -c info/infoArchive.c -o info/infoArchive.o

info/infoPg.o: info/infoPg.c common/debug.h common/error.auto.h common/error.h common/ini.h common/log.h common/logLevel.h common/memContext.h common/regExp.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/list.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h crypto/hash.h info/info.h info/infoPg.h postgres/info.h postgres/version.h storage/driver/posix/driverRead.h storage/driver/posix/driverWrite.h storage/fileRead.h storage/fileWrite.h storage/helper.h storage/info.h storage/storage.h version.h
	$(CC) $(CFLAGS) -c info/infoPg.c -o info/infoPg.o

main.o: main.c command/archive/get/get.h command/archive/push/push.h command/command.h command/help/help.h common/debug.h common/error.auto.h common/error.h common/exit.h common/lock.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h config/config.auto.h config/config.h config/define.auto.h config/define.h config/load.h perl/exec.h version.h
	$(CC) $(CFLAGS) -c main.c -o main.o

perl/config.o: perl/config.c common/debug.h common/error.auto.h common/error.h common/lock.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h config/config.auto.h config/config.h config/define.auto.h config/define.h
	$(CC) $(CFLAGS) -c perl/config.c -o perl/config.o

perl/exec.o: perl/exec.c ../libc/LibC.h common/debug.h common/encode.h common/error.auto.h common/error.h common/io/filter/filter.h common/lock.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h config/config.auto.h config/config.h config/define.auto.h config/define.h config/load.h config/parse.h crypto/cipher.h crypto/cipherBlock.h crypto/hash.h crypto/random.h perl/config.h perl/embed.auto.c perl/exec.h perl/libc.auto.c postgres/pageChecksum.h storage/driver/posix/driver.h storage/driver/posix/driverRead.h storage/driver/posix/driverWrite.h storage/info.h version.h ../libc/xs/common/encode.xsh ../libc/xs/crypto/cipherBlock.xsh ../libc/xs/crypto/hash.xsh
	$(CC) $(CFLAGS) -c perl/exec.c -o perl/exec.o

postgres/info.o: postgres/info.c common/debug.h common/error.auto.h common/error.h common/io/filter/filter.h common/io/filter/group.h common/io/read.h common/io/write.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h postgres/info.h postgres/type.h postgres/version.h storage/driver/posix/driverRead.h storage/driver/posix/driverWrite.h storage/fileRead.h storage/fileWrite.h storage/helper.h storage/info.h storage/storage.h version.h
	$(CC) $(CFLAGS) -c postgres/info.c -o postgres/info.o

postgres/pageChecksum.o: postgres/pageChecksum.c common/assert.h common/debug.h common/error.auto.h common/error.h common/log.h common/logLevel.h common/stackTrace.h common/type/convert.h postgres/pageChecksum.h postgres/type.h
	$(CC) $(CFLAGS) -funroll-loops -ftree-vectorize -c postgres/pageChecksum.c -o postgres/pageChecksum.o

storage/driver/posix/driver.o: storage/driver/posix/driver.c common/debug.h common/error.auto.h common/error.h common/io/filter/filter.h common/io/filter/group.h common/io/read.h common/io/write.h common/log.h common/logLevel.h common/memContext.h common/regExp.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h storage/driver/posix/driver.h storage/driver/posix/driverFile.h storage/driver/posix/driverRead.h storage/driver/posix/driverWrite.h storage/fileRead.h storage/fileWrite.h storage/info.h storage/storage.h version.h
	$(CC) $(CFLAGS) -c storage/driver/posix/driver.c -o storage/driver/posix/driver.o

storage/driver/posix/driverFile.o: storage/driver/posix/driverFile.c common/assert.h common/debug.h common/error.auto.h common/error.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/string.h storage/driver/posix/driverFile.h
	$(CC) $(CFLAGS) -c storage/driver/posix/driverFile.c -o storage/driver/posix/driverFile.o

storage/driver/posix/driverRead.o: storage/driver/posix/driverRead.c common/assert.h common/debug.h common/error.auto.h common/error.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/string.h storage/driver/posix/driverFile.h storage/driver/posix/driverRead.h
	$(CC) $(CFLAGS) -c storage/driver/posix/driverRead.c -o storage/driver/posix/driverRead.o

storage/driver/posix/driverWrite.o: storage/driver/posix/driverWrite.c common/assert.h common/debug.h common/error.auto.h common/error.h common/io/filter/filter.h common/io/filter/group.h common/io/write.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h storage/driver/posix/driver.h storage/driver/posix/driverFile.h storage/driver/posix/driverRead.h storage/driver/posix/driverWrite.h storage/fileWrite.h storage/info.h version.h
	$(CC) $(CFLAGS) -c storage/driver/posix/driverWrite.c -o storage/driver/posix/driverWrite.o

storage/fileRead.o: storage/fileRead.c common/assert.h common/debug.h common/error.auto.h common/error.h common/io/filter/filter.h common/io/filter/group.h common/io/read.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/variant.h common/type/variantList.h storage/driver/posix/driverRead.h storage/fileRead.h
	$(CC) $(CFLAGS) -c storage/fileRead.c -o storage/fileRead.o

storage/fileWrite.o: storage/fileWrite.c common/assert.h common/debug.h common/error.auto.h common/error.h common/io/filter/filter.h common/io/filter/group.h common/io/write.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/variant.h common/type/variantList.h storage/driver/posix/driverWrite.h storage/fileWrite.h version.h
	$(CC) $(CFLAGS) -c storage/fileWrite.c -o storage/fileWrite.o

storage/helper.o: storage/helper.c common/debug.h common/error.auto.h common/error.h common/io/filter/filter.h common/io/filter/group.h common/io/read.h common/io/write.h common/lock.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h config/config.auto.h config/config.h config/define.auto.h config/define.h storage/driver/posix/driverRead.h storage/driver/posix/driverWrite.h storage/fileRead.h storage/fileWrite.h storage/helper.h storage/info.h storage/storage.h version.h
	$(CC) $(CFLAGS) -c storage/helper.c -o storage/helper.o

storage/storage.o: storage/storage.c common/assert.h common/debug.h common/error.auto.h common/error.h common/io/filter/filter.h common/io/filter/group.h common/io/io.h common/io/read.h common/io/write.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h common/wait.h storage/driver/posix/driver.h storage/driver/posix/driverRead.h storage/driver/posix/driverWrite.h storage/fileRead.h storage/fileWrite.h storage/info.h storage/storage.h version.h
	$(CC) $(CFLAGS) -c storage/storage.c -o storage/storage.o
