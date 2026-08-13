####################################################################################################################################
# Generate help
####################################################################################################################################
# Generate help.auto.c.inc, unless this is a distribution tarball that ships the file. The generator writes into the build path so
# the file lands where the output is declared, else it would be missing there and ninja would run the generator on every build.
if not dist
    help_auto_c_inc = custom_target(
        'help.auto.c.inc',
        output: 'help.auto.c.inc',
        depend_files: [
            '../../../build/config.yaml',
            '../../../doc/xml/reference.xml',
            build_code_lib,
        ],
        command: [
            build_code,
            'help',
            '--repo-path=@CURRENT_SOURCE_DIR@/../../..',
            '--build-path=@BUILD_ROOT@',
        ],
    )
else
    # The shipped file is included by help.c via the implicit source include path, so it is not a compiled source
    help_auto_c_inc = []
endif
