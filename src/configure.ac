# Initialize configuration
AC_PREREQ([2.69])
AC_INIT([pgBackRest], [2.14dev])
AC_CONFIG_SRCDIR([version.h])

# Check compiler
AC_PROG_CC_C99

# Enable internal test code and assertions by undefining NDEBUG
warning='-Wall -Wextra -Wno-clobbered'

AC_ARG_ENABLE(
    test, [AS_HELP_STRING([--enable-test], [enable internal test code and assertions for debugging])])

if test "$enable_test" != yes
then
    AC_DEFINE(NDEBUG)

    # Enable optimizations if not testing and they have not been disabled
    AC_ARG_ENABLE(optimize, [AS_HELP_STRING([--disable-optimize], [disable compiler optimizations])])

    if test "$enable_optimize" != no
    then
        AC_SUBST(COPTIMIZE, "-O2")
        AC_SUBST(COPTIMIZE_PAGE_CHECKSUM, "-funroll-loops -ftree-vectorize")
    fi

    AC_SUBST(CWARNING, $warning)
else
    AC_SUBST(CWARNING, "$warning -Wwrite-strings -Wswitch-enum -Wconversion -Wformat=2 -Wformat-nonliteral -Wno-missing-field-initializers -Wstrict-prototypes -Wpointer-arith -Wvla")
fi

# Set includes
AC_SUBST(CINCLUDE, "-I. -I../libc")

# Set library C options
AC_SUBST(CLIBRARY, "`perl -MExtUtils::Embed -e ccopts` `xml2-config --cflags`")

# Check required Perl
LIBS_BEFORE_PERL=$LIBS
AC_CHECK_LIB([perl], [perl_parse], [], [AC_MSG_ERROR([library 'perl' is required])])
LIBS="$LIBS_BEFORE_PERL `perl -MExtUtils::Embed -e ldopts`"

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
