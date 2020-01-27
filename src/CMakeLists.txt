cmake_minimum_required(VERSION 3.0)
project(pgbackrest VERSION 2.23)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

configure_file(build.auto.h.in build.auto.h)
find_package(LibXml2 REQUIRED)
find_package(ZLIB REQUIRED)
find_package(OpenSSL REQUIRED)
pkg_check_modules(LibPQ libpq)

add_definitions(-D_POSIX_C_SOURCE=200809L)
add_compile_options(-std=c99 -Wall -Wextra -Wno-clobbered -Wno-missing-field-initializers)
include_directories(${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR} ${LIBXML2_INCLUDE_DIRS} ${LibPQ_INCLUDE_DIRS})
link_libraries(${LIBXML2_LIBRARY} ${LibPQ_LDFLAGS} ${ZLIB_LIBRARY} ${OPENSSL_SSL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY})
add_executable(pgbackrest 
	command/archive/common.c
	command/archive/get/file.c
	command/archive/get/get.c
	command/archive/get/protocol.c
	command/archive/push/file.c
	command/archive/push/protocol.c
	command/archive/push/push.c
	command/backup/backup.c
	command/backup/common.c
	command/backup/file.c
	command/backup/pageChecksum.c
	command/check/check.c
	command/check/common.c
	command/backup/protocol.c
	command/expire/expire.c
	command/help/help.c
	command/info/info.c
	command/command.c
	command/control/common.c
	command/control/start.c
	command/control/stop.c
	command/local/local.c
	command/restore/file.c
	command/restore/protocol.c
	command/restore/restore.c
	command/remote/remote.c
	command/stanza/common.c
	command/stanza/create.c
	command/stanza/delete.c
	command/stanza/upgrade.c
	command/storage/list.c
	common/compress/gzip/common.c
	common/compress/gzip/compress.c
	common/compress/gzip/decompress.c
	common/crypto/cipherBlock.c
	common/crypto/common.c
	common/crypto/hash.c
	common/debug.c
	common/encode.c
	common/encode/base64.c
	common/error.c
	common/exec.c
	common/exit.c
	common/fork.c
	common/io/bufferRead.c
	common/io/bufferWrite.c
	common/io/filter/buffer.c
	common/io/filter/filter.c
	common/io/filter/group.c
	common/io/filter/sink.c
	common/io/filter/size.c
	common/io/handleRead.c
	common/io/handleWrite.c
	common/io/http/cache.c
	common/io/http/client.c
	common/io/http/common.c
	common/io/http/header.c
	common/io/http/query.c
	common/io/io.c
	common/io/read.c
	common/io/tls/client.c
	common/io/write.c
	common/ini.c
	common/lock.c
	common/log.c
	common/memContext.c
	common/regExp.c
	common/stackTrace.c
	common/time.c
	common/type/buffer.c
	common/type/convert.c
	common/type/json.c
	common/type/keyValue.c
	common/type/list.c
	common/type/mcv.c
	common/type/string.c
	common/type/stringList.c
	common/type/variant.c
	common/type/variantList.c
	common/type/xml.c
	common/user.c
	common/wait.c
	config/config.c
	config/define.c
	config/exec.c
	config/load.c
	config/parse.c
	config/protocol.c
	db/db.c
	db/helper.c
	db/protocol.c
	info/info.c
	info/infoArchive.c
	info/infoBackup.c
	info/manifest.c
	info/infoPg.c
	postgres/client.c
	postgres/interface.c
	postgres/interface/v083.c
	postgres/interface/v084.c
	postgres/interface/v090.c
	postgres/interface/v091.c
	postgres/interface/v092.c
	postgres/interface/v093.c
	postgres/interface/v094.c
	postgres/interface/v095.c
	postgres/interface/v096.c
	postgres/interface/v100.c
	postgres/interface/v110.c
	postgres/interface/v120.c
	postgres/pageChecksum.c
	protocol/client.c
	protocol/command.c
	protocol/helper.c
	protocol/parallel.c
	protocol/parallelJob.c
	protocol/server.c
	storage/cifs/storage.c
	storage/posix/read.c
	storage/posix/storage.c
	storage/posix/write.c
	storage/remote/read.c
	storage/remote/protocol.c
	storage/remote/storage.c
	storage/remote/write.c
	storage/s3/read.c
	storage/s3/storage.c
	storage/s3/write.c
	storage/helper.c
	storage/read.c
	storage/storage.c
	storage/write.c
	main.c)

