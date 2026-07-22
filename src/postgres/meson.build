####################################################################################################################################
# Generate PostgreSQL interfaces
####################################################################################################################################
# Generate interface.auto.c.inc, unless this is a distribution tarball that ships the file
if not dist
    interface_auto_c_inc = custom_target(
        'interface.auto.c.inc',
        output: 'interface.auto.c.inc',
        depend_files: [
            '../build/postgres/postgres.yaml',
        ],
        command: [
            build_code,
            'postgres',
            '@CURRENT_SOURCE_DIR@/../..',
        ],
    )
else
    # The shipped file is included by interface.c via the implicit source include path, so it is not a compiled source
    interface_auto_c_inc = []
endif
