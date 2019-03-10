####################################################################################################################################
# pgBackRest Makefile
####################################################################################################################################

####################################################################################################################################
# Compile options
####################################################################################################################################
CC=gcc

# Compile using C99 and Posix 2001 standards (also _DARWIN_C_SOURCE for MacOS)
#
# For BSD variants use: CSTD = -std=c99
CSTD = -std=c99 -D_POSIX_C_SOURCE=200112L -D_DARWIN_C_SOURCE

# Compile optimizations
COPT = -O2

# Locations of header files
CINCLUDE = -I. -I../libc

# Compile warnings
CWARN = -Wfatal-errors -Wall -Wextra -Wwrite-strings -Wswitch-enum -Wconversion -Wformat=2 -Wformat-nonliteral \
		-Wno-clobbered -Wno-missing-field-initializers -Wstrict-prototypes -Wpointer-arith -Wvla

# Automatically generate Perl compile options for the local system
CPERL = `perl -MExtUtils::Embed -e ccopts` -DWITH_PERL

# Automatically generate xml compile options for the local system
CXML = `xml2-config --cflags`

# Debug options
CDEBUG = -DNDEBUG

# Extra compile options to be set by caller
CEXTRA =

# Concatenate options for easy usage
CFLAGS = $(CINCLUDE) $(CSTD) $(COPT) $(CWARN) $(CXML) $(CPERL) $(CDEBUG) $(CEXTRA)

####################################################################################################################################
# Link options
####################################################################################################################################
# Automatically generate Perl linker options for the local system
LDPERL = `perl -MExtUtils::Embed -e ldopts`

# Extra linker options to be set by caller
LDEXTRA =

# Concatenate options for easy usage
LDFLAGS = -lcrypto -lssl -lxml2 -lz $(LDPERL) $(LDEXTRA)

####################################################################################################################################
# Install options
####################################################################################################################################
# Modify destination install directory
DESTDIR = /usr/bin

####################################################################################################################################
# List of required source files.  main.c should always be listed last and the rest in alpha order.
####################################################################################################################################
SRCS = \
	command/archive/common.c \
	command/archive/get/file.c \
	command/archive/get/get.c \
	command/archive/get/protocol.c \
	command/archive/push/push.c \
	command/help/help.c \
	command/info/info.c \
	command/command.c \
	command/control/control.c \
	command/local/local.c \
	command/remote/remote.c \
	common/compress/gzip/common.c \
	common/compress/gzip/compress.c \
	common/compress/gzip/decompress.c \
	common/debug.c \
	common/encode.c \
	common/encode/base64.c \
	common/error.c \
	common/exec.c \
	common/exit.c \
	common/fork.c \
	common/io/bufferRead.c \
	common/io/bufferWrite.c \
	common/io/filter/buffer.c \
	common/io/filter/filter.c \
	common/io/filter/group.c \
	common/io/filter/size.c \
	common/io/handleRead.c \
	common/io/handleWrite.c \
	common/io/http/client.c \
	common/io/http/common.c \
	common/io/http/header.c \
	common/io/http/query.c \
	common/io/io.c \
	common/io/read.c \
	common/io/tls/client.c \
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
	common/type/json.c \
	common/type/keyValue.c \
	common/type/list.c \
	common/type/string.c \
	common/type/stringList.c \
	common/type/variant.c \
	common/type/variantList.c \
	common/type/xml.c \
	common/wait.c \
	config/config.c \
	config/define.c \
	config/exec.c \
	config/load.c \
	config/parse.c \
	config/protocol.c \
	crypto/cipherBlock.c \
	crypto/hash.c \
	crypto/crypto.c \
	info/info.c \
	info/infoArchive.c \
	info/infoBackup.c \
	info/infoManifest.c \
	info/infoPg.c \
	perl/config.c \
	perl/exec.c \
	postgres/interface.c \
	postgres/interface/v083.c \
	postgres/interface/v084.c \
	postgres/interface/v090.c \
	postgres/interface/v091.c \
	postgres/interface/v092.c \
	postgres/interface/v093.c \
	postgres/interface/v094.c \
	postgres/interface/v095.c \
	postgres/interface/v096.c \
	postgres/interface/v100.c \
	postgres/interface/v110.c \
	postgres/pageChecksum.c \
	protocol/client.c \
	protocol/command.c \
	protocol/helper.c \
	protocol/parallel.c \
	protocol/parallelJob.c \
	protocol/server.c \
	storage/driver/posix/storage.c \
	storage/driver/posix/common.c \
	storage/driver/posix/fileRead.c \
	storage/driver/posix/fileWrite.c \
	storage/driver/remote/fileRead.c \
	storage/driver/remote/protocol.c \
	storage/driver/remote/storage.c \
	storage/driver/s3/fileRead.c \
	storage/driver/s3/storage.c \
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
	install -d $(DESTDIR)
	install -m 755 pgbackrest $(DESTDIR)

####################################################################################################################################
# Uninstallation.  DESTDIR should be set to the same value as when installed.
####################################################################################################################################
uninstall:
	rm -f $(DESTDIR)/pgbackrest

####################################################################################################################################
# Clean object files and executable created by make
####################################################################################################################################
.PHONY = clean

clean:
	rm -f $(OBJS)
	rm -f pgbackrest

####################################################################################################################################
# Compile rules
####################################################################################################################################
command/archive/common.o: command/archive/common.c command/archive/common.h common/assert.h common/debug.h common/error.auto.h common/error.h common/io/filter/filter.h common/io/filter/group.h common/io/read.h common/io/write.h common/log.h common/logLevel.h common/memContext.h common/regExp.h common/stackTrace.h common/time.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h common/wait.h postgres/version.h storage/fileRead.h storage/fileWrite.h storage/helper.h storage/info.h storage/storage.h
	$(CC) $(CFLAGS) -c command/archive/common.c -o command/archive/common.o

