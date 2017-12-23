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
use File::Copy qw(copy);

use lib dirname($0) . '/lib';
use pgBackRest::LibCAuto;

####################################################################################################################################
# Storage object to use for all file operations
####################################################################################################################################
use constant BACKREST_NAME                                          => 'pgBackRest';
use constant LIB_NAME                                               => 'LibC';
use constant LIB_AUTO_NAME                                          => LIB_NAME . 'Auto';

####################################################################################################################################
# Build list of constants to export from C and add them to the constant array
####################################################################################################################################
{
    # Build constants
    my $rhExport = pgBackRest::LibCAuto::libcAutoExportTag();
    my @stryConstant;

    foreach my $strSection (sort(keys(%{$rhExport})))
    {
        # Search exports for constants
        foreach my $strExport (@{$rhExport->{$strSection}})
        {
            # Constants will be upper-case
            if (uc($strExport) eq $strExport)
            {
                push(@stryConstant, $strExport);
            }
        }
    }

    # Build constant C code
    if  (eval {require ExtUtils::Constant; 1})
    {
        ExtUtils::Constant::WriteConstants
        (
            NAME            => BACKREST_NAME . '::' . LIB_NAME,
            NAMES           => \@stryConstant,
            DEFAULT_TYPE    => 'IV',
            C_FILE          => 'const-c.inc',
            XS_FILE         => 'const-xs.inc',
        );
    }
    else
    {
        die "ExtUtils::Constant is required to build constants!";
    }
}

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
    'common/encode.c',
    'common/encode/base64.c',
    'common/error.c',
    'common/errorType.c',
    'common/memContext.c',
    'common/type/buffer.c',
    'common/type/keyValue.c',
    'common/type/list.c',
    'common/type/string.c',
    'common/type/stringList.c',
    'common/type/variant.c',
    'common/type/variantList.c',
    'config/config.c',
    'config/define.c',
    'postgres/pageChecksum.c',
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
    VERSION_FROM => 'lib/' . BACKREST_NAME . '/LibC.pm',
    AUTHOR => 'David Steele <david@pgbackrest.org>',

    CCFLAGS => join(' ', qw(
        -Wfatal-errors -Wall -Wextra -Wwrite-strings -Wno-clobbered
        -o $@
        -std=c99
        -D_FILE_OFFSET_BITS=64
        -funroll-loops
        -ftree-vectorize
        $(CFLAGS)
    )),

    INC => join(' ', qw(
        -I.
        -I../src
    )),

    PM =>
    {
        ('lib/' . BACKREST_NAME . '/' . LIB_NAME . '.pm') => ('$(INST_LIB)/' . BACKREST_NAME . '/' . LIB_NAME . '.pm'),
        ('lib/' . BACKREST_NAME . '/' . LIB_AUTO_NAME . '.pm') => ('$(INST_LIB)/' . BACKREST_NAME . '/' . LIB_AUTO_NAME . '.pm'),
    },

    C => \@stryCFile,

    LIBS => [-lcrypto],

    OBJECT => '$(O_FILES)',
);
