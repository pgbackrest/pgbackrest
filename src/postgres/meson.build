####################################################################################################################################
# Generate PostgreSQL interfaces
####################################################################################################################################
interface_auto_c_inc = custom_target(
    'interface.auto.c.inc',
    output : 'interface.auto.c.inc',
    depend_files: [
        '../build/postgres/postgres.yaml',
    ],
    command : [
        build_code,
        'postgres',
        '@CURRENT_SOURCE_DIR@/../..',
    ],
)
