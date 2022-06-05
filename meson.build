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

# Check if the C compiler supports _Static_assert()
if cc.compiles('''int main(int arg, char **argv) {({ _Static_assert(1, "foo");});} ''')
  configuration.set('HAVE_STATIC_ASSERT', true, description: 'Does the compiler provide _Static_assert()?')
endif

# Enable debug code
if get_option('debug')
    configuration.set('DEBUG', true, description: 'Enable debug code')
endif

# Set configuration path
configuration.set_quoted('CFGOPTDEF_CONFIG_PATH', get_option('configdir'), description: 'Configuration path')

####################################################################################################################################
# Include src
####################################################################################################################################
subdir('src')