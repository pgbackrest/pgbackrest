# Initialize configuration
# ----------------------------------------------------------------------------------------------------------------------------------
AC_PREREQ([2.69])
AC_INIT([pgBackRest], [2.35])
AC_CONFIG_SRCDIR([version.h])
AC_CONFIG_AUX_DIR(build)

# Don't add any CFLAGS by default (otherwise -g -O2 will be added)
# ----------------------------------------------------------------------------------------------------------------------------------
: ${CFLAGS=""}

# Check compiler supports C99 standard
# ----------------------------------------------------------------------------------------------------------------------------------
AC_PROG_CC_C99

# Build C standard based on the host type.  C99 is required and other flags are added depending on the host.
# ----------------------------------------------------------------------------------------------------------------------------------
AC_CANONICAL_HOST
AC_SUBST(CFLAGS, "${CFLAGS} -std=c99")

case $host_os in
    darwin*)
        AC_SUBST(CPPFLAGS, "${CPPFLAGS} -D_DARWIN_C_SOURCE")
        ;;

    linux*)
        AC_SUBST(CPPFLAGS, "${CPPFLAGS} -D_POSIX_C_SOURCE=200809L")
        ;;
esac

# Check if the C compiler supports _Static_assert()
#
# Test the syntax ({_Static_assert(...)}) because gcc-style compound expressions are needed to wrap it into macros.
# ----------------------------------------------------------------------------------------------------------------------------------
AC_LINK_IFELSE([AC_LANG_PROGRAM([], [({_Static_assert(1, "foo");})])], [AC_DEFINE(HAVE_STATIC_ASSERT)])

# Check if the C compiler supports __builtin_types_compatible_p()
# ----------------------------------------------------------------------------------------------------------------------------------
AC_LINK_IFELSE(
    [AC_LANG_PROGRAM([], [[int x; static int y[__builtin_types_compatible_p(__typeof__(x), int)];]])],
    [AC_DEFINE(HAVE_BUILTIN_TYPES_COMPATIBLE_P)])

# Set warnings and optimizations based on build type (i.e. production or test)
# ----------------------------------------------------------------------------------------------------------------------------------
AC_SUBST(CFLAGS, "${CFLAGS} -Wall -Wextra -Wno-missing-field-initializers -Wno-implicit-fallthrough")

# -Wno-clobbered is not supported on all compilers
AX_CHECK_COMPILE_FLAG([-Wno-clobbered], [AC_SUBST(CFLAGS, "${CFLAGS} -Wno-clobbered")], [], [-Werror])

AC_ARG_ENABLE(
    test, [AS_HELP_STRING([--enable-test], [enable internal test code and assertions for debugging])])

if test "$enable_test" != yes
then
    AC_DEFINE(NDEBUG)

    # Enable optimizations if not testing and they have not been disabled
    AC_ARG_ENABLE(optimize, [AS_HELP_STRING([--disable-optimize], [disable compiler optimizations])])

    if test "$enable_optimize" != no
    then
        AC_SUBST(CFLAGS, "${CFLAGS} -O2")
        AC_SUBST(CFLAGS_PAGE_CHECKSUM, "-funroll-loops -ftree-vectorize")
    fi
else
    # Check for optional warnings (note that these must be checked before the additional warnings below are added)
    AX_CHECK_COMPILE_FLAG([-Wformat-signedness], [AC_SUBST(CFLAGS, "${CFLAGS} -Wformat-signedness")], [], [-Werror])
    AX_CHECK_COMPILE_FLAG([-Wduplicated-branches], [AC_SUBST(CFLAGS, "${CFLAGS} -Wduplicated-branches")], [], [-Werror])
    AX_CHECK_COMPILE_FLAG([-Wduplicated-cond], [AC_SUBST(CFLAGS, "${CFLAGS} -Wduplicated-cond")], [], [-Werror])

    # Add additional warnings
    AC_SUBST(CFLAGS, "${CFLAGS} -Wwrite-strings -Wconversion -Wformat=2 -Wformat-nonliteral -Wstrict-prototypes -Wpointer-arith")
    AC_SUBST(CFLAGS, "${CFLAGS} -Wvla")
fi

# Include the build directory
# ----------------------------------------------------------------------------------------------------------------------------------
AC_SUBST(CPPFLAGS, "${CPPFLAGS} -I.")

