####################################################################################################################################
# Regex dependency
####################################################################################################################################

# Selected C compiler
cc = meson.get_compiler('c')

####################################################################################################################################
# Enable/disable warnings
####################################################################################################################################
# Enable as many additional warnings as possible to catch potential errors
warning_enable = []

# Disable various unhelpful warnings
warning_disable = [
    # [msvc]:[C4114]: Warn if the same type qualifier used more than once
    '/wd4114',

    # [msvc]:[C4116]: Warn for unnamed type definition in parentheses, triggers with ALIGN_OF macro
    '/wd4116',
]

add_project_arguments(cc.get_supported_arguments(warning_enable, warning_disable), language: 'c')

####################################################################################################################################
# Enable additional optimizations if the level is high enough
####################################################################################################################################
if get_option('optimization') in ['2', '3']
    optimization_enable = [
        # Unroll loops whose number of iterations can be determined at compile time or upon entry to the loop
        '-funroll-loops',

        # Perform loop vectorization on trees
        '-ftree-vectorize',
    ]

    add_project_arguments(cc.get_supported_arguments(optimization_enable), language: 'c')
endif

####################################################################################################################################
# Stop after the first error when error on warn enabled. Subsequent errors are often caused by the first error.
####################################################################################################################################
if get_option('fatal-errors')
    add_project_arguments(cc.get_supported_arguments('-Wfatal-errors'), language: 'c')
endif

####################################################################################################################################
# Build configuration
####################################################################################################################################
configuration = configuration_data()

configuration.set('ssize_t', '__int64')
configuration.set('VERSION', '2.7')
configuration.set('HAVE_ALLOCA', 1)
configuration.set('alloca', '_alloca')

if cc.has_header('alloca.h')
  configuration.set('HAVE_ALLOCA_H', 1)
endif
if cc.has_function('clock_gettime')
  configuration.set('HAVE_CLOCK_GETTIME', 1)
endif
if cc.has_function('isascii')
  configuration.set('HAVE_ISASCII', 1)
endif
if cc.has_header('dlfcn.h')
  configuration.set('HAVE_DLFCN_H', 1)
endif
if cc.has_header('fcntl.h')
  configuration.set('HAVE_FCNTL_H', 1)
endif
if cc.has_header('inttypes.h')
  configuration.set('HAVE_INTTYPES_H', 1)
endif

if cc.has_header('langinfo.h')
  configuration.set('HAVE_LANGINFO_H', 1)
endif
if cc.has_header('libintl.h')
  configuration.set('HAVE_LIBINTL_H', 1)
endif
if cc.has_header('limits.h')
  configuration.set('HAVE_LIMITS_H', 1)
endif
if cc.has_header('locale.h')
  configuration.set('HAVE_LOCALE_H', 1)
endif
if cc.has_header('memory.h')
  configuration.set('HAVE_MEMORY_H', 1)
endif

if cc.has_function('malloc')
  configuration.set('HAVE_MALLOC', 1)
endif
if cc.has_function('realloc')
  configuration.set('HAVE_REALLOC', 1)
endif
if cc.has_function('mbrtowc') and cc.has_type('mbstate_t')
  configuration.set('HAVE_MBRTOWC', 1)
endif
if cc.has_function('memmove')
  configuration.set('HAVE_MEMMOVE', 1)
endif

if cc.has_function('memset')
  configuration.set('HAVE_MEMSET', 1)
endif
if cc.has_function('nl_langinfo')
  configuration.set('HAVE_NL_LANGINFO', 1)
endif

if cc.has_function('regcomp')
  configuration.set('HAVE_REGCOMP', 1)
endif
if cc.has_function('re_comp')
  configuration.set('HAVE_RE_COMP', 1)
endif
if cc.has_function('setlocale')
  configuration.set('HAVE_SETLOCALE', 1)
endif


if cc.has_header('stdbool.h')
  configuration.set('HAVE_STDBOOL_H', 1)
endif
if cc.has_header('stdint.h')
  configuration.set('HAVE_STDINT_H', 1)
endif
if cc.has_header('stdlib.h')
  configuration.set('HAVE_STDLIB_H', 1)
endif

if cc.has_function('stpcpy')
  configuration.set('HAVE_STPCPY', 1)
endif
if cc.has_function('strcasecmp')
  configuration.set('HAVE_STRCASECMP', 1)
else
  configuration.set('HAVE_STRCASECMP', 1)
  configuration.set('strcasecmp', '_stricmp')
endif
if cc.has_function('strchr')
  configuration.set('HAVE_STRCHR', 1)
endif

if cc.has_header('strings.h')
  configuration.set('HAVE_STRINGS_H', 1)
endif
if cc.has_header('string.h')
  configuration.set('HAVE_STRING_H', 1)
endif
if cc.has_header('sys/stat.h')
  configuration.set('HAVE_SYS_STAT_H', 1)
endif
if cc.has_header('sys/types.h')
  configuration.set('HAVE_SYS_TYPES_H', 1)
endif
if cc.has_header('unistd.h')
  configuration.set('HAVE_UNISTD_H', 1)
endif
if cc.has_header('wchar.h')
  configuration.set('HAVE_WCHAR_H', 1)
endif
if cc.has_header('wctype.h')
  configuration.set('HAVE_WCTYPE_H', 1)
endif

if cc.has_type('_Bool')
  configuration.set('HAVE__BOOL', 1)
endif

configuration.set('PACKAGE', '1.0')
configuration.set('PACKAGE_BUGREPORT', '1.0')
configuration.set('PACKAGE_NAME', '1.0')
configuration.set('PACKAGE_STRING', '1.0')
configuration.set('PACKAGE_TARNAME', '1.0')
configuration.set('PACKAGE_VERSION', '1.0')
configuration.set('_CRT_SECURE_NO_WARNINGS', true)

configuration.set('STACK_DIRECTION', 0)
configuration.set('STDC_HEADERS', 1)

# configuration.set('inline', '__inline')
configuration.set('BUILD_REGEX_DLL', 1)

configure_file(output: 'config.h', configuration: configuration)

add_project_arguments('-DHAVE_CONFIG_H', language: 'c')

regex_sources = [
    'regex.c',
]

dep_regex = static_library(
    'regex',
    regex_sources,
)


