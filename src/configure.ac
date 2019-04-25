# Initialize configuration
AC_PREREQ([2.69])
AC_INIT([pgBackRest], [2.14dev])
AC_CONFIG_SRCDIR([version.h])

# Check compiler
AC_PROG_CC_C99

# Check required Perl (this must be the first library checked since it overwrites LIBS)
AC_CHECK_LIB([perl], [perl_parse], [], [AC_MSG_ERROR([library 'perl' is required])])
LIBS=`perl -MExtUtils::Embed -e ldopts`

# Check other required libraries
AC_CHECK_LIB([crypto], [CRYPTO_new_ex_data], [], [AC_MSG_ERROR([library 'crypto' is required])])
AC_CHECK_LIB([ssl], [SSL_new], [], [AC_MSG_ERROR([library 'ssl' is required])])
AC_CHECK_LIB([xml2], [xmlSaveToBuffer], [], [AC_MSG_ERROR([library 'xml2' is required])])
AC_CHECK_LIB([z], [deflate], [], [AC_MSG_ERROR([library 'z' is required])])

# Check optional libraries
AC_CHECK_LIB([lz4], [LZ4F_isError])

# Write output
AC_CONFIG_HEADERS([build.auto.h])
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
