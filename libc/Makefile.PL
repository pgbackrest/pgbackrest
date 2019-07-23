####################################################################################################################################
# Build Makefile and Auto-Generate Files Required for Build
#
# The C library is only used for Perl unit tests.  For a production build all C library exports are built directly into the
# pgbackrest executable.  See src/perl/libc.auto.c.
####################################################################################################################################
use 5.010001;
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

# Convert die to confess to capture the stack trace
$SIG{__DIE__} = sub { Carp::confess @_ };

use Cwd qw(abs_path);
use ExtUtils::MakeMaker;
use File::Basename qw(dirname);

####################################################################################################################################
# Storage object to use for all file operations
####################################################################################################################################
use constant PROJECT_NAME                                           => 'pgBackRest';

####################################################################################################################################
# Make sure the makefile is being created in an expected test directory.  This should prevent users from building it in production.
####################################################################################################################################
if (dirname($0) !~ /\.vagrant\/bin\/[^\/]+\/libc$/)
{
    confess
        "LibC is not being built in a test directory (" . dirname($0) .
        ").  LibC should not be distributed for production builds.  See build documentation for details.";
}

####################################################################################################################################
# Create C Makefile
####################################################################################################################################
my $strBuildPath = dirname(dirname(abs_path($0)));

# Create C files array
my @stryCFile =
(
    'LibC.c',

    'command/backup/pageChecksum.c',
    'command/command.c',
    'common/compress/gzip/common.c',
    'common/compress/gzip/compress.c',
    'common/compress/gzip/decompress.c',
    'common/crypto/cipherBlock.c',
    'common/crypto/common.c',
    'common/crypto/hash.c',
    'common/debug.c',
    'common/encode.c',
    'common/encode/base64.c',
    'common/error.c',
    'common/ini.c',
    'common/io/bufferRead.c',
    'common/io/bufferWrite.c',
    'common/io/filter/buffer.c',
    'common/io/filter/filter.c',
    'common/io/filter/group.c',
    'common/io/filter/sink.c',
    'common/io/filter/size.c',
    'common/io/handleWrite.c',
    'common/io/http/cache.c',
    'common/io/http/client.c',
    'common/io/http/common.c',
    'common/io/http/header.c',
    'common/io/http/query.c',
    'common/io/io.c',
    'common/io/read.c',
    'common/io/tls/client.c',
    'common/io/write.c',
    'common/lock.c',
    'common/log.c',
    'common/memContext.c',
    'common/regExp.c',
    'common/stackTrace.c',
    'common/time.c',
    'common/type/convert.c',
    'common/type/buffer.c',
    'common/type/json.c',
    'common/type/keyValue.c',
    'common/type/list.c',
    'common/type/string.c',
    'common/type/stringList.c',
    'common/type/variant.c',
    'common/type/variantList.c',
    'common/type/xml.c',
    'common/wait.c',
    'config/config.c',
    'config/define.c',
    'config/load.c',
    'config/parse.c',
    'perl/config.c',
	'protocol/client.c',
	'protocol/command.c',
	'protocol/helper.c',
	'protocol/parallel.c',
	'protocol/parallelJob.c',
	'protocol/server.c',
    'postgres/client.c',
    'postgres/pageChecksum.c',
    'storage/posix/read.c',
    'storage/posix/storage.c',
    'storage/posix/write.c',
    'storage/s3/read.c',
    'storage/s3/storage.c',
    'storage/s3/write.c',
    'storage/helper.c',
    'storage/read.c',
    'storage/storage.c',
    'storage/write.c',
);

# Add ../src for files that are outside libc
for (my $iFileIdx = 1; $iFileIdx < @stryCFile; $iFileIdx++)
{
    $stryCFile[$iFileIdx] = '../src/' . $stryCFile[$iFileIdx];
}

# Write the makefile
WriteMakefile
(
    NAME => PROJECT_NAME . '::LibC',
    VERSION => '999',
    AUTHOR => 'David Steele <david@pgbackrest.org>',

    CCFLAGS => join(' ', qw)
        -Wfatal-errors -Wall -Wextra -Wwrite-strings -Wno-clobbered -Wno-missing-field-initializers
        -o $@
        -std=c99
        -D_POSIX_C_SOURCE=200112L
        -D_FILE_OFFSET_BITS=64
        `xml2-config --cflags`
        -I`pg_config --includedir`
    )),

    INC => join(' ', qw(
        -I.
        -I../src
    )),

    C => \@stryCFile,

    LIBS => '-lcrypto -lpq -lssl -lxml2',

    OBJECT => '$(O_FILES)',
);
