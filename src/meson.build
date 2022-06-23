####################################################################################################################################
# It is very easy to get into confusing states when the source directory contains an in-place build, e.g. the wrong build.auto.h
# will be used. So just refuse to build in this case.
####################################################################################################################################
if import('fs').exists(meson.current_source_dir() / 'build.auto.h')
  error('''

Non-clean source code directory detected.

To build with meson the source tree may not have an in-place, ./configure
style, build configured. Use a separate check out for meson based builds, or
run 'make clean-all' in the source tree.

You can have both meson and ./configure style builds for the same source tree
by building out-of-source / VPATH with configure as well.''')
endif

####################################################################################################################################
# Write configuration
####################################################################################################################################
configure_file(output: 'build.auto.h', configuration: configuration)

####################################################################################################################################
# Common source used by all targets
####################################################################################################################################
src_common = files(
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
	'common/user.c',
	'common/wait.c',
	'config/common.c',
	'storage/posix/read.c',
	'storage/posix/storage.c',
	'storage/posix/write.c',
	'storage/read.c',
	'storage/storage.c',
	'storage/write.c',
)

####################################################################################################################################
# Build config target
####################################################################################################################################
src_build_config = [
	'build/common/render.c',
	'build/common/yaml.c',
	'build/config/main.c',
	'build/config/parse.c',
	'build/config/render.c',
]

build_config = executable(
    'build-config',
    src_common,
    src_build_config,
    dependencies : [
        lib_yaml
    ],
    build_by_default: false,
)

alias_target('build-config', build_config)

####################################################################################################################################
# Build error target
####################################################################################################################################
src_build_error = [
	'build/common/render.c',
	'build/common/yaml.c',
	'build/error/main.c',
	'build/error/parse.c',
	'build/error/render.c',
]

build_error = executable(
    'build-error',
    src_common,
    src_build_error,
    dependencies : [
        lib_yaml
    ],
    build_by_default: false,
)

alias_target('build-error', build_error)

####################################################################################################################################
# Build help target
####################################################################################################################################
src_build_help = [
	'build/common/render.c',
	'build/common/yaml.c',
	'build/config/parse.c',
	'build/help/main.c',
	'build/help/parse.c',
	'build/help/render.c',
	'common/compress/bz2/common.c',
	'common/compress/bz2/compress.c',
	'common/type/xml.c',
]

build_help = executable(
    'build-help',
    src_common,
    src_build_help,
    dependencies: [
        lib_bz2,
        lib_xml,
        lib_yaml
    ],
)

# build help.auto.c.inc
subdir('command/help')

alias_target('build-help', help_auto_c_inc)

####################################################################################################################################
# Build postgres target
####################################################################################################################################
src_build_postgres = [
	'build/common/render.c',
	'build/common/yaml.c',
	'build/postgres/main.c',
	'build/postgres/parse.c',
	'build/postgres/render.c',
]

build_postgres = executable(
    'build-postgres',
    src_common,
    src_build_postgres,
    dependencies : [
        lib_yaml
    ],
)

# build interface.auto.c.inc
subdir('postgres')

alias_target('build-postgres', build_postgres)

