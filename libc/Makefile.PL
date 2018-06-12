####################################################################################################################################
# Build Makefile and Auto-Generate Files Required for Build
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

use lib dirname($0) . '/lib';

####################################################################################################################################
# Storage object to use for all file operations
####################################################################################################################################
use constant BACKREST_NAME                                          => 'pgBackRest';

####################################################################################################################################
# Create C Makefile
####################################################################################################################################
my $strBuildPath = dirname(dirname(abs_path($0)));

# Create C files array
my @stryCFile =
(
    'LibC.c',

    'command/command.c',
    'common/debug.c',
    'common/encode.c',
    'common/encode/base64.c',
    'common/error.c',
    'common/ini.c',
    'common/io/handle.c',
    'common/lock.c',
    'common/log.c',
    'common/memContext.c',
    'common/regExp.c',
    'common/stackTrace.c',
    'common/time.c',
    'common/type/convert.c',
    'common/type/buffer.c',
    'common/type/keyValue.c',
    'common/type/list.c',
    'common/type/string.c',
    'common/type/stringList.c',
    'common/type/variant.c',
    'common/type/variantList.c',
    'common/wait.c',
    'config/config.c',
    'config/define.c',
    'config/load.c',
    'config/parse.c',
    'crypto/cipherBlock.c',
    'crypto/crypto.c',
    'crypto/hash.c',
    'crypto/random.c',
    'perl/config.c',
    'postgres/pageChecksum.c',
    'storage/driver/posix/driver.c',
    'storage/driver/posix/driverFile.c',
    'storage/driver/posix/driverRead.c',
    'storage/driver/posix/driverWrite.c',
    'storage/fileRead.c',
    'storage/fileWrite.c',
    'storage/helper.c',
    'storage/storage.c',
);

# Add ../src for files that are outside libc
for (my $iFileIdx = 1; $iFileIdx < @stryCFile; $iFileIdx++)
{
    $stryCFile[$iFileIdx] = '../src/' . $stryCFile[$iFileIdx];
}

# Write the makefile
WriteMakefile
(
    NAME => BACKREST_NAME . '::LibC',
    VERSION => '999',
    AUTHOR => 'David Steele <david@pgbackrest.org>',

    CCFLAGS => join(' ', qw)
        -Wfatal-errors -Wall -Wextra -Wwrite-strings -Wno-clobbered -Wno-missing-field-initializers
        -o $@
        -std=c99
        -D_FILE_OFFSET_BITS=64
    )),

    INC => join(' ', qw(
        -I.
        -I../src
    )),

    C => \@stryCFile,

    LIBS => [-lcrypto],

    OBJECT => '$(O_FILES)',
);
