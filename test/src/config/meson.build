####################################################################################################################################
# Generate config
####################################################################################################################################
test_parse_auto_c_inc = custom_target(
    'parse.auto.c.inc',
    output : 'parse.auto.c.inc',
    depend_files: [
        '../build/config/config.yaml',
    ],
    command : [
        build_code,
        'config',
        '@CURRENT_SOURCE_DIR@/../..',
        '@BUILD_ROOT@/test',
    ],
)