####################################################################################################################################
# pgBackRest target
####################################################################################################################################
src_pgbackrest = [
	'command/archive/common.c',
	'command/archive/get/file.c',
	'command/archive/get/get.c',
	'command/archive/get/protocol.c',
	'command/archive/push/file.c',
	'command/archive/push/protocol.c',
	'command/archive/push/push.c',
	'command/backup/backup.c',
	'command/backup/common.c',
	'command/backup/pageChecksum.c',
	'command/backup/protocol.c',
	'command/backup/file.c',
	'command/check/check.c',
	'command/check/common.c',
	'command/exit.c',
	'command/expire/expire.c',
	'command/help/help.c',
	'command/info/info.c',
	'command/command.c',
	'command/control/common.c',
	'command/control/start.c',
	'command/control/stop.c',
	'command/local/local.c',
	'command/repo/common.c',
	'command/repo/create.c',
	'command/repo/get.c',
	'command/repo/ls.c',
	'command/repo/put.c',
	'command/repo/rm.c',
	'command/restore/file.c',
	'command/restore/protocol.c',
	'command/restore/restore.c',
	'command/remote/remote.c',
	'command/server/ping.c',
	'command/server/server.c',
	'command/stanza/common.c',
	'command/stanza/create.c',
	'command/stanza/delete.c',
	'command/stanza/upgrade.c',
	'command/verify/file.c',
	'command/verify/protocol.c',
	'command/verify/verify.c',
	'common/compress/helper.c',
	'common/compress/bz2/common.c',
	'common/compress/bz2/compress.c',
	'common/compress/bz2/decompress.c',
	'common/compress/gz/common.c',
	'common/compress/gz/compress.c',
	'common/compress/gz/decompress.c',
	'common/compress/lz4/common.c',
	'common/compress/lz4/compress.c',
	'common/compress/lz4/decompress.c',
	'common/compress/zst/common.c',
	'common/compress/zst/compress.c',
	'common/compress/zst/decompress.c',
	'common/crypto/cipherBlock.c',
	'common/crypto/common.c',
	'common/crypto/hash.c',
	'common/exec.c',
	'common/fork.c',
	'common/ini.c',
	'common/io/client.c',
	'common/io/fd.c',
	'common/io/fdRead.c',
	'common/io/fdWrite.c',
	'common/io/filter/size.c',
	'common/io/http/client.c',
	'common/io/http/common.c',
	'common/io/http/header.c',
	'common/io/http/query.c',
	'common/io/http/request.c',
	'common/io/http/response.c',
	'common/io/http/session.c',
	'common/io/http/url.c',
	'common/io/server.c',
	'common/io/session.c',
	'common/io/socket/client.c',
	'common/io/socket/common.c',
	'common/io/socket/server.c',
	'common/io/socket/session.c',
	'common/io/tls/client.c',
	'common/io/tls/common.c',
	'common/io/tls/server.c',
	'common/io/tls/session.c',
	'common/lock.c',
	'common/stat.c',
	'common/type/json.c',
	'common/type/xml.c',
	'config/config.c',
	'config/exec.c',
	'config/load.c',
	'config/parse.c',
	'config/protocol.c',
	'db/db.c',
	'db/helper.c',
	'db/protocol.c',
	'info/info.c',
	'info/infoArchive.c',
	'info/infoBackup.c',
	'info/manifest.c',
	'info/infoPg.c',
	'postgres/client.c',
	'postgres/interface.c',
	'postgres/interface/page.c',
	'protocol/client.c',
	'protocol/command.c',
	'protocol/helper.c',
	'protocol/parallel.c',
	'protocol/parallelJob.c',
	'protocol/server.c',
	'storage/azure/helper.c',
	'storage/azure/read.c',
	'storage/azure/storage.c',
	'storage/azure/write.c',
	'storage/cifs/helper.c',
	'storage/cifs/storage.c',
	'storage/gcs/helper.c',
	'storage/gcs/read.c',
	'storage/gcs/storage.c',
	'storage/gcs/write.c',
	'storage/helper.c',
	'storage/remote/read.c',
	'storage/remote/protocol.c',
	'storage/remote/storage.c',
	'storage/remote/write.c',
	'storage/s3/helper.c',
	'storage/s3/read.c',
	'storage/s3/storage.c',
	'storage/s3/write.c',
    'main.c',
]

pgbackrest = executable(
    'pgbackrest',
    src_common,
    src_pgbackrest,
    help_auto_c_inc,
    interface_auto_c_inc,
    dependencies : [
        lib_bz2,
        lib_openssl,
        lib_lz4,
        lib_pq,
        lib_xml,
        lib_z,
        lib_zstd,
    ]
)

alias_target('pgbackrest', pgbackrest)