command/archive/get/file.o: command/archive/get/file.c command/archive/common.h command/archive/get/file.h command/control/control.h common/assert.h common/compress/gzip/common.h common/compress/gzip/decompress.h common/debug.h common/error.auto.h common/error.h common/ini.h common/io/filter/filter.h common/io/filter/group.h common/io/read.h common/io/write.h common/lock.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/time.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h config/config.auto.h config/config.h config/define.auto.h config/define.h crypto/cipherBlock.h crypto/crypto.h info/infoArchive.h info/infoPg.h postgres/interface.h storage/fileRead.h storage/fileWrite.h storage/helper.h storage/info.h storage/storage.h
	$(CC) $(CFLAGS) -c command/archive/get/file.c -o command/archive/get/file.o

command/archive/get/get.o: command/archive/get/get.c command/archive/common.h command/archive/get/file.h command/archive/get/protocol.h command/command.h common/assert.h common/debug.h common/error.auto.h common/error.h common/fork.h common/io/filter/filter.h common/io/filter/group.h common/io/read.h common/io/write.h common/lock.h common/log.h common/logLevel.h common/memContext.h common/regExp.h common/stackTrace.h common/time.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h common/wait.h config/config.auto.h config/config.h config/define.auto.h config/define.h config/exec.h crypto/crypto.h perl/exec.h postgres/interface.h protocol/client.h protocol/command.h protocol/helper.h protocol/parallel.h protocol/parallelJob.h protocol/server.h storage/fileRead.h storage/fileWrite.h storage/helper.h storage/info.h storage/storage.h
	$(CC) $(CFLAGS) -c command/archive/get/get.c -o command/archive/get/get.o

command/archive/get/protocol.o: command/archive/get/protocol.c command/archive/get/file.h command/archive/get/protocol.h common/assert.h common/debug.h common/error.auto.h common/error.h common/io/filter/filter.h common/io/filter/group.h common/io/io.h common/io/read.h common/io/write.h common/lock.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/time.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h config/config.auto.h config/config.h config/define.auto.h config/define.h crypto/crypto.h protocol/server.h storage/fileRead.h storage/fileWrite.h storage/helper.h storage/info.h storage/storage.h
	$(CC) $(CFLAGS) -c command/archive/get/protocol.c -o command/archive/get/protocol.o

command/archive/push/push.o: command/archive/push/push.c command/archive/common.h command/command.h common/assert.h common/debug.h common/error.auto.h common/error.h common/fork.h common/io/filter/filter.h common/io/filter/group.h common/io/read.h common/io/write.h common/lock.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/time.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h common/wait.h config/config.auto.h config/config.h config/define.auto.h config/define.h config/load.h perl/exec.h storage/fileRead.h storage/fileWrite.h storage/helper.h storage/info.h storage/storage.h
	$(CC) $(CFLAGS) -c command/archive/push/push.c -o command/archive/push/push.o

command/command.o: command/command.c common/assert.h common/debug.h common/error.auto.h common/error.h common/lock.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/time.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h config/config.auto.h config/config.h config/define.auto.h config/define.h version.h
	$(CC) $(CFLAGS) -c command/command.c -o command/command.o

command/control/control.o: command/control/control.c command/control/control.h common/assert.h common/debug.h common/error.auto.h common/error.h common/io/filter/filter.h common/io/filter/group.h common/io/read.h common/io/write.h common/lock.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/time.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h config/config.auto.h config/config.h config/define.auto.h config/define.h storage/fileRead.h storage/fileWrite.h storage/helper.h storage/info.h storage/storage.h
	$(CC) $(CFLAGS) -c command/control/control.c -o command/control/control.o

command/help/help.o: command/help/help.c common/assert.h common/debug.h common/error.auto.h common/error.h common/io/filter/filter.h common/io/filter/group.h common/io/handleWrite.h common/io/write.h common/lock.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/time.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h config/config.auto.h config/config.h config/define.auto.h config/define.h version.h
	$(CC) $(CFLAGS) -c command/help/help.c -o command/help/help.o

command/info/info.o: command/info/info.c command/archive/common.h command/info/info.h common/assert.h common/debug.h common/error.auto.h common/error.h common/ini.h common/io/filter/filter.h common/io/filter/group.h common/io/handleWrite.h common/io/read.h common/io/write.h common/lock.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/time.h common/type/buffer.h common/type/convert.h common/type/json.h common/type/keyValue.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h config/config.auto.h config/config.h config/define.auto.h config/define.h crypto/crypto.h crypto/hash.h info/info.h info/infoArchive.h info/infoBackup.h info/infoPg.h perl/exec.h postgres/interface.h storage/fileRead.h storage/fileWrite.h storage/helper.h storage/info.h storage/storage.h
	$(CC) $(CFLAGS) -c command/info/info.c -o command/info/info.o

command/local/local.o: command/local/local.c command/archive/get/protocol.h common/assert.h common/debug.h common/error.auto.h common/error.h common/io/filter/filter.h common/io/filter/group.h common/io/handleRead.h common/io/handleWrite.h common/io/read.h common/io/write.h common/lock.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/time.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h config/config.auto.h config/config.h config/define.auto.h config/define.h config/protocol.h protocol/client.h protocol/command.h protocol/helper.h protocol/server.h
	$(CC) $(CFLAGS) -c command/local/local.c -o command/local/local.o

command/remote/remote.o: command/remote/remote.c common/assert.h common/debug.h common/error.auto.h common/error.h common/io/filter/filter.h common/io/filter/group.h common/io/handleRead.h common/io/handleWrite.h common/io/read.h common/io/write.h common/lock.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/time.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h config/config.auto.h config/config.h config/define.auto.h config/define.h config/protocol.h protocol/client.h protocol/command.h protocol/helper.h protocol/server.h storage/driver/remote/protocol.h
	$(CC) $(CFLAGS) -c command/remote/remote.c -o command/remote/remote.o

common/compress/gzip/common.o: common/compress/gzip/common.c common/assert.h common/compress/gzip/common.h common/debug.h common/error.auto.h common/error.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/convert.h
	$(CC) $(CFLAGS) -c common/compress/gzip/common.c -o common/compress/gzip/common.o

