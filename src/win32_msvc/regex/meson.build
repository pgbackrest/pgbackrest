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
regex_configuration = configuration_data()

regex_configuration.set('ssize_t', '__int64')
regex_configuration.set('VERSION', '2.7')
regex_configuration.set('HAVE_ALLOCA', 1)
regex_configuration.set('alloca', '_alloca')

if cc.has_header('alloca.h')
  regex_configuration.set('HAVE_ALLOCA_H', 1)
endif
if cc.has_function('clock_gettime')
  regex_configuration.set('HAVE_CLOCK_GETTIME', 1)
endif
if cc.has_function('isascii')
  regex_configuration.set('HAVE_ISASCII', 1)
endif
if cc.has_header('dlfcn.h')
  regex_configuration.set('HAVE_DLFCN_H', 1)
endif
if cc.has_header('fcntl.h')
  regex_configuration.set('HAVE_FCNTL_H', 1)
endif
if cc.has_header('inttypes.h')
  regex_configuration.set('HAVE_INTTYPES_H', 1)
endif

if cc.has_header('langinfo.h')
  regex_configuration.set('HAVE_LANGINFO_H', 1)
endif
if cc.has_header('libintl.h')
  regex_configuration.set('HAVE_LIBINTL_H', 1)
endif
if cc.has_header('limits.h')
  regex_configuration.set('HAVE_LIMITS_H', 1)
endif
if cc.has_header('locale.h')
  regex_configuration.set('HAVE_LOCALE_H', 1)
endif
if cc.has_header('memory.h')
  regex_configuration.set('HAVE_MEMORY_H', 1)
endif

if cc.has_function('malloc')
  regex_configuration.set('HAVE_MALLOC', 1)
endif
if cc.has_function('realloc')
  regex_configuration.set('HAVE_REALLOC', 1)
endif
if cc.has_function('mbrtowc') and cc.has_type('mbstate_t')
  regex_configuration.set('HAVE_MBRTOWC', 1)
endif
if cc.has_function('memmove')
  regex_configuration.set('HAVE_MEMMOVE', 1)
endif

if cc.has_function('memset')
  regex_configuration.set('HAVE_MEMSET', 1)
endif
if cc.has_function('nl_langinfo')
  regex_configuration.set('HAVE_NL_LANGINFO', 1)
endif

if cc.has_function('regcomp')
  regex_configuration.set('HAVE_REGCOMP', 1)
endif
if cc.has_function('re_comp')
  regex_configuration.set('HAVE_RE_COMP', 1)
endif
if cc.has_function('setlocale')
  regex_configuration.set('HAVE_SETLOCALE', 1)
endif


if cc.has_header('stdbool.h')
  regex_configuration.set('HAVE_STDBOOL_H', 1)
endif
if cc.has_header('stdint.h')
  regex_configuration.set('HAVE_STDINT_H', 1)
endif
if cc.has_header('stdlib.h')
  regex_configuration.set('HAVE_STDLIB_H', 1)
endif

if cc.has_function('stpcpy')
  regex_configuration.set('HAVE_STPCPY', 1)
endif
if cc.has_function('strcasecmp')
  regex_configuration.set('HAVE_STRCASECMP', 1)
else
  regex_configuration.set('HAVE_STRCASECMP', 1)
  regex_configuration.set('strcasecmp', '_stricmp')
endif
if cc.has_function('strchr')
  regex_configuration.set('HAVE_STRCHR', 1)
endif

if cc.has_header('strings.h')
  regex_configuration.set('HAVE_STRINGS_H', 1)
endif
if cc.has_header('string.h')
  regex_configuration.set('HAVE_STRING_H', 1)
endif
if cc.has_header('sys/stat.h')
  regex_configuration.set('HAVE_SYS_STAT_H', 1)
endif
if cc.has_header('sys/types.h')
  regex_configuration.set('HAVE_SYS_TYPES_H', 1)
endif
if cc.has_header('unistd.h')
  regex_configuration.set('HAVE_UNISTD_H', 1)
endif
if cc.has_header('wchar.h')
  regex_configuration.set('HAVE_WCHAR_H', 1)
endif
if cc.has_header('wctype.h')
  regex_configuration.set('HAVE_WCTYPE_H', 1)
endif

if cc.has_type('_Bool')
  regex_configuration.set('HAVE__BOOL', 1)
endif

regex_configuration.set('PACKAGE', '1.0')
regex_configuration.set('PACKAGE_BUGREPORT', '1.0')
regex_configuration.set('PACKAGE_NAME', '1.0')
regex_configuration.set('PACKAGE_STRING', '1.0')
regex_configuration.set('PACKAGE_TARNAME', '1.0')
regex_configuration.set('PACKAGE_VERSION', '1.0')
regex_configuration.set('_CRT_SECURE_NO_WARNINGS', true)

regex_configuration.set('STACK_DIRECTION', 0)
regex_configuration.set('STDC_HEADERS', 1)

# regex_configuration.set('inline', '__inline')
regex_configuration.set('BUILD_REGEX_DLL', 1)

configure_file(output: 'config.h', configuration: regex_configuration)

add_project_arguments('-DHAVE_CONFIG_H', language: 'c')

regex_sources = [
    'regex.c',
]

dep_regex = static_library(
    'regex',
    regex_sources,
)


