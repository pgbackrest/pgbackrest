# Initialize configuration
# ----------------------------------------------------------------------------------------------------------------------------------
AC_PREREQ([2.69])
AC_INIT([pgBackRest], [2.25dev])
AC_CONFIG_SRCDIR([version.h])
AC_CONFIG_AUX_DIR(build)

# Check compiler supports C99 standard
# ----------------------------------------------------------------------------------------------------------------------------------
AC_PROG_CC_C99

# Set warnings and optimizations based on build type (i.e. production or test)
# ----------------------------------------------------------------------------------------------------------------------------------
warning='-Wall -Wextra -Wno-clobbered -Wno-missing-field-initializers'

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
    AC_SUBST(CWARNING, "$warning -Wwrite-strings -Wconversion -Wformat=2 -Wformat-nonliteral -Wstrict-prototypes -Wpointer-arith -Wvla")
fi

# Include the build directory
# ----------------------------------------------------------------------------------------------------------------------------------
AC_SUBST(CINCLUDE, "-I.")

# Check required pq library
# ----------------------------------------------------------------------------------------------------------------------------------
AC_CHECK_LIB([pq], [PQconnectdb], [], [AC_MSG_ERROR([library 'pq' is required])])
AC_SUBST(CINCLUDE, "$CINCLUDE -I`pg_config --includedir`")

# Check required openssl libraries
# ----------------------------------------------------------------------------------------------------------------------------------
AC_CHECK_LIB([crypto], [EVP_get_digestbyname], [], [AC_MSG_ERROR([library 'crypto' is required])])
AC_CHECK_LIB([ssl], [SSL_new], [], [AC_MSG_ERROR([library 'ssl' is required])])

# Check required xml library
# ----------------------------------------------------------------------------------------------------------------------------------
AC_ARG_VAR(XML2_CONFIG, [path to xml2 config utility])dnl

if test -z "$XML2_CONFIG"; then
    XML2_CONFIG="pkg-config libxml-2.0"
fi

CPPFLAGS="$CPPFLAGS `$XML2_CONFIG --cflags`"

AC_CHECK_LIB([xml2], [xmlSaveToBuffer], [], [AC_MSG_ERROR([library 'xml2' is required])])
AC_CHECK_HEADER(libxml/parser.h, [], [AC_MSG_ERROR([header file <libxml/parser.h> is required])])
AC_SUBST(CLIBRARY, "$CLIBRARY `pkg-config libxml-2.0 --cflags`")

# Check required gz library
# ----------------------------------------------------------------------------------------------------------------------------------
AC_CHECK_LIB([z], [deflate], [], [AC_MSG_ERROR([library 'z' is required])])

# Check optional lz4 library
# ----------------------------------------------------------------------------------------------------------------------------------
AC_CHECK_LIB([lz4], [LZ4F_isError])

# Write output
# ----------------------------------------------------------------------------------------------------------------------------------
AC_CONFIG_HEADERS([build.auto.h])
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
