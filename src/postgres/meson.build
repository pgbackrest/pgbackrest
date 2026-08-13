####################################################################################################################################
# Generate PostgreSQL interfaces
####################################################################################################################################
# Generate interface.auto.c.inc, unless this is a distribution tarball that ships the file. The generator writes into the build path
# so the file lands where the output is declared, else it would be missing there and ninja would run the generator on every build.
if not dist
    interface_auto_c_inc = custom_target(
        'interface.auto.c.inc',
        output: 'interface.auto.c.inc',
        depend_files: [
            '../../build/postgres.yaml',
            'interface/version.intern.h',
            'interface/version.vendor.h',
            build_code_lib,
        ],
        command: [
            build_code,
            'postgres',
            '--repo-path=@CURRENT_SOURCE_DIR@/../..',
            '--build-path=@BUILD_ROOT@',
        ],
    )
else
    # The shipped file is included by interface.c via the implicit source include path, so it is not a compiled source
    interface_auto_c_inc = []
endif