common/compress/gzip/compress.o: common/compress/gzip/compress.c common/assert.h common/compress/gzip/common.h common/compress/gzip/compress.h common/debug.h common/error.auto.h common/error.h common/io/filter/filter.h common/io/filter/filter.intern.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/variant.h common/type/variantList.h
	$(CC) $(CFLAGS) -c common/compress/gzip/compress.c -o common/compress/gzip/compress.o

common/compress/gzip/decompress.o: common/compress/gzip/decompress.c common/assert.h common/compress/gzip/common.h common/compress/gzip/decompress.h common/debug.h common/error.auto.h common/error.h common/io/filter/filter.h common/io/filter/filter.intern.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/variant.h common/type/variantList.h
	$(CC) $(CFLAGS) -c common/compress/gzip/decompress.c -o common/compress/gzip/decompress.o

common/debug.o: common/debug.c common/assert.h common/debug.h common/error.auto.h common/error.h common/logLevel.h common/stackTrace.h common/type/convert.h
	$(CC) $(CFLAGS) -c common/debug.c -o common/debug.o

common/encode.o: common/encode.c common/assert.h common/debug.h common/encode.h common/encode/base64.h common/error.auto.h common/error.h common/logLevel.h common/stackTrace.h common/type/convert.h
	$(CC) $(CFLAGS) -c common/encode.c -o common/encode.o

common/encode/base64.o: common/encode/base64.c common/assert.h common/debug.h common/encode/base64.h common/error.auto.h common/error.h common/logLevel.h common/stackTrace.h common/type/convert.h
	$(CC) $(CFLAGS) -c common/encode/base64.c -o common/encode/base64.o

common/error.o: common/error.c common/error.auto.c common/error.auto.h common/error.h common/logLevel.h common/stackTrace.h
	$(CC) $(CFLAGS) -c common/error.c -o common/error.o

common/exec.o: common/exec.c common/assert.h common/debug.h common/error.auto.h common/error.h common/exec.h common/io/filter/filter.h common/io/filter/group.h common/io/handleRead.h common/io/handleWrite.h common/io/io.h common/io/read.h common/io/read.intern.h common/io/write.h common/io/write.intern.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/time.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h common/wait.h
	$(CC) $(CFLAGS) -c common/exec.c -o common/exec.o

common/exit.o: common/exit.c command/command.h common/assert.h common/debug.h common/error.auto.h common/error.h common/exit.h common/io/filter/filter.h common/io/filter/group.h common/io/read.h common/io/write.h common/lock.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/time.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h config/config.auto.h config/config.h config/define.auto.h config/define.h perl/exec.h protocol/client.h protocol/command.h protocol/helper.h
	$(CC) $(CFLAGS) -c common/exit.c -o common/exit.o

common/fork.o: common/fork.c common/assert.h common/debug.h common/error.auto.h common/error.h common/log.h common/logLevel.h common/stackTrace.h common/type/convert.h
	$(CC) $(CFLAGS) -c common/fork.c -o common/fork.o

common/ini.o: common/ini.c common/assert.h common/debug.h common/error.auto.h common/error.h common/ini.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/variant.h common/type/variantList.h
	$(CC) $(CFLAGS) -c common/ini.c -o common/ini.o

common/io/bufferRead.o: common/io/bufferRead.c common/assert.h common/debug.h common/error.auto.h common/error.h common/io/bufferRead.h common/io/filter/filter.h common/io/filter/group.h common/io/read.h common/io/read.intern.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/variant.h common/type/variantList.h
	$(CC) $(CFLAGS) -c common/io/bufferRead.c -o common/io/bufferRead.o

common/io/bufferWrite.o: common/io/bufferWrite.c common/assert.h common/debug.h common/error.auto.h common/error.h common/io/bufferWrite.h common/io/filter/filter.h common/io/filter/group.h common/io/write.h common/io/write.intern.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/variant.h common/type/variantList.h
	$(CC) $(CFLAGS) -c common/io/bufferWrite.c -o common/io/bufferWrite.o

common/io/filter/buffer.o: common/io/filter/buffer.c common/assert.h common/debug.h common/error.auto.h common/error.h common/io/filter/buffer.h common/io/filter/filter.h common/io/filter/filter.intern.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/variant.h common/type/variantList.h
	$(CC) $(CFLAGS) -c common/io/filter/buffer.c -o common/io/filter/buffer.o

common/io/filter/filter.o: common/io/filter/filter.c common/assert.h common/debug.h common/error.auto.h common/error.h common/io/filter/filter.h common/io/filter/filter.intern.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/variant.h common/type/variantList.h
	$(CC) $(CFLAGS) -c common/io/filter/filter.c -o common/io/filter/filter.o

common/io/filter/group.o: common/io/filter/group.c common/assert.h common/debug.h common/error.auto.h common/error.h common/io/filter/buffer.h common/io/filter/filter.h common/io/filter/filter.intern.h common/io/filter/group.h common/io/io.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/list.h common/type/string.h common/type/variant.h common/type/variantList.h
	$(CC) $(CFLAGS) -c common/io/filter/group.c -o common/io/filter/group.o

common/io/filter/size.o: common/io/filter/size.c common/assert.h common/debug.h common/error.auto.h common/error.h common/io/filter/filter.h common/io/filter/filter.intern.h common/io/filter/size.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/variant.h common/type/variantList.h
	$(CC) $(CFLAGS) -c common/io/filter/size.c -o common/io/filter/size.o

common/io/handleRead.o: common/io/handleRead.c common/assert.h common/debug.h common/error.auto.h common/error.h common/io/filter/filter.h common/io/filter/group.h common/io/handleRead.h common/io/read.h common/io/read.intern.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/time.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/variant.h common/type/variantList.h
	$(CC) $(CFLAGS) -c common/io/handleRead.c -o common/io/handleRead.o

