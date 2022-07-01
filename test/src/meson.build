####################################################################################################################################
# Write configuration
####################################################################################################################################
if cc.get_id() != 'msvc'
configure_file(output: 'build.auto.h', configuration: configuration)
else
configure_file(input: '../../src/win32_msvc/build.auto.h.in', output: 'build.auto.h', configuration: configuration)
endif

####################################################################################################################################
# Build config target
####################################################################################################################################
# build parse.auto.c.inc
subdir('config')

alias_target('test-build-config', test_parse_auto_c_inc)

####################################################################################################################################
# Build help target
####################################################################################################################################
# build help.auto.c.inc
subdir('command/help')

alias_target('test-build-help', test_help_auto_c_inc)

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

if cc.get_id() != 'msvc'
test_pgbackrest = executable(
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

else

test_pgbackrest = executable(
    'test-pgbackrest',
    src_common,
    src_test,
    test_help_auto_c_inc,
    test_parse_auto_c_inc,
    src_win32,
    include_directories: [
        include_directories('.', '../../src'),
        dep_inc_dir,
        include_dir_win32,
    ],
    dependencies: [
        lib_bz2,
        lib_yaml,
        lib_xml,
        lib_regex,
        win32_deps,
    ],
    build_by_default: true,
)

endif

alias_target('test-pgbackrest', test_pgbackrest)
