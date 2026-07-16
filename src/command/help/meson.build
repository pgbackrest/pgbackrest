####################################################################################################################################
# Generate help
####################################################################################################################################
# Generate help.auto.c.inc, unless this is a distribution tarball that ships the file
if not dist
    help_auto_c_inc = custom_target(
        'help.auto.c.inc',
        output: 'help.auto.c.inc',
        depend_files: [
            '../../build/config/config.yaml',
            '../../build/help/help.xml',
        ],
        command: [
            build_code,
            'help',
            '@CURRENT_SOURCE_DIR@/../../..',
        ],
    )
else
    # The shipped file is included by main.c via the implicit source include path, so it is not a compiled source
    help_auto_c_inc = []
endif