common/io/handleWrite.o: common/io/handleWrite.c common/assert.h common/debug.h common/error.auto.h common/error.h common/io/filter/filter.h common/io/filter/group.h common/io/handleWrite.h common/io/write.h common/io/write.intern.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/variant.h common/type/variantList.h
	$(CC) $(CFLAGS) -c common/io/handleWrite.c -o common/io/handleWrite.o

common/io/http/client.o: common/io/http/client.c common/assert.h common/debug.h common/error.auto.h common/error.h common/io/filter/filter.h common/io/filter/group.h common/io/http/client.h common/io/http/common.h common/io/http/header.h common/io/http/query.h common/io/io.h common/io/read.h common/io/read.intern.h common/io/tls/client.h common/io/write.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/time.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h common/wait.h
	$(CC) $(CFLAGS) -c common/io/http/client.c -o common/io/http/client.o

common/io/http/common.o: common/io/http/common.c common/assert.h common/debug.h common/error.auto.h common/error.h common/io/http/common.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/string.h
	$(CC) $(CFLAGS) -c common/io/http/common.c -o common/io/http/common.o

common/io/http/header.o: common/io/http/header.c common/assert.h common/debug.h common/error.auto.h common/error.h common/io/http/header.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h
	$(CC) $(CFLAGS) -c common/io/http/header.c -o common/io/http/header.o

common/io/http/query.o: common/io/http/query.c common/assert.h common/debug.h common/error.auto.h common/error.h common/io/http/common.h common/io/http/query.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h
	$(CC) $(CFLAGS) -c common/io/http/query.c -o common/io/http/query.o

common/io/io.o: common/io/io.c common/assert.h common/debug.h common/error.auto.h common/error.h common/io/io.h common/log.h common/logLevel.h common/stackTrace.h common/type/convert.h
	$(CC) $(CFLAGS) -c common/io/io.c -o common/io/io.o

common/io/read.o: common/io/read.c common/assert.h common/debug.h common/error.auto.h common/error.h common/io/filter/filter.h common/io/filter/group.h common/io/io.h common/io/read.h common/io/read.intern.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/variant.h common/type/variantList.h
	$(CC) $(CFLAGS) -c common/io/read.c -o common/io/read.o

common/io/tls/client.o: common/io/tls/client.c common/assert.h common/debug.h common/error.auto.h common/error.h common/io/filter/filter.h common/io/filter/group.h common/io/io.h common/io/read.h common/io/read.intern.h common/io/tls/client.h common/io/write.h common/io/write.intern.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/time.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/variant.h common/type/variantList.h common/wait.h crypto/crypto.h
	$(CC) $(CFLAGS) -c common/io/tls/client.c -o common/io/tls/client.o

common/io/write.o: common/io/write.c common/assert.h common/debug.h common/error.auto.h common/error.h common/io/filter/filter.h common/io/filter/group.h common/io/io.h common/io/write.h common/io/write.intern.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/variant.h common/type/variantList.h
	$(CC) $(CFLAGS) -c common/io/write.c -o common/io/write.o

common/lock.o: common/lock.c common/assert.h common/debug.h common/error.auto.h common/error.h common/io/filter/filter.h common/io/filter/group.h common/io/handleWrite.h common/io/read.h common/io/write.h common/lock.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/time.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h common/wait.h storage/fileRead.h storage/fileWrite.h storage/helper.h storage/info.h storage/storage.h storage/storage.intern.h version.h
	$(CC) $(CFLAGS) -c common/lock.c -o common/lock.o

common/log.o: common/log.c common/assert.h common/debug.h common/error.auto.h common/error.h common/log.h common/logLevel.h common/stackTrace.h common/time.h common/type/convert.h
	$(CC) $(CFLAGS) -c common/log.c -o common/log.o

common/memContext.o: common/memContext.c common/assert.h common/debug.h common/error.auto.h common/error.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/convert.h
	$(CC) $(CFLAGS) -c common/memContext.c -o common/memContext.o

common/regExp.o: common/regExp.c common/assert.h common/debug.h common/error.auto.h common/error.h common/logLevel.h common/memContext.h common/regExp.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/string.h
	$(CC) $(CFLAGS) -c common/regExp.c -o common/regExp.o

common/stackTrace.o: common/stackTrace.c common/assert.h common/error.auto.h common/error.h common/logLevel.h common/stackTrace.h
	$(CC) $(CFLAGS) -c common/stackTrace.c -o common/stackTrace.o

common/time.o: common/time.c common/assert.h common/debug.h common/error.auto.h common/error.h common/logLevel.h common/stackTrace.h common/time.h common/type/convert.h
	$(CC) $(CFLAGS) -c common/time.c -o common/time.o

common/type/buffer.o: common/type/buffer.c common/assert.h common/debug.h common/error.auto.h common/error.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/string.h
	$(CC) $(CFLAGS) -c common/type/buffer.c -o common/type/buffer.o

common/type/convert.o: common/type/convert.c common/assert.h common/debug.h common/error.auto.h common/error.h common/logLevel.h common/stackTrace.h common/type/convert.h
	$(CC) $(CFLAGS) -c common/type/convert.c -o common/type/convert.o

common/type/json.o: common/type/json.c common/assert.h common/debug.h common/error.auto.h common/error.h common/log.h common/logLevel.h common/stackTrace.h common/type/convert.h common/type/json.h common/type/keyValue.h common/type/variantList.h
	$(CC) $(CFLAGS) -c common/type/json.c -o common/type/json.o

common/type/keyValue.o: common/type/keyValue.c common/assert.h common/debug.h common/error.auto.h common/error.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/list.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h
	$(CC) $(CFLAGS) -c common/type/keyValue.c -o common/type/keyValue.o

common/type/list.o: common/type/list.c common/assert.h common/debug.h common/error.auto.h common/error.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/list.h common/type/string.h
	$(CC) $(CFLAGS) -c common/type/list.c -o common/type/list.o

common/type/string.o: common/type/string.c common/assert.h common/debug.h common/error.auto.h common/error.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/string.h
	$(CC) $(CFLAGS) -c common/type/string.c -o common/type/string.o

