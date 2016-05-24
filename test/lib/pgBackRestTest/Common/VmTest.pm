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
# VM hash keywords
####################################################################################################################################
use constant VM_IMAGE                                               => 'image';
    push @EXPORT, qw(VM_IMAGE);
use constant VM_OS                                                  => 'os';
    push @EXPORT, qw(VM_OS);
use constant VM_OS_BASE                                             => 'os-base';
    push @EXPORT, qw(VM_OS_BASE);
use constant VM_OS_REPO                                             => 'os-repo';
    push @EXPORT, qw(VM_OS_REPO);

####################################################################################################################################
# Valid OS base List
####################################################################################################################################
use constant VM_OS_BASE_DEBIAN                                      => 'debian';
    push @EXPORT, qw(VM_OS_BASE_DEBIAN);
use constant VM_OS_BASE_RHEL                                        => 'rhel';
    push @EXPORT, qw(VM_OS_BASE_RHEL);

####################################################################################################################################
# Valid OS list
####################################################################################################################################
use constant VM_OS_CENTOS                                           => 'centos';
    push @EXPORT, qw(VM_OS_CENTOS);
use constant VM_OS_DEBIAN                                           => 'debian';
    push @EXPORT, qw(VM_OS_DEBIAN);
use constant VM_OS_UBUNTU                                           => 'ubuntu';
    push @EXPORT, qw(VM_OS_DEBIAN);

####################################################################################################################################
# Valid VM list
####################################################################################################################################
use constant VM_CO6                                                 => 'co6';
    push @EXPORT, qw(VM_CO6);
use constant VM_CO7                                                 => 'co7';
    push @EXPORT, qw(VM_CO7);
use constant VM_U12                                                 => 'u12';
    push @EXPORT, qw(VM_U12);
use constant VM_U14                                                 => 'u14';
    push @EXPORT, qw(VM_U14);
use constant VM_U16                                                 => 'u16';
    push @EXPORT, qw(VM_U16);
use constant VM_D8                                                  => 'd8';
    push @EXPORT, qw(VM_D8);

my $oyVm =
{
    # CentOS 6
    &VM_CO6 =>
    {
        &VM_OS_BASE => VM_OS_BASE_RHEL,
        &VM_OS => VM_OS_CENTOS,
        &VM_IMAGE => 'centos:6',

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
    &VM_CO7 =>
    {
        &VM_OS_BASE => VM_OS_BASE_RHEL,
        &VM_OS => VM_OS_CENTOS,
        &VM_IMAGE => 'centos:7',

        db =>
        [
            PG_VERSION_93,
            PG_VERSION_94,
            PG_VERSION_95,
            PG_VERSION_96,
        ],

        db_minimal =>
        [
            PG_VERSION_95,
            PG_VERSION_96,
        ],
    },

    # Debian 8
    &VM_D8 =>
    {
        &VM_OS_BASE => VM_OS_BASE_DEBIAN,
        &VM_OS => VM_OS_DEBIAN,
        &VM_OS_REPO => 'jessie',
        &VM_IMAGE => 'debian:8',

        db =>
        [
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
            PG_VERSION_84,
            PG_VERSION_93,
        ],
    },

    # Ubuntu 12.04
    &VM_U12 =>
    {
        &VM_OS_BASE => VM_OS_BASE_DEBIAN,
        &VM_OS => VM_OS_UBUNTU,
        &VM_OS_REPO => 'precise',
        &VM_IMAGE => 'ubuntu:12.04',

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
        ],
    },

    # Ubuntu 14.04
    &VM_U14 =>
    {
        &VM_OS_BASE => VM_OS_BASE_DEBIAN,
        &VM_OS => VM_OS_UBUNTU,
        &VM_OS_REPO => 'trusty',
        &VM_IMAGE => 'ubuntu:14.04',

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
        ],
    },

    # Ubuntu 16.04
    &VM_U16 =>
    {
        &VM_OS_BASE => VM_OS_BASE_DEBIAN,
        &VM_OS => VM_OS_UBUNTU,
        &VM_OS_REPO => 'xenial',
        &VM_IMAGE => 'ubuntu:16.04',

        db =>
        [
            PG_VERSION_91,
            PG_VERSION_92,
            PG_VERSION_93,
            PG_VERSION_94,
            PG_VERSION_95,
            PG_VERSION_96,
        ],

        db_minimal =>
        [
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
