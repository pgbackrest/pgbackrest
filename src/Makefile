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
CINCLUDE = -I.

# Compile warnings
CWARN = -Wfatal-errors -Wall -Wextra -Wwrite-strings -Wno-clobbered -Wswitch-enum

# Automatically generate Perl compile options for the local system
CPERL = `perl -MExtUtils::Embed -e ccopts`

# Extra compile options to be set by caller
CEXTRA =

# Concatenate options for easy usage
CFLAGS = $(CINCLUDE) $(CSTD) $(COPT) $(CWARN) $(CPERL) $(CEXTRA)

####################################################################################################################################
# Link options
####################################################################################################################################
# Automatically generate Perl linker options for the local system
LDPERL = `perl -MExtUtils::Embed -e ldopts`

# Extra linker options to be set by caller
LDEXTRA =

# Concatenate options for easy usage
LDFLAGS = $(LDPERL) $(LDEXTRA)

####################################################################################################################################
# Install options
####################################################################################################################################
# Modify destination install directory
DESTDIR =

####################################################################################################################################
# List of required source files.  main.c should always be listed last and the rest in alpha order.
####################################################################################################################################
SRCS = \
	command/archive/push/push.c \
	command/help/help.c \
	command/command.c \
	common/error.c \
	common/exit.c \
	common/ini.c \
	common/log.c \
	common/memContext.c \
	common/regExp.c \
	common/time.c \
	common/type/buffer.c \
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
	perl/config.c \
	perl/exec.c \
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

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

####################################################################################################################################
# Installation.  DESTDIR can be used to modify the install location.
####################################################################################################################################
install: pgbackrest
	sudo install -d $(DESTDIR)/usr/bin
	sudo install -m 755 pgbackrest $(DESTDIR)/usr/bin
