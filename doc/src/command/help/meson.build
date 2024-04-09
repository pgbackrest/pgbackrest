####################################################################################################################################
# Generate help
####################################################################################################################################
doc_help_auto_c_inc = custom_target(
    'doc_help.auto.c.inc',
    output: 'help.auto.c.inc',
    depend_files: [
        '../../build/config/config.yaml',
        '../../build/help/help.xml',
    ],
    command: [
        build_code,
        'help',
        '@CURRENT_SOURCE_DIR@/../../..',
        '@BUILD_ROOT@/doc',
    ],
)
