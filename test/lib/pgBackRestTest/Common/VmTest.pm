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

use Exporter qw(import);
    our @EXPORT = qw();

use pgBackRest::Common::Log;
use pgBackRest::DbVersion;

####################################################################################################################################
# VM hash keywords
####################################################################################################################################
use constant VM_DB                                                  => 'db';
    push @EXPORT, qw(VM_DB);
use constant VM_DB_DOC                                              => 'db-doc';
    push @EXPORT, qw(VM_DB_DOC);
use constant VM_DB_MINIMAL                                          => 'db-minimal';
    push @EXPORT, qw(VM_DB_MINIMAL);
use constant VM_CONTROL_MASTER                                      => 'control-master';
    push @EXPORT, qw(VM_CONTROL_MASTER);
use constant VM_IMAGE                                               => 'image';
    push @EXPORT, qw(VM_IMAGE);
use constant VM_OS                                                  => 'os';
    push @EXPORT, qw(VM_OS);
use constant VM_OS_BASE                                             => 'os-base';
    push @EXPORT, qw(VM_OS_BASE);
use constant VM_OS_REPO                                             => 'os-repo';
    push @EXPORT, qw(VM_OS_REPO);
use constant VMDEF_PGSQL_BIN                                        => 'pgsql-bin';
    push @EXPORT, qw(VMDEF_PGSQL_BIN);
use constant VMDEF_PERL_ARCH_PATH                                   => 'perl-arch-path';
    push @EXPORT, qw(VMDEF_PERL_ARCH_PATH);

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
use constant VM_ALL                                                 => 'all';
    push @EXPORT, qw(VM_ALL);

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

# Defines the host VM (the VM that the containers run in)
use constant VM_HOST_DEFAULT                                        => VM_U16;
    push @EXPORT, qw(VM_HOST_DEFAULT);

# Lists valid VMs
use constant VM_LIST                                                => (VM_CO6, VM_U16, VM_D8, VM_CO7, VM_U14);
    push @EXPORT, qw(VM_LIST);

my $oyVm =
{
    # CentOS 6
    &VM_CO6 =>
    {
        &VM_OS_BASE => VM_OS_BASE_RHEL,
        &VM_OS => VM_OS_CENTOS,
        &VM_IMAGE => 'centos:6',
        &VM_CONTROL_MASTER => false,
        &VMDEF_PGSQL_BIN => '/usr/pgsql-{[version]}/bin',
        &VMDEF_PERL_ARCH_PATH => '/usr/local/lib64/perl5',

        &VM_DB =>
        [
            PG_VERSION_90,
            PG_VERSION_91,
            PG_VERSION_92,
            PG_VERSION_93,
            PG_VERSION_94,
            PG_VERSION_95,
            PG_VERSION_96,
        ],

        &VM_DB_MINIMAL =>
        [
            PG_VERSION_90,
            PG_VERSION_91,
            PG_VERSION_95,
        ],

        &VM_DB_DOC =>
        [
            PG_VERSION_95,
        ],
    },

    # CentOS 7
    &VM_CO7 =>
    {
        &VM_OS_BASE => VM_OS_BASE_RHEL,
        &VM_OS => VM_OS_CENTOS,
        &VM_IMAGE => 'centos:7',
        &VM_CONTROL_MASTER => false,
        &VMDEF_PGSQL_BIN => '/usr/pgsql-{[version]}/bin',
        &VMDEF_PERL_ARCH_PATH => '/usr/local/lib64/perl5',

        &VM_DB =>
        [
            PG_VERSION_93,
            PG_VERSION_94,
            PG_VERSION_95,
            PG_VERSION_96,
        ],

        &VM_DB_MINIMAL =>
        [
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
        &VM_CONTROL_MASTER => false,
        &VMDEF_PGSQL_BIN => '/usr/lib/postgresql/{[version]}/bin',
        &VMDEF_PERL_ARCH_PATH => '/usr/local/lib/x86_64-linux-gnu/perl/5.20.2',

        &VM_DB =>
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

        &VM_DB_MINIMAL =>
        [
            PG_VERSION_84,
            PG_VERSION_92,
        ],
    },

    # Ubuntu 12.04 (no longer included in normal testing, but here if needed)
    &VM_U12 =>
    {
        &VM_OS_BASE => VM_OS_BASE_DEBIAN,
        &VM_OS => VM_OS_UBUNTU,
        &VM_OS_REPO => 'precise',
        &VM_IMAGE => 'ubuntu:12.04',
        &VM_CONTROL_MASTER => false,
        &VMDEF_PGSQL_BIN => '/usr/lib/postgresql/{[version]}/bin',
        &VMDEF_PERL_ARCH_PATH => '/usr/local/lib/perl/5.14.2',

        &VM_DB =>
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

        &VM_DB_MINIMAL =>
        [
            PG_VERSION_84,
        ],
    },

    # Ubuntu 14.04
    &VM_U14 =>
    {
        &VM_OS_BASE => VM_OS_BASE_DEBIAN,
        &VM_OS => VM_OS_UBUNTU,
        &VM_OS_REPO => 'trusty',
        &VM_IMAGE => 'ubuntu:14.04',
        &VM_CONTROL_MASTER => false,
        &VMDEF_PGSQL_BIN => '/usr/lib/postgresql/{[version]}/bin',
        &VMDEF_PERL_ARCH_PATH => '/usr/local/lib/perl/5.18.2',

        &VM_DB =>
        [
            PG_VERSION_90,
            PG_VERSION_91,
            PG_VERSION_92,
            PG_VERSION_93,
            PG_VERSION_94,
            PG_VERSION_95,
            PG_VERSION_96,
        ],

        &VM_DB_MINIMAL =>
        [
            PG_VERSION_94,
        ],

        &VM_DB_DOC =>
        [
            PG_VERSION_94,
        ],
    },

    # Ubuntu 16.04
    &VM_U16 =>
    {
        &VM_OS_BASE => VM_OS_BASE_DEBIAN,
        &VM_OS => VM_OS_UBUNTU,
        &VM_OS_REPO => 'xenial',
        &VM_IMAGE => 'ubuntu:16.04',
        &VM_CONTROL_MASTER => false,
        &VMDEF_PGSQL_BIN => '/usr/lib/postgresql/{[version]}/bin',
        &VMDEF_PERL_ARCH_PATH => '/usr/local/lib/x86_64-linux-gnu/perl/5.22.1',

        &VM_DB =>
        [
            PG_VERSION_91,
            PG_VERSION_92,
            PG_VERSION_93,
            PG_VERSION_94,
            PG_VERSION_95,
            PG_VERSION_96,
        ],

        &VM_DB_MINIMAL =>
        [
            PG_VERSION_93,
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

####################################################################################################################################
# vmBaseTest
####################################################################################################################################
sub vmBaseTest
{
    my $strVm = shift;
    my $strDistroTest = shift;

    return $oyVm->{$strVm}{&VM_OS_BASE} eq $strDistroTest ? true : false;
}

push @EXPORT, qw(vmBaseTest);

####################################################################################################################################
# vmCoverage
####################################################################################################################################
sub vmCoverage
{
    my $strVm = shift;

    return $strVm eq VM_ALL ? false : vmBaseTest($strVm, VM_OS_BASE_DEBIAN);
}

push @EXPORT, qw(vmCoverage);

1;
