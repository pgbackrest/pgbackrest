# Initialize configuration
AC_PREREQ([2.69])
AC_INIT([pgBackRest], [2.14dev])
AC_CONFIG_SRCDIR([version.h])

# Check compiler
AC_PROG_CC

# Check required libraries
AC_CHECK_LIB([crypto], [CRYPTO_new_ex_data], [], [AC_MSG_ERROR([library 'crypto' is required])])
AC_CHECK_LIB([ssl], [SSL_new], [], [AC_MSG_ERROR([library 'ssl' is required])])
AC_CHECK_LIB([xml2], [xmlSaveToBuffer], [], [AC_MSG_ERROR([library 'xml2' is required])])
AC_CHECK_LIB([z], [deflate], [], [AC_MSG_ERROR([library 'z' is required])])

# Check optional lz4 library
have_liblz4=1
# AC_SEARCH_LIBS(LZ4F_isError, lz4, [], [have_liblz4=1])
AC_CHECK_LIB([lz4], [LZ4F_isError], [], [have_liblz4=0])

if test $have_liblz4 == 1
then
    AC_DEFINE(HAVE_LIBLZ4)
fi

# Write output
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
