####################################################################################################################################
# pgBackRest Project
####################################################################################################################################
project(
    'pgbackrest',
    ['c'],
    version: '2.40dev',
    license: 'MIT',
    meson_version: '>=0.53.2',
    default_options: [
        # Core options
        'buildtype=release',
        'warning_level=2',

        # Base options
        'b_ndebug=if-release',

        # Compiler options
        'c_std=c99',
    ],
)

# Selected C compiler
cc = meson.get_compiler('c')

####################################################################################################################################
# Error on release builds since we do not want anyone using meson for production yet
####################################################################################################################################
if get_option('buildtype') != 'debug'
#    error('meson is currently not supported for release builds')
endif

####################################################################################################################################
# OS-specific settings
####################################################################################################################################
if host_machine.system() == 'linux'
    add_global_arguments('-D_POSIX_C_SOURCE=200809L', language : 'c')
elif host_machine.system() == 'darwin'
    add_global_arguments('-D_DARWIN_C_SOURCE', language : 'c')
endif

####################################################################################################################################
# Enable/disable warnings
####################################################################################################################################
# Enable as many additional warnings as possible to catch potential errors
warning_enable = [
    # Warn for implicit conversions that may alter a value
    '-Wconversion',

    # Warn about duplicated conditions in an if-else-if chain
    '-Wduplicated-cond',

    # Warn when an if-else has identical branches
    '-Wduplicated-branches',

    # Warn if the format string is not a string literal and cannot be checked
    '-Wformat-nonliteral',

    # Enable -Wformat plus additional format checks
    '-Wformat=2',

    # Warn if the format string requires an unsigned argument and the argument is signed and vice versa
    '-Wformat-signedness',

    # Warn about anything that depends on the “size of” a function type or of void
    '-Wpointer-arith',

    # Warn if a function is declared or defined without specifying the argument types
    '-Wstrict-prototypes',

    # Warn if a variable-length array is used
    '-Wvla',

    # Give string constants the type const char[length] so that copying the address of one into a non-const char * pointer produces
    # a warning
    '-Wwrite-strings',
]

# Disable various unhelpful warnings
warning_disable = [
    # Warn for variables that might be changed by longjmp or vfork. Disable because of constant false positives/negatives.
    '-Wno-clobbered',

    # Warn if a structure’s initializer has some fields missing. Disable so we can initialize with {0}.
    '-Wno-missing-field-initializers',

    # Warn when a switch case falls through. Disable because this an useful aspect of switches and tests should catch problems.
    '-Wno-implicit-fallthrough',

    # [msvc]:[C4114]: Warn if the same type qualifier used more than once
    '/wd4114',

    # [msvc]:[C4116]: Warn for unnamed type definition in parentheses, triggers with ALIGN_OF macro
    '/wd4116',

    # [msvc]:[C5105]: Warn for macro expansion producing 'defined' has undefined behavior. Disable as it triggers
    # from winbase.h
    '/wd5105',
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

if cc.get_id() != 'msvc'
# Find required bz2 library
lib_bz2 = cc.find_library('bz2')

# Find optional lz4 library
lib_lz4 = dependency('liblz4', required: false)

if lib_lz4.found()
    configuration.set('HAVE_LIBLZ4', true, description: 'Is liblz4 present?')
endif

# Find required openssl library
lib_openssl = dependency('openssl')

# Find required pq library
lib_pq = dependency('libpq')

# Find required xml library
lib_xml = dependency('libxml-2.0')

# Find required yaml library (only used for build)
lib_yaml = dependency('yaml-0.1')

# Find required gz library
lib_z = dependency('zlib')

# Find optional zstd library
lib_zstd = dependency('libzstd', version: '>=1.0', required: false)

if lib_zstd.found()
    configuration.set('HAVE_LIBZST', true, description: 'Is libzstd present?')
endif
else

# This section assumes default install path for CMAKE (C:/Program Files (x86))

# Due to a meson bug that throws ValueError: path is on mount 'C:', start on mount 'D:'
# The pgbackrest HAS to be on the same drive as the dependencies

dep_inc_dir = []

# Find required yaml library (only used for build)
lib_yaml = dependency('yaml')

# Find required regex library (not available by default in msvc)
lib_regex = dependency('RXSpencer')

# Find required bz2 library
dep_inc_dir += include_directories('C:/Program Files (x86)/bzip2/include')
lib_bz2 = cc.find_library('bz2', dirs: ['C:/Program Files (x86)/bzip2/lib'],)

# Find required openssl library
# lib_openssl = dependency('openssl')

# Find required pq library
# lib_pq = dependency('libpq')

# Find required xml library
dep_inc_dir += include_directories('C:/Program Files (x86)/libxml2/include/libxml2')
lib_xml = cc.find_library('xml2s', dirs: ['C:/Program Files (x86)/libxml2/lib'],)

# Find required gz library
# lib_z = dependency('zlib')

# Find optional zstd library
# lib_zstd = dependency('libzstd', version: '>=1.0', required: false)

if not lib_yaml.found()
    error('lib yaml not found')
endif

endif

# Check if the C compiler supports _Static_assert()
if cc.compiles('''int main(int arg, char **argv) {({ _Static_assert(1, "foo");});} ''')
  configuration.set('HAVE_STATIC_ASSERT', true, description: 'Does the compiler provide _Static_assert()?')
endif

# Enable debug code
if get_option('debug')
    if cc.get_id() == 'msvc'
        configuration.set('DEBUG', '#define DEBUG 1', description: 'Enable debug code')
    else
        configuration.set('DEBUG', true, description: 'Enable debug code')
    endif
endif

# Set configuration path
configuration.set_quoted('CFGOPTDEF_CONFIG_PATH', get_option('configdir'), description: 'Configuration path')

# Set FN_NO_RETURN macro
if cc.has_function_attribute('noreturn')
    configuration.set('FN_NO_RETURN', '__attribute__((__noreturn__))', description: 'Indicate that a function does not return')
else
    configuration.set('FN_NO_RETURN', '__declspec(noreturn)', description: 'Indicate that a function does not return')
endif

# if cc.get_id() == 'msvc'
if host_machine.system() == 'windows'
    if cc.get_id() != 'msvc'
        # lib regex for mingW
        lib_regex = dependency('rxspencer')
        dep_inc_dir = []
    endif
    # Include MSVC compatibility header
    configuration.set('INCLUDE_MSVC_COMPAT', '#include "msvcCompat.h"', description: 'MSVC compatibility header')
    # Enable static import for lib yaml
    if lib_yaml.found()
        configuration.set('ENABLE_YAML', '#define ENABLE_YAML', description: 'Enable lib_yaml static import')
    else
        configuration.set('ENABLE_YAML', '')
    endif
    # Enable static import for lib regex
    if lib_regex.found()
        configuration.set('ENABLE_REGEX', '#define ENABLE_REGEX', description: 'Enable lib_regex static import')
    else
        configuration.set('ENABLE_REGEX', '')
    endif
    # Enable static import for lib xml
    if lib_xml.found()
        configuration.set('ENABLE_XML', '#define ENABLE_XML', description: 'Enable libxml2 static import')
    else
        configuration.set('ENABLE_XML', '')
    endif
else
    configuration.set('INCLUDE_MSVC_COMPAT', '', description: '')
endif

####################################################################################################################################
# Include subdirs
####################################################################################################################################
subdir('src')
subdir('test/src')
