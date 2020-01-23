# Initialize configuration
AC_PREREQ([2.69])
AC_INIT([pgBackRest], [2.23dev])
AC_CONFIG_SRCDIR([version.h])

# Check compiler
AC_PROG_CC_C99

# Wrap AC_PATH_PROGS to check if the value is already set before looking for programs. AC_PATH_PROGS preserves the value only if it
# looks like an absolute path.
AC_DEFUN([BRAC_PATH_PROGS],
[if test -z "$$1"; then
    AC_PATH_PROGS($@)
else
    # Report the value of $1 in configure's output
    AC_MSG_CHECKING([for $1])
    AC_MSG_RESULT([$$1])
fi
])

# Enable internal test code and assertions by undefining NDEBUG
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

# Set includes
AC_SUBST(CINCLUDE, "-I. -I../libc")

# Check required pq library
AC_CHECK_LIB([pq], [PQconnectdb], [], [AC_MSG_ERROR([library 'pq' is required])])
AC_SUBST(CINCLUDE, "$CINCLUDE -I`pg_config --includedir`")

# Check required openssl libraries
AC_CHECK_LIB([crypto], [EVP_get_digestbyname], [], [AC_MSG_ERROR([library 'crypto' is required])])
AC_CHECK_LIB([ssl], [SSL_new], [], [AC_MSG_ERROR([library 'ssl' is required])])

# Check required xml library
BRAC_PATH_PROGS(XML2_CONFIG, [xml2-config])
AC_ARG_VAR(XML2_CONFIG, [path to xml2-config utility])dnl

if test -n "$XML2_CONFIG"; then
    for ac_option in `$XML2_CONFIG --cflags`; do
        case $ac_option in
            -I*|-D*) CPPFLAGS="$CPPFLAGS $ac_option";;
        esac
    done
        for pgac_option in `$XML2_CONFIG --libs`; do
        case $ac_option in
            -L*) LDFLAGS="$LDFLAGS $ac_option";;
        esac
    done
fi

AC_CHECK_LIB([xml2], [xmlSaveToBuffer], [], [AC_MSG_ERROR([library 'xml2' is required])])
AC_CHECK_HEADER(libxml/parser.h, [], [AC_MSG_ERROR([header file <libxml/parser.h> is required])])
AC_SUBST(CLIBRARY, "$CLIBRARY `$XML2_CONFIG --cflags`")

# Check required gzip library
AC_CHECK_LIB([z], [deflate], [], [AC_MSG_ERROR([library 'z' is required])])

# Write output
AC_CONFIG_HEADERS([build.auto.h])
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
