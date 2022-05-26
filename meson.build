project(
    'pgbackrest',
    'c',
    default_options: [
        # Core options
        'autofeatures=disabled',
        'debug=false',
        'buildtype=release',
        'optimization=2',

        # Compiler options
        'c_std=c99',
    ],
)

if build_machine.system() == 'linux'
    add_global_arguments('-D_POSIX_C_SOURCE=200809L', language : 'c')
endif

subdir('src')
