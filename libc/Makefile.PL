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
use lib dirname($0) . '/build/lib';

####################################################################################################################################
# Storage object to use for all file operations
####################################################################################################################################
use constant BACKREST_NAME                                          => 'pgBackRest';
use constant LIB_NAME                                               => 'LibC';
use constant LIB_AUTO_NAME                                          => LIB_NAME . 'Auto';

####################################################################################################################################
# Create C Makefile
####################################################################################################################################
my $strBuildPath = dirname(dirname(abs_path($0)));

# Create C files array
my @stryCFile =
(
    'LibC.c',

    'cipher/block.c',
    'cipher/cipher.c',
    'cipher/random.c',
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

# Link source files to build the C library.  The library has different build options and we don't want the bin build to reuse any of
# them.  Also, this makes the output of the __FILE__ macro prettier.
for (my $iFileIdx = 1; $iFileIdx < @stryCFile; $iFileIdx++)
{
    # Make directory
    system("mkdir -p " . dirname($stryCFile[$iFileIdx])) == 0
        or die('unable to mkdir ' . dirname($stryCFile[$iFileIdx]));

    # Link the file to the source directory
    my @stryLink = split('/', $stryCFile[$iFileIdx]);
    my $strLink;

    for (my $iLinkIdx = 0; $iLinkIdx < @stryLink; $iLinkIdx++)
    {
        $strLink .= (defined($strLink) ? '/' : '') . '..';
    }

    if (!-l $stryCFile[$iFileIdx])
    {
        system("ln -s ${strLink}/src/$stryCFile[$iFileIdx] $stryCFile[$iFileIdx]") == 0
            or die("unable to link ../src/$stryCFile[$iFileIdx] to $stryCFile[$iFileIdx]");
    }
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