# Check required pq library
# ----------------------------------------------------------------------------------------------------------------------------------
AC_ARG_VAR(PG_CONFIG, [path to pg_config utility])dnl

if test -z "$PG_CONFIG"; then
    PG_CONFIG="pg_config"
fi

AC_CHECK_PROG(PG_CONFIG_EXISTS, [${PG_CONFIG?}], ["yes"], ["no"])

if test ${PG_CONFIG_EXISTS?} == yes
then
    AC_SUBST(CPPFLAGS, "${CPPFLAGS} -I`${PG_CONFIG?} --includedir`")
    AC_SUBST(LDFLAGS, "${LDFLAGS} -L`${PG_CONFIG?} --libdir`")
fi

AC_CHECK_LIB([pq], [PQconnectdb], [], [AC_MSG_ERROR([library 'pq' is required])])
AC_CHECK_HEADER(libpq-fe.h, [], [AC_MSG_ERROR([header file <libpq-fe.h> is required])])

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

AC_CHECK_PROG(XML2_CONFIG_EXISTS, [${XML2_CONFIG?}], ["yes"], ["no"])

if test ${XML2_CONFIG_EXISTS?} == yes
then
    AC_SUBST(CPPFLAGS, "$CPPFLAGS `${XML2_CONFIG?} --cflags`")
fi

AC_CHECK_LIB([xml2], [xmlSaveToBuffer], [], [AC_MSG_ERROR([library 'xml2' is required])])
AC_CHECK_HEADER(libxml/parser.h, [], [AC_MSG_ERROR([header file <libxml/parser.h> is required])])

# Check required yaml library (only required for build)
# ----------------------------------------------------------------------------------------------------------------------------------
AC_CHECK_LIB(
    [yaml], [yaml_parser_initialize], [AC_SUBST(LIBS_BUILD, "${LIBS_BUILD} -lyaml")], [AC_MSG_ERROR([library 'yaml' is required])])
AC_CHECK_HEADER(zlib.h, [], [AC_MSG_ERROR([header file <yaml.h> is required])])

# Check required gz library
# ----------------------------------------------------------------------------------------------------------------------------------
AC_CHECK_LIB([z], [deflate], [], [AC_MSG_ERROR([library 'z' is required])])
AC_CHECK_HEADER(zlib.h, [], [AC_MSG_ERROR([header file <zlib.h> is required])])

# Check required bzip2 library
# ----------------------------------------------------------------------------------------------------------------------------------
AC_CHECK_LIB([bz2], [BZ2_bzCompress], [], [AC_MSG_ERROR([library 'bz2' is required])])
AC_CHECK_HEADER(bzlib.h, [], [AC_MSG_ERROR([header file <bzlib.h> is required])])

# Check optional lz4 library
# ----------------------------------------------------------------------------------------------------------------------------------
AC_CHECK_LIB(
    [lz4], [LZ4F_isError],
    [AC_CHECK_HEADER(lz4frame.h, [AC_DEFINE(HAVE_LIBLZ4) AC_SUBST(LIBS, "${LIBS} -llz4")],
        [AC_MSG_ERROR([header file <lz4frame.h> is required])])])

# Check optional zst library. Ignore any versions below 1.0.
# ----------------------------------------------------------------------------------------------------------------------------------
AC_CHECK_LIB(
    [zstd], [ZSTD_isError],
    [AC_CHECK_HEADER(zstd.h,
        [AC_LINK_IFELSE(
            [AC_LANG_PROGRAM(
                [#include <zstd.h>],
                [#if ZSTD_VERSION_MAJOR < 1
                    #error "ZSTD_VERSION_MAJOR must be >= 1"
                    #endif])],
            [AC_DEFINE(HAVE_LIBZST) AC_SUBST(LIBS, "${LIBS} -lzstd")])],
        [AC_MSG_ERROR([header file <zstd.h> is required])])])

# Set configuration path
# ----------------------------------------------------------------------------------------------------------------------------------
AC_ARG_WITH(
    [configdir], [AS_HELP_STRING([--with-configdir=DIR], [default configuration path])],
    [AC_DEFINE_UNQUOTED([CFGOPTDEF_CONFIG_PATH], ["${withval}"])],
    [AC_DEFINE_UNQUOTED([CFGOPTDEF_CONFIG_PATH], ["/etc/" PROJECT_BIN])])

# Write output
# ----------------------------------------------------------------------------------------------------------------------------------
AC_CONFIG_HEADERS([build.auto.h])
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
