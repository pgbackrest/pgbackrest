####################################################################################################################################
# pgBackRest Project
####################################################################################################################################
project(
    'pgbackrest',
    'c',
    default_options: [
        # Core options
        'autofeatures=disabled',
        'debug=false',
        'buildtype=plain',

        # Compiler options
        'c_std=c99',
    ],
)

####################################################################################################################################
# OS-specific settings
####################################################################################################################################
if build_machine.system() == 'linux'
    add_global_arguments('-D_POSIX_C_SOURCE=200809L', language : 'c')
elif build_machine.system() == 'darwin'
    add_global_arguments('-D_DARWIN_C_SOURCE', language : 'c')
endif

####################################################################################################################################
# Enable as many warnings as possible to catch potential errors
####################################################################################################################################
warning_enable = [
    # Warn about basic issues
    'all',

    # Warn for implicit conversions that may alter a value
    'conversion',

    # Warn about duplicated conditions in an if-else-if chain
    'duplicated-cond',

    # Warn when an if-else has identical branches
    'duplicated-branches',

    # Warn about more advance issues
    'extra',

    # Warn if the format string is not a string literal and cannot be checked
    'format-nonliteral',

    # Enable -Wformat plus additional format checks
    'format=2',

    # Warn if the format string requires an unsigned argument and the argument is signed and vice versa
    'format-signedness',

    # Warn about anything that depends on the “size of” a function type or of void
    'pointer-arith',

    # Warn if a function is declared or defined without specifying the argument types
    'strict-prototypes',

    # Warn if a variable-length array is used
    'vla',

    # Give string constants the type const char[length] so that copying the address of one into a non-const char * pointer produces
    # a warning
    'write-strings',
]

foreach warning : warning_enable
    if meson.get_compiler('c').has_argument('-W' + warning)
        add_project_arguments('-W' + warning, language: 'c')
    endif
endforeach

####################################################################################################################################
# Disable various unhelpful warnings by adding -Wno-warning switches. GCC won't complain about unrecognized -Wno-warning switches,
# so we have to test for the positive form, and if that works, add the negative form.
####################################################################################################################################
warning_disable = [
    # Warn for variables that might be changed by longjmp or vfork. Disable because of constant false positives/negatives.
    'clobbered',

    # Warn if a structure’s initializer has some fields missing. Disable so we can initialize with {0}.
    'missing-field-initializers',

    # Warn when a switch case falls through. Disable because this an useful aspect of switches and tests should catch problems.
    'implicit-fallthrough',
]

foreach warning : warning_disable
    if meson.get_compiler('c').has_argument('-W' + warning)
        add_project_arguments('-Wno-' + warning, language: 'c')
    endif
endforeach

####################################################################################################################################
# Test options
####################################################################################################################################
if get_option('test') == true
    ################################################################################################################################
    # Make warnings errors and stop after the first error. Subsequent errors are often caused by the first error.
    ################################################################################################################################
    error_enable = [
        # Make all warnings into errors
        'error',

        # Causes the compiler to abort compilation on the first error
        'fatal-errors',
    ]

    foreach error : error_enable
        if meson.get_compiler('c').has_argument('-W' + error)
            add_project_arguments('-W' + error, language: 'c')
        endif
    endforeach

####################################################################################################################################
# Release options
####################################################################################################################################
elif
endif

subdir('src')
