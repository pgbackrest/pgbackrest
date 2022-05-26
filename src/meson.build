project('pgbackrest', 'c')

cc = meson.get_compiler('c')

lib_bz2 = cc.find_library('bz2')
lib_z = dependency('zlib')
lib_xml = dependency('libxml-2.0')
lib_yaml = cc.find_library('yaml')

conf_data = configuration_data()

conf_data.set_quoted('CFGOPTDEF_CONFIG_PATH', '/etc/pgbackrest')
conf_data.set('NDEBUG', true)

configure_file(output: 'build.auto.h', configuration: conf_data)

src_common = [
	'common/compress/bz2/common.c',
	'common/compress/bz2/compress.c',
	'common/debug.c',
	'common/encode.c',
	'common/error.c',
	'common/io/filter/buffer.c',
	'common/io/filter/filter.c',
	'common/io/filter/group.c',
	'common/io/filter/sink.c',
	'common/io/bufferRead.c',
	'common/io/bufferWrite.c',
	'common/io/io.c',
	'common/io/read.c',
	'common/io/write.c',
	'common/log.c',
	'common/memContext.c',
	'common/regExp.c',
	'common/stackTrace.c',
	'common/time.c',
	'common/type/buffer.c',
	'common/type/convert.c',
	'common/type/keyValue.c',
	'common/type/list.c',
	'common/type/object.c',
	'common/type/pack.c',
	'common/type/string.c',
	'common/type/stringZ.c',
	'common/type/stringId.c',
	'common/type/stringList.c',
	'common/type/variant.c',
	'common/type/variantList.c',
	'common/type/xml.c',
	'common/user.c',
	'common/wait.c',
	'config/common.c',
	'storage/posix/read.c',
	'storage/posix/storage.c',
	'storage/posix/write.c',
	'storage/read.c',
	'storage/storage.c',
	'storage/write.c',
]

src_build_help = [
	'build/common/render.c',
	'build/common/yaml.c',
	'build/config/parse.c',
	'build/help/main.c',
	'build/help/parse.c',
	'build/help/render.c',
]

build_help = executable('build-help', src_common, src_build_help, dependencies : [lib_bz2, lib_xml, lib_yaml])

subdir('command/help')

executable('pgbackrest', 'main.c', 'help.auto.c', dependencies : [lib_bz2])
