####################################################################################################################################
# Write configuration
####################################################################################################################################
configure_file(output: 'build.auto.h', configuration: configuration)

####################################################################################################################################
# Build config target
####################################################################################################################################
# build parse.auto.c.inc
subdir('config')

####################################################################################################################################
# Build help target
####################################################################################################################################
# build help.auto.c.inc
subdir('command/help')

####################################################################################################################################
# test target
####################################################################################################################################
src_test = [
    '../../src/build/common/render.c',
    '../../src/build/common/yaml.c',
    '../../src/command/command.c',
    '../../src/command/exit.c',
    '../../src/command/help/help.c',
	'../../src/common/compress/bz2/common.c',
	'../../src/common/compress/bz2/decompress.c',
    '../../src/common/ini.c',
    '../../src/common/io/fd.c',
    '../../src/common/io/fdRead.c',
    '../../src/common/io/fdWrite.c',
    '../../src/common/lock.c',
    '../../src/common/stat.c',
    '../../src/common/type/json.c',
    '../../src/config/config.c',
    '../../src/config/parse.c',
    'command/test/build.c',
    'command/test/define.c',
    'command/test/test.c',
    'config/load.c',
    'main.c',
]

executable(
    'test-pgbackrest',
    src_common,
    src_test,
    test_help_auto_c_inc,
    test_parse_auto_c_inc,
    include_directories: include_directories('.', '../../src'),
    dependencies: [
        lib_bz2,
        lib_yaml,
    ],
    build_by_default: false,
)
