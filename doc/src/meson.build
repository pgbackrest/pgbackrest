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
src_doc = [
    '../../src/build/common/json.c',
    '../../src/build/common/regExp.c',
    '../../src/build/common/render.c',
    '../../src/build/common/string.c',
    '../../src/build/common/xml.c',
    '../../src/build/common/yaml.c',
    '../../src/build/config/parse.c',
    '../../src/build/help/parse.c',
    '../../src/build/help/render.c',
    '../../src/command/command.c',
    '../../src/command/exit.c',
    '../../src/command/help/help.c',
    '../../src/command/lock.c',
    '../../src/common/compress/common.c',
    '../../src/common/compress/bz2/common.c',
    '../../src/common/compress/bz2/compress.c',
    '../../src/common/compress/bz2/decompress.c',
    '../../src/common/ini.c',
    '../../src/common/io/fd.c',
    '../../src/common/io/fdRead.c',
    '../../src/common/io/fdWrite.c',
    '../../src/common/lock.c',
    '../../src/common/stat.c',
    '../../src/config/config.c',
    '../../src/config/parse.c',
    'command/build/build.c',
    'command/build/man.c',
    'command/build/reference.c',
    'config/load.c',
    'main.c',
]

executable(
    'doc-pgbackrest',
    src_common,
    src_doc,
    doc_help_auto_c_inc,
    doc_parse_auto_c_inc,
    include_directories: include_directories('.', '../../src'),
    c_args: [
        # Use large buffer sizes to capture large error/log outputs
        '-DERROR_MESSAGE_BUFFER_SIZE=262144',
        '-DLOG_BUFFER_SIZE=262144',
        arg_unity,
    ],
    dependencies: [
        lib_backtrace,
        lib_bz2,
        lib_xml,
        lib_yaml,
    ],
    build_by_default: false,
)