common/type/stringList.o: common/type/stringList.c common/assert.h common/debug.h common/error.auto.h common/error.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/list.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h
	$(CC) $(CFLAGS) -c common/type/stringList.c -o common/type/stringList.o

common/type/variant.o: common/type/variant.c common/assert.h common/debug.h common/error.auto.h common/error.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/variant.h common/type/variantList.h
	$(CC) $(CFLAGS) -c common/type/variant.c -o common/type/variant.o

common/type/variantList.o: common/type/variantList.c common/assert.h common/debug.h common/error.auto.h common/error.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/list.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h
	$(CC) $(CFLAGS) -c common/type/variantList.c -o common/type/variantList.o

common/type/xml.o: common/type/xml.c common/assert.h common/debug.h common/error.auto.h common/error.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/list.h common/type/string.h common/type/xml.h
	$(CC) $(CFLAGS) -c common/type/xml.c -o common/type/xml.o

common/wait.o: common/wait.c common/assert.h common/debug.h common/error.auto.h common/error.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/time.h common/type/convert.h common/wait.h
	$(CC) $(CFLAGS) -c common/wait.c -o common/wait.o

config/config.o: config/config.c common/assert.h common/debug.h common/error.auto.h common/error.h common/lock.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/time.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h config/config.auto.c config/config.auto.h config/config.h config/define.auto.h config/define.h
	$(CC) $(CFLAGS) -c config/config.c -o config/config.o

config/define.o: config/define.c common/assert.h common/debug.h common/error.auto.h common/error.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/string.h config/define.auto.c config/define.auto.h config/define.h
	$(CC) $(CFLAGS) -c config/define.c -o config/define.o

config/exec.o: config/exec.c common/assert.h common/debug.h common/error.auto.h common/error.h common/lock.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/time.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h config/config.auto.h config/config.h config/define.auto.h config/define.h config/exec.h
	$(CC) $(CFLAGS) -c config/exec.c -o config/exec.o

config/load.o: config/load.c command/command.h common/assert.h common/debug.h common/error.auto.h common/error.h common/io/io.h common/lock.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/time.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h config/config.auto.h config/config.h config/define.auto.h config/define.h config/load.h config/parse.h
	$(CC) $(CFLAGS) -c config/load.c -o config/load.o

config/parse.o: config/parse.c common/assert.h common/debug.h common/error.auto.h common/error.h common/ini.h common/io/filter/filter.h common/io/filter/group.h common/io/read.h common/io/write.h common/lock.h common/log.h common/logLevel.h common/memContext.h common/regExp.h common/stackTrace.h common/time.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h config/config.auto.h config/config.h config/define.auto.h config/define.h config/parse.auto.c config/parse.h storage/fileRead.h storage/fileWrite.h storage/helper.h storage/info.h storage/storage.h version.h
	$(CC) $(CFLAGS) -c config/parse.c -o config/parse.o

config/protocol.o: config/protocol.c common/assert.h common/debug.h common/error.auto.h common/error.h common/io/filter/filter.h common/io/filter/group.h common/io/io.h common/io/read.h common/io/write.h common/lock.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/time.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h config/config.auto.h config/config.h config/define.auto.h config/define.h config/protocol.h protocol/client.h protocol/command.h protocol/server.h
	$(CC) $(CFLAGS) -c config/protocol.c -o config/protocol.o

crypto/cipherBlock.o: crypto/cipherBlock.c common/assert.h common/debug.h common/error.auto.h common/error.h common/io/filter/filter.h common/io/filter/filter.intern.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/variant.h common/type/variantList.h crypto/cipherBlock.h crypto/crypto.h
	$(CC) $(CFLAGS) -c crypto/cipherBlock.c -o crypto/cipherBlock.o

crypto/crypto.o: crypto/crypto.c common/assert.h common/debug.h common/error.auto.h common/error.h common/log.h common/logLevel.h common/stackTrace.h common/type/convert.h crypto/crypto.h
	$(CC) $(CFLAGS) -c crypto/crypto.c -o crypto/crypto.o

crypto/hash.o: crypto/hash.c common/assert.h common/debug.h common/error.auto.h common/error.h common/io/filter/filter.h common/io/filter/filter.intern.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/variant.h common/type/variantList.h crypto/crypto.h crypto/hash.h
	$(CC) $(CFLAGS) -c crypto/hash.c -o crypto/hash.o

info/info.o: info/info.c common/assert.h common/debug.h common/error.auto.h common/error.h common/ini.h common/io/filter/filter.h common/io/filter/group.h common/io/read.h common/io/write.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/time.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h crypto/cipherBlock.h crypto/crypto.h crypto/hash.h info/info.h storage/fileRead.h storage/fileWrite.h storage/helper.h storage/info.h storage/storage.h version.h
	$(CC) $(CFLAGS) -c info/info.c -o info/info.o

info/infoArchive.o: info/infoArchive.c common/assert.h common/debug.h common/error.auto.h common/error.h common/ini.h common/io/filter/filter.h common/io/filter/group.h common/io/read.h common/io/write.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/time.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h crypto/crypto.h info/infoArchive.h info/infoPg.h postgres/interface.h storage/fileRead.h storage/fileWrite.h storage/helper.h storage/info.h storage/storage.h
	$(CC) $(CFLAGS) -c info/infoArchive.c -o info/infoArchive.o

info/infoBackup.o: info/infoBackup.c common/assert.h common/debug.h common/error.auto.h common/error.h common/ini.h common/io/filter/filter.h common/io/filter/group.h common/io/read.h common/io/write.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/time.h common/type/buffer.h common/type/convert.h common/type/json.h common/type/keyValue.h common/type/list.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h crypto/crypto.h crypto/hash.h info/info.h info/infoBackup.h info/infoManifest.h info/infoPg.h postgres/interface.h storage/fileRead.h storage/fileWrite.h storage/helper.h storage/info.h storage/storage.h
	$(CC) $(CFLAGS) -c info/infoBackup.c -o info/infoBackup.o

