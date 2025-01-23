####################################################################################################################################
# pgBackRest Project
####################################################################################################################################
project(
    'pgbackrest',
    ['c'],
    version: '2.55.0dev',
    license: 'MIT',
    meson_version: '>=0.47',
    default_options: [
        # Core options
        'buildtype=release',
        'warning_level=2',

        # Base options
        'b_ndebug=if-release',

        # Perform unity builds in a single file
        'unity_size=100000',

        # Compiler options
        'c_std=c99',
    ],
)

# Selected C compiler
cc = meson.get_compiler('c')

####################################################################################################################################
# OS-specific settings
####################################################################################################################################
if host_machine.system() == 'linux'
    add_global_arguments('-D_POSIX_C_SOURCE=200809L', language: 'c')
elif host_machine.system() == 'darwin'
    add_global_arguments('-D_DARWIN_C_SOURCE', language: 'c')
endif

####################################################################################################################################
# Enable/disable warnings
####################################################################################################################################
# Enable as many additional warnings as possible to catch potential errors
warning_enable = [
    # Warn whenever a pointer is cast so as to remove a type qualifier from the target type
    '-Wcast-qual',

    # Warn for implicit conversions that may alter a value
    '-Wconversion',

    # Warn about duplicated conditions in an if-else-if chain
    '-Wduplicated-cond',

    # Warn when an if-else has identical branches
    '-Wduplicated-branches',

    # Warn if floating-point values are used in equality comparisons
    '--Wfloat-equal',

    # Warn if the format string is not a string literal and cannot be checked
    '-Wformat-nonliteral',

    # Enable -Wformat plus additional format checks
    '-Wformat=2',

    # Warn if the format string requires an unsigned argument and the argument is signed and vice versa
    '-Wformat-signedness',

    # Warn about uninitialized variables which are initialized with themselves
    '-Winit-self',

    # Warn if a global function is defined without a previous prototype declaration
    '-Wmissing-prototypes',

    # Warn if a global variable is defined without a previous declaration
    '-Wmissing-variable-declarations',

    # Warn about anything that depends on the “size of” a function type or of void
    '-Wpointer-arith',

    # Warn if anything is declared more than once in the same scope, even when the extra declaration is valid and changes nothing
    '-Wredundant-decls',

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
]

add_project_arguments(cc.get_supported_arguments(warning_enable, warning_disable), language: 'c')

####################################################################################################################################
# Enable additional optimizations for release builds. We would prefer to use `get_option('optimization') in ['2', '3']` when our
# minimum version is high enough to allow it.
####################################################################################################################################
if get_option('buildtype') == 'release'
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
# Generate -fmacro-prefix-map so the relative path to the source directory is not included in the __FILE__ macro. This provides
# reproducible builds and minimizes the file path in debug messages, just like an in-tree make build. For test source, prefix with
# test/ in case there are any module name collisions.
####################################################################################################################################
python = import('python').find_installation()

file_prefix = run_command(
    [
        python,
        '-c',
        'import sys, os; print(os.path.relpath(sys.argv[1], sys.argv[2]))',
        meson.current_source_dir(),
        meson.current_build_dir(),
    ],
    check: true,
).stdout().strip()

add_project_arguments(cc.get_supported_arguments('-fmacro-prefix-map=@0@/src/=' . format(file_prefix)), language: 'c')
add_project_arguments(cc.get_supported_arguments('-fmacro-prefix-map=@0@/test/src/=test/' . format(file_prefix)), language: 'c')

####################################################################################################################################
# Build configuration
####################################################################################################################################
configuration = configuration_data()

# Ensure the C compiler supports __builtin_clzl()
if not cc.links('''int main(int arg, char **argv) {__builtin_clzl(1);}''')
    error('compiler must support __builtin_clzl()')
endif

# Ensure the C compiler supports __builtin_bswap64()
if not cc.links('''int main(int arg, char **argv) {__builtin_bswap64(1);}''')
    error('compiler must support __builtin_bswap64()')
endif

# Find optional backtrace library
lib_backtrace = cc.find_library('backtrace', required: false)

if lib_backtrace.found()
    configuration.set('HAVE_LIBBACKTRACE', true, description: 'Is libbacktrace present?')
endif

# Find required bz2 library
lib_bz2 = cc.find_library('bz2')

# Find required lz4 library
lib_lz4 = dependency('liblz4')

# Find required openssl library
lib_openssl = dependency('openssl', version : '>=1.1.1')

# Find required pq library
lib_pq = dependency('libpq')

# Find required xml library
lib_xml = dependency('libxml-2.0')

# Find required yaml library (only used for build)
lib_yaml = dependency('yaml-0.1')

# Find required gz library
lib_z = dependency('zlib')

configuration.set('ZLIB_CONST', true, description: 'Require zlib const input buffer')

# Find optional libssh2 library
lib_ssh2 = dependency('libssh2', required: get_option('libssh2'))

if lib_ssh2.found()
    configuration.set('HAVE_LIBSSH2', true, description: 'Is libssh2 present?')
endif

# Find optional zstd library
lib_zstd = dependency('libzstd', version: '>=1.0', required: get_option('libzstd'))

if lib_zstd.found()
    configuration.set('HAVE_LIBZST', true, description: 'Is libzstd present?')
endif

# Check if the C compiler supports _Static_assert()
if cc.compiles('''int main(int arg, char **argv) {({ _Static_assert(1, "foo");});} ''')
  configuration.set('HAVE_STATIC_ASSERT', true, description: 'Does the compiler provide _Static_assert()?')
endif

# Enable debug code. We would prefer to use `get_option('debug')` when our minimum version is high enough to allow it.
if get_option('buildtype') == 'debug' or get_option('buildtype') == 'debugoptimized'
    configuration.set('DEBUG', true, description: 'Enable debug code')
endif

# Set configuration path
configuration.set_quoted('CFGOPTDEF_CONFIG_PATH', get_option('configdir'), description: 'Configuration path')

# Set FN_NO_RETURN macro
configuration.set('FN_NO_RETURN', '__attribute__((__noreturn__))', description: 'Indicate that a function does not return')

# Set FN_INLINE_ALWAYS macro
configuration.set(
    'FN_INLINE_ALWAYS', '__attribute__((always_inline)) static inline',
    description: 'Indicate that a function should always be inlined'
)

# Set FN_PRINTF macro
configuration.set(
    'FN_PRINTF(fmt, args)', '__attribute__((format(printf, fmt, args)))',
    description: 'Indicate that a function is formatted like printf (and provide format and args position)'
)

# Set FN_STRFTIME macro
configuration.set(
    'FN_STRFTIME(fmt)', '__attribute__((format(strftime, fmt, 0)))',
    description: 'Indicate that a function is formatted like strftime (and provide format position)'
)

####################################################################################################################################
# Include subdirs
####################################################################################################################################
subdir('src')
subdir('doc/src')
subdir('test/src')
