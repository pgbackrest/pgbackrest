####################################################################################################################################
# VmTest.pm - Vm constants and data
####################################################################################################################################
package pgBackRestTest::Common::VmTest;

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use File::Basename qw(dirname);

use lib dirname($0) . '/../lib';
use pgBackRest::Db;

use Exporter qw(import);
    our @EXPORT = qw();

####################################################################################################################################
# Valid OS list
####################################################################################################################################
use constant OS_CO6                                                 => 'co6';
    push @EXPORT, qw(OS_CO6);
use constant OS_CO7                                                 => 'co7';
    push @EXPORT, qw(OS_CO7);
use constant OS_U12                                                 => 'u12';
    push @EXPORT, qw(OS_U12);
use constant OS_U14                                                 => 'u14';
    push @EXPORT, qw(OS_U14);

my $oyVm =
{
    # CentOS 6
    &OS_CO6 =>
    {
        db =>
        [
            PG_VERSION_90,
            PG_VERSION_91,
            PG_VERSION_92,
            PG_VERSION_93,
            PG_VERSION_94,
            PG_VERSION_95,
            PG_VERSION_96,
        ],

        db_minimal =>
        [
            PG_VERSION_90,
            PG_VERSION_91,
        ],
    },

    # CentOS 7
    &OS_CO7 =>
    {
        db =>
        [
            PG_VERSION_93,
            PG_VERSION_94,
            PG_VERSION_95,
            PG_VERSION_96,
        ],

        db_minimal =>
        [
            PG_VERSION_93,
            PG_VERSION_95,
            PG_VERSION_96,
        ],
    },

    # Ubuntu 12.04
    &OS_U12 =>
    {
        db =>
        [
            PG_VERSION_83,
            PG_VERSION_84,
            PG_VERSION_90,
            PG_VERSION_91,
            PG_VERSION_92,
            PG_VERSION_93,
            PG_VERSION_94,
            PG_VERSION_95,
            PG_VERSION_96,
        ],

        db_minimal =>
        [
            PG_VERSION_83,
            PG_VERSION_84,
        ],
    },

    # Ubuntu 14.04
    &OS_U14 =>
    {
        db =>
        [
            PG_VERSION_90,
            PG_VERSION_91,
            PG_VERSION_92,
            PG_VERSION_93,
            PG_VERSION_94,
            PG_VERSION_95,
            PG_VERSION_96,
        ],

        db_minimal =>
        [
            PG_VERSION_92,
            PG_VERSION_94,
        ],
    },
};

####################################################################################################################################
# vmGet
####################################################################################################################################
sub vmGet
{
    return $oyVm;
}

push @EXPORT, qw(vmGet);

1;