info/infoManifest.o: info/infoManifest.c common/error.auto.h common/error.h common/memContext.h common/type/buffer.h common/type/string.h info/infoManifest.h
	$(CC) $(CFLAGS) -c info/infoManifest.c -o info/infoManifest.o

info/infoPg.o: info/infoPg.c common/assert.h common/debug.h common/error.auto.h common/error.h common/ini.h common/io/filter/filter.h common/io/filter/group.h common/io/read.h common/io/write.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/time.h common/type/buffer.h common/type/convert.h common/type/json.h common/type/keyValue.h common/type/list.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h crypto/crypto.h crypto/hash.h info/info.h info/infoPg.h postgres/interface.h postgres/version.h storage/fileRead.h storage/fileWrite.h storage/helper.h storage/info.h storage/storage.h
	$(CC) $(CFLAGS) -c info/infoPg.c -o info/infoPg.o

main.o: main.c command/archive/get/get.h command/archive/push/push.h command/command.h command/help/help.h command/info/info.h command/local/local.h command/remote/remote.h common/assert.h common/debug.h common/error.auto.h common/error.h common/exit.h common/lock.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/time.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h config/config.auto.h config/config.h config/define.auto.h config/define.h config/load.h perl/exec.h postgres/interface.h version.h
	$(CC) $(CFLAGS) -c main.c -o main.o

perl/config.o: perl/config.c common/assert.h common/debug.h common/error.auto.h common/error.h common/lock.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/time.h common/type/buffer.h common/type/convert.h common/type/json.h common/type/keyValue.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h config/config.auto.h config/config.h config/define.auto.h config/define.h
	$(CC) $(CFLAGS) -c perl/config.c -o perl/config.o

perl/exec.o: perl/exec.c ../libc/LibC.h common/assert.h common/debug.h common/encode.h common/error.auto.h common/error.h common/io/filter/filter.h common/io/filter/group.h common/io/read.h common/io/write.h common/lock.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/time.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h config/config.auto.h config/config.h config/define.auto.h config/define.h config/load.h config/parse.h crypto/cipherBlock.h crypto/crypto.h crypto/hash.h perl/config.h perl/embed.auto.c perl/exec.h perl/libc.auto.c postgres/pageChecksum.h storage/driver/posix/fileRead.h storage/driver/posix/fileWrite.h storage/driver/posix/storage.h storage/fileRead.h storage/fileWrite.h storage/info.h storage/storage.h storage/storage.intern.h version.h ../libc/xs/common/encode.xsh ../libc/xs/crypto/cipherBlock.xsh ../libc/xs/crypto/hash.xsh
	$(CC) $(CFLAGS) -c perl/exec.c -o perl/exec.o

postgres/interface.o: postgres/interface.c common/assert.h common/debug.h common/error.auto.h common/error.h common/io/filter/filter.h common/io/filter/group.h common/io/read.h common/io/write.h common/log.h common/logLevel.h common/memContext.h common/regExp.h common/stackTrace.h common/time.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h postgres/interface.h postgres/interface/v083.h postgres/interface/v084.h postgres/interface/v090.h postgres/interface/v091.h postgres/interface/v092.h postgres/interface/v093.h postgres/interface/v094.h postgres/interface/v095.h postgres/interface/v096.h postgres/interface/v100.h postgres/interface/v110.h postgres/version.h storage/fileRead.h storage/fileWrite.h storage/helper.h storage/info.h storage/storage.h
	$(CC) $(CFLAGS) -c postgres/interface.c -o postgres/interface.o

postgres/interface/v083.o: postgres/interface/v083.c common/assert.h common/debug.h common/error.auto.h common/error.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/string.h postgres/interface.h postgres/interface/v083.auto.c postgres/interface/v083.h
	$(CC) $(CFLAGS) -c postgres/interface/v083.c -o postgres/interface/v083.o

postgres/interface/v084.o: postgres/interface/v084.c common/assert.h common/debug.h common/error.auto.h common/error.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/string.h postgres/interface.h postgres/interface/v084.auto.c postgres/interface/v084.h
	$(CC) $(CFLAGS) -c postgres/interface/v084.c -o postgres/interface/v084.o

postgres/interface/v090.o: postgres/interface/v090.c common/assert.h common/debug.h common/error.auto.h common/error.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/string.h postgres/interface.h postgres/interface/v090.auto.c postgres/interface/v090.h
	$(CC) $(CFLAGS) -c postgres/interface/v090.c -o postgres/interface/v090.o

postgres/interface/v091.o: postgres/interface/v091.c common/assert.h common/debug.h common/error.auto.h common/error.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/string.h postgres/interface.h postgres/interface/v091.auto.c postgres/interface/v091.h
	$(CC) $(CFLAGS) -c postgres/interface/v091.c -o postgres/interface/v091.o

postgres/interface/v092.o: postgres/interface/v092.c common/assert.h common/debug.h common/error.auto.h common/error.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/string.h postgres/interface.h postgres/interface/v092.auto.c postgres/interface/v092.h
	$(CC) $(CFLAGS) -c postgres/interface/v092.c -o postgres/interface/v092.o

postgres/interface/v093.o: postgres/interface/v093.c common/assert.h common/debug.h common/error.auto.h common/error.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/string.h postgres/interface.h postgres/interface/v093.auto.c postgres/interface/v093.h
	$(CC) $(CFLAGS) -c postgres/interface/v093.c -o postgres/interface/v093.o

postgres/interface/v094.o: postgres/interface/v094.c common/assert.h common/debug.h common/error.auto.h common/error.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/string.h postgres/interface.h postgres/interface/v094.auto.c postgres/interface/v094.h
	$(CC) $(CFLAGS) -c postgres/interface/v094.c -o postgres/interface/v094.o

postgres/interface/v095.o: postgres/interface/v095.c common/assert.h common/debug.h common/error.auto.h common/error.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/string.h postgres/interface.h postgres/interface/v095.auto.c postgres/interface/v095.h
	$(CC) $(CFLAGS) -c postgres/interface/v095.c -o postgres/interface/v095.o

