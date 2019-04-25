#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([FULL-PACKAGE-NAME], [VERSION], [BUG-REPORT-ADDRESS])
AC_CONFIG_SRCDIR([version.h])
# AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC
# AC_PROG_INSTALL

# Checks for libraries.
# FIXME: Replace `main' with a function in `-lcrypto':
# AC_CHECK_LIB([crypto], [main])
# FIXME: Replace `main' with a function in `-lssl':
# AC_CHECK_LIB([ssl], [main])
# FIXME: Replace `main' with a function in `-lxml2':
# AC_CHECK_LIB([xml2], [main])
# FIXME: Replace `main' with a function in `-lz':
# AC_CHECK_LIB([z], [main])
AC_CHECK_LIB([lz4], [LZ4F_isError])

# Checks for header files.
# AC_CHECK_HEADERS([arpa/inet.h fcntl.h inttypes.h limits.h netdb.h stddef.h stdint.h stdlib.h string.h strings.h sys/file.h sys/socket.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
# AC_CHECK_HEADER_STDBOOL
# AC_C_INLINE
# AC_TYPE_INT32_T
# AC_TYPE_INT64_T
# AC_TYPE_MODE_T
# AC_TYPE_PID_T
# AC_TYPE_SIZE_T
# AC_TYPE_SSIZE_T
# AC_TYPE_UINT16_T
# AC_TYPE_UINT32_T
# AC_TYPE_UINT64_T

# Checks for library functions.
# AC_FUNC_FORK
# AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK
# AC_FUNC_MALLOC
# AC_FUNC_REALLOC
# AC_CHECK_FUNCS([dup2 gettimeofday memchr memmove memset mkdir regcomp rmdir select socket strcasecmp strchr strerror strstr strtol strtoull])

AC_CONFIG_FILES([../config/Makefile])
AC_OUTPUT
