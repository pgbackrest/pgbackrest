use 5.010001;
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use ExtUtils::MakeMaker;

# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.
WriteMakefile
(
    NAME              => 'pgBackRest::LibC',
    VERSION_FROM      => 'lib/pgBackRest/LibC.pm', # finds $VERSION, requires EU::MM from perl >= 5.5
    AUTHOR            => 'David Steele <david@pgbackrest.org>',
    CCFLAGS           => '-std=c99 -msse4.1 -funroll-loops -ftree-vectorize -ftree-vectorizer-verbose=2',
    LIBS              => [''], # e.g., '-lm'
    DEFINE            => '', # e.g., '-DHAVE_SOMETHING'
    INC               => '-I.', # e.g., '-I. -I/usr/include/other'
    OBJECT            => 'LibC.o pageChecksum.o', # link all C files
);

if  (eval {require ExtUtils::Constant; 1})
{
    # List of C constants to export
    my @names =
    (
        qw(UVSIZE),
    );

    ExtUtils::Constant::WriteConstants
    (
        NAME            => 'pgBackRest::LibC',
        NAMES           => \@names,
        DEFAULT_TYPE    => 'IV',
        C_FILE          => 'const-c.inc',
        XS_FILE         => 'const-xs.inc',
    );
}
# Require constants to be built dynamically
else
{
    die "NO FALLBACK - ExtUtils::Constant is required to build constants!";
}