postgres/interface/v096.o: postgres/interface/v096.c common/assert.h common/debug.h common/error.auto.h common/error.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/string.h postgres/interface.h postgres/interface/v096.auto.c postgres/interface/v096.h
	$(CC) $(CFLAGS) -c postgres/interface/v096.c -o postgres/interface/v096.o

postgres/interface/v100.o: postgres/interface/v100.c common/assert.h common/debug.h common/error.auto.h common/error.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/string.h postgres/interface.h postgres/interface/v100.auto.c postgres/interface/v100.h
	$(CC) $(CFLAGS) -c postgres/interface/v100.c -o postgres/interface/v100.o

postgres/interface/v110.o: postgres/interface/v110.c common/assert.h common/debug.h common/error.auto.h common/error.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/string.h postgres/interface.h postgres/interface/v110.auto.c postgres/interface/v110.h
	$(CC) $(CFLAGS) -c postgres/interface/v110.c -o postgres/interface/v110.o

postgres/pageChecksum.o: postgres/pageChecksum.c common/assert.h common/debug.h common/error.auto.h common/error.h common/log.h common/logLevel.h common/stackTrace.h common/type/convert.h postgres/pageChecksum.h
	$(CC) $(CFLAGS) -funroll-loops -ftree-vectorize -c postgres/pageChecksum.c -o postgres/pageChecksum.o

protocol/client.o: protocol/client.c common/assert.h common/debug.h common/error.auto.h common/error.h common/io/filter/filter.h common/io/filter/group.h common/io/read.h common/io/write.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/time.h common/type/buffer.h common/type/convert.h common/type/json.h common/type/keyValue.h common/type/string.h common/type/variant.h common/type/variantList.h protocol/client.h protocol/command.h version.h
	$(CC) $(CFLAGS) -c protocol/client.c -o protocol/client.o

protocol/command.o: protocol/command.c common/assert.h common/debug.h common/error.auto.h common/error.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/json.h common/type/keyValue.h common/type/string.h common/type/variant.h common/type/variantList.h protocol/command.h
	$(CC) $(CFLAGS) -c protocol/command.c -o protocol/command.o

protocol/helper.o: protocol/helper.c common/assert.h common/debug.h common/error.auto.h common/error.h common/exec.h common/io/filter/filter.h common/io/filter/group.h common/io/read.h common/io/write.h common/lock.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/time.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h config/config.auto.h config/config.h config/define.auto.h config/define.h config/exec.h config/protocol.h crypto/crypto.h protocol/client.h protocol/command.h protocol/helper.h protocol/server.h
	$(CC) $(CFLAGS) -c protocol/helper.c -o protocol/helper.o

protocol/parallel.o: protocol/parallel.c common/assert.h common/debug.h common/error.auto.h common/error.h common/io/filter/filter.h common/io/filter/group.h common/io/read.h common/io/write.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/time.h common/type/buffer.h common/type/convert.h common/type/json.h common/type/keyValue.h common/type/list.h common/type/string.h common/type/variant.h common/type/variantList.h protocol/client.h protocol/command.h protocol/parallel.h protocol/parallelJob.h
	$(CC) $(CFLAGS) -c protocol/parallel.c -o protocol/parallel.o

protocol/parallelJob.o: protocol/parallelJob.c common/assert.h common/debug.h common/error.auto.h common/error.h common/io/filter/filter.h common/io/filter/group.h common/io/read.h common/io/write.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/time.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/variant.h common/type/variantList.h protocol/client.h protocol/command.h protocol/parallelJob.h
	$(CC) $(CFLAGS) -c protocol/parallelJob.c -o protocol/parallelJob.o

protocol/server.o: protocol/server.c common/assert.h common/debug.h common/error.auto.h common/error.h common/io/filter/filter.h common/io/filter/group.h common/io/read.h common/io/write.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/time.h common/type/buffer.h common/type/convert.h common/type/json.h common/type/keyValue.h common/type/list.h common/type/string.h common/type/variant.h common/type/variantList.h protocol/client.h protocol/command.h protocol/server.h version.h
	$(CC) $(CFLAGS) -c protocol/server.c -o protocol/server.o

storage/driver/posix/common.o: storage/driver/posix/common.c common/assert.h common/debug.h common/error.auto.h common/error.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/string.h storage/driver/posix/common.h
	$(CC) $(CFLAGS) -c storage/driver/posix/common.c -o storage/driver/posix/common.o

storage/driver/posix/fileRead.o: storage/driver/posix/fileRead.c common/assert.h common/debug.h common/error.auto.h common/error.h common/io/filter/filter.h common/io/filter/group.h common/io/read.h common/io/read.intern.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/variant.h common/type/variantList.h storage/driver/posix/common.h storage/driver/posix/fileRead.h storage/driver/posix/storage.h storage/fileRead.h storage/fileRead.intern.h
	$(CC) $(CFLAGS) -c storage/driver/posix/fileRead.c -o storage/driver/posix/fileRead.o

storage/driver/posix/fileWrite.o: storage/driver/posix/fileWrite.c common/assert.h common/debug.h common/error.auto.h common/error.h common/io/filter/filter.h common/io/filter/group.h common/io/write.h common/io/write.intern.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/variant.h common/type/variantList.h storage/driver/posix/common.h storage/driver/posix/fileWrite.h storage/driver/posix/storage.h storage/fileWrite.h storage/fileWrite.intern.h version.h
	$(CC) $(CFLAGS) -c storage/driver/posix/fileWrite.c -o storage/driver/posix/fileWrite.o

storage/driver/posix/storage.o: storage/driver/posix/storage.c common/assert.h common/debug.h common/error.auto.h common/error.h common/io/filter/filter.h common/io/filter/group.h common/io/read.h common/io/write.h common/log.h common/logLevel.h common/memContext.h common/regExp.h common/stackTrace.h common/time.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h storage/driver/posix/common.h storage/driver/posix/fileRead.h storage/driver/posix/fileWrite.h storage/driver/posix/storage.h storage/fileRead.h storage/fileWrite.h storage/info.h storage/storage.h storage/storage.intern.h
	$(CC) $(CFLAGS) -c storage/driver/posix/storage.c -o storage/driver/posix/storage.o

storage/driver/remote/fileRead.o: storage/driver/remote/fileRead.c common/assert.h common/debug.h common/error.auto.h common/error.h common/io/filter/filter.h common/io/filter/group.h common/io/http/client.h common/io/http/header.h common/io/http/query.h common/io/read.h common/io/read.intern.h common/io/write.h common/log.h common/logLevel.h common/memContext.h common/regExp.h common/stackTrace.h common/time.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h protocol/client.h protocol/command.h protocol/server.h storage/driver/remote/fileRead.h storage/driver/remote/protocol.h storage/driver/remote/storage.h storage/fileRead.h storage/fileRead.intern.h storage/fileWrite.h storage/info.h storage/storage.h storage/storage.intern.h
	$(CC) $(CFLAGS) -c storage/driver/remote/fileRead.c -o storage/driver/remote/fileRead.o

storage/driver/remote/protocol.o: storage/driver/remote/protocol.c common/assert.h common/debug.h common/error.auto.h common/error.h common/io/filter/filter.h common/io/filter/group.h common/io/io.h common/io/read.h common/io/write.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/time.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h protocol/server.h storage/driver/remote/protocol.h storage/fileRead.h storage/fileWrite.h storage/helper.h storage/info.h storage/storage.h storage/storage.intern.h
	$(CC) $(CFLAGS) -c storage/driver/remote/protocol.c -o storage/driver/remote/protocol.o

storage/driver/remote/storage.o: storage/driver/remote/storage.c common/assert.h common/debug.h common/error.auto.h common/error.h common/io/filter/filter.h common/io/filter/group.h common/io/http/client.h common/io/http/header.h common/io/http/query.h common/io/read.h common/io/write.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/time.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h protocol/client.h protocol/command.h protocol/helper.h protocol/server.h storage/driver/remote/fileRead.h storage/driver/remote/protocol.h storage/driver/remote/storage.h storage/fileRead.h storage/fileWrite.h storage/info.h storage/storage.h storage/storage.intern.h
	$(CC) $(CFLAGS) -c storage/driver/remote/storage.c -o storage/driver/remote/storage.o

storage/driver/s3/fileRead.o: storage/driver/s3/fileRead.c common/assert.h common/debug.h common/error.auto.h common/error.h common/io/filter/filter.h common/io/filter/group.h common/io/http/client.h common/io/http/header.h common/io/http/query.h common/io/read.h common/io/read.intern.h common/io/write.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/time.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h storage/driver/s3/fileRead.h storage/driver/s3/storage.h storage/fileRead.h storage/fileRead.intern.h storage/fileWrite.h storage/info.h storage/storage.h storage/storage.intern.h
	$(CC) $(CFLAGS) -c storage/driver/s3/fileRead.c -o storage/driver/s3/fileRead.o

storage/driver/s3/storage.o: storage/driver/s3/storage.c common/assert.h common/debug.h common/error.auto.h common/error.h common/io/filter/filter.h common/io/filter/group.h common/io/http/client.h common/io/http/common.h common/io/http/header.h common/io/http/query.h common/io/read.h common/io/write.h common/log.h common/logLevel.h common/memContext.h common/regExp.h common/stackTrace.h common/time.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h common/type/xml.h crypto/hash.h storage/driver/s3/fileRead.h storage/driver/s3/storage.h storage/fileRead.h storage/fileWrite.h storage/info.h storage/storage.h storage/storage.intern.h
	$(CC) $(CFLAGS) -c storage/driver/s3/storage.c -o storage/driver/s3/storage.o

storage/fileRead.o: storage/fileRead.c common/assert.h common/debug.h common/error.auto.h common/error.h common/io/filter/filter.h common/io/filter/group.h common/io/read.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/variant.h common/type/variantList.h storage/fileRead.h storage/fileRead.intern.h
	$(CC) $(CFLAGS) -c storage/fileRead.c -o storage/fileRead.o

storage/fileWrite.o: storage/fileWrite.c common/assert.h common/debug.h common/error.auto.h common/error.h common/io/filter/filter.h common/io/filter/group.h common/io/write.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/variant.h common/type/variantList.h storage/fileWrite.h storage/fileWrite.intern.h version.h
	$(CC) $(CFLAGS) -c storage/fileWrite.c -o storage/fileWrite.o

storage/helper.o: storage/helper.c common/assert.h common/debug.h common/error.auto.h common/error.h common/io/filter/filter.h common/io/filter/group.h common/io/http/client.h common/io/http/header.h common/io/http/query.h common/io/read.h common/io/write.h common/lock.h common/log.h common/logLevel.h common/memContext.h common/regExp.h common/stackTrace.h common/time.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h config/config.auto.h config/config.h config/define.auto.h config/define.h protocol/client.h protocol/command.h protocol/helper.h storage/driver/posix/fileRead.h storage/driver/posix/fileWrite.h storage/driver/posix/storage.h storage/driver/remote/storage.h storage/driver/s3/storage.h storage/fileRead.h storage/fileWrite.h storage/helper.h storage/info.h storage/storage.h storage/storage.intern.h
	$(CC) $(CFLAGS) -c storage/helper.c -o storage/helper.o

storage/storage.o: storage/storage.c common/assert.h common/debug.h common/error.auto.h common/error.h common/io/filter/filter.h common/io/filter/group.h common/io/io.h common/io/read.h common/io/write.h common/log.h common/logLevel.h common/memContext.h common/stackTrace.h common/time.h common/type/buffer.h common/type/convert.h common/type/keyValue.h common/type/string.h common/type/stringList.h common/type/variant.h common/type/variantList.h common/wait.h storage/fileRead.h storage/fileWrite.h storage/info.h storage/storage.h storage/storage.intern.h
	$(CC) $(CFLAGS) -c storage/storage.c -o storage/storage.o
