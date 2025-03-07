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

use pgBackRestDoc::Common::Exception;
use pgBackRestDoc::Common::Log;
use pgBackRestDoc::Common::String;

use pgBackRestTest::Common::DbVersion;

####################################################################################################################################
# VM hash keywords
####################################################################################################################################
use constant VM_ARCH                                                => 'arch';
    push @EXPORT, qw(VM_ARCH);
use constant VM_DB                                                  => 'db';
    push @EXPORT, qw(VM_DB);
use constant VM_DB_TEST                                             => 'db-test';
    push @EXPORT, qw(VM_DB_TEST);
use constant VMDEF_DEBUG_INTEGRATION                                => 'debug-integration';
    push @EXPORT, qw(VMDEF_DEBUG_INTEGRATION);
use constant VM_CONTROL_MTR                                         => 'control-mtr';
    push @EXPORT, qw(VM_CONTROL_MTR);
# Will coverage testing be run for C?
use constant VMDEF_COVERAGE_C                                       => 'coverage-c';
use constant VM_DEPRECATED                                          => 'deprecated';
    push @EXPORT, qw(VM_DEPRECATED);
use constant VM_IMAGE                                               => 'image';
    push @EXPORT, qw(VM_IMAGE);
use constant VM_OS_BASE                                             => 'os-base';
    push @EXPORT, qw(VM_OS_BASE);
use constant VMDEF_PG_REPO                                          => 'pg-repo';
use constant VMDEF_PGSQL_BIN                                        => 'psql-bin';
    push @EXPORT, qw(VMDEF_PGSQL_BIN);
use constant VMDEF_WITH_LZ4                                         => 'with-lz4';
    push @EXPORT, qw(VMDEF_WITH_LZ4);
use constant VMDEF_WITH_ZST                                         => 'with-zst';
    push @EXPORT, qw(VMDEF_WITH_ZST);

####################################################################################################################################
# Valid OS base List
####################################################################################################################################
use constant VM_OS_BASE_DEBIAN                                      => 'debian';
    push @EXPORT, qw(VM_OS_BASE_DEBIAN);
use constant VM_OS_BASE_RHEL                                        => 'rhel';
    push @EXPORT, qw(VM_OS_BASE_RHEL);

####################################################################################################################################
# Valid architecture list
####################################################################################################################################
use constant VM_ARCH_AARCH64                                        => 'aarch64';
    push @EXPORT, qw(VM_ARCH_AARCH64);
use constant VM_ARCH_I386                                           => 'i386';
    push @EXPORT, qw(VM_ARCH_I386);
use constant VM_ARCH_AMD64                                          => 'amd64';
    push @EXPORT, qw(VM_ARCH_AMD64);

####################################################################################################################################
# Valid VM list
####################################################################################################################################
use constant VM_ALL                                                 => 'all';
    push @EXPORT, qw(VM_ALL);

use constant VM_NONE                                                => 'none';
    push @EXPORT, qw(VM_NONE);

use constant VM_D11                                                 => 'd11';
    push @EXPORT, qw(VM_D11);
use constant VM_RH8                                                 => 'rh8';
    push @EXPORT, qw(VM_RH8);
use constant VM_F41                                                 => 'f41';
    push @EXPORT, qw(VM_F41);
use constant VM_U20                                                 => 'u20';
    push @EXPORT, qw(VM_U20);
use constant VM_U22                                                 => 'u22';
    push @EXPORT, qw(VM_U22);

# List of default test VMs
use constant VM_LIST                                                => (VM_U20, VM_D11, VM_RH8, VM_U22);
    push @EXPORT, qw(VM_LIST);

my $oyVm =
{
    # None
    &VM_NONE =>
    {
        &VM_OS_BASE => VM_OS_BASE_DEBIAN,
        &VM_ARCH => VM_ARCH_AMD64,
        &VMDEF_COVERAGE_C => true,
        &VMDEF_PGSQL_BIN => '/usr/lib/postgresql/{[version]}/bin',

        &VMDEF_WITH_ZST => true,

        &VM_DB =>
        [
            PG_VERSION_10,
        ],

        &VM_DB_TEST =>
        [
            PG_VERSION_10,
        ],
    },

    # Debian 11
    &VM_D11 =>
    {
        &VM_OS_BASE => VM_OS_BASE_DEBIAN,
        &VM_IMAGE => 'debian:11',
        &VM_ARCH => VM_ARCH_I386,
        &VMDEF_PG_REPO => false,
        &VMDEF_PGSQL_BIN => '/usr/lib/postgresql/{[version]}/bin',

        &VMDEF_WITH_ZST => true,

        &VM_DB =>
        [
            PG_VERSION_13,
        ],

        &VM_DB_TEST =>
        [
            PG_VERSION_13,
        ],
    },

    # RHEL 8
    &VM_RH8 =>
    {
        &VM_OS_BASE => VM_OS_BASE_RHEL,
        &VM_IMAGE => 'rockylinux/rockylinux:8',
        &VM_ARCH => VM_ARCH_AMD64,
        &VMDEF_PGSQL_BIN => '/usr/pgsql-{[version]}/bin',

        &VMDEF_DEBUG_INTEGRATION => false,
        &VMDEF_WITH_ZST => true,

        &VM_DB =>
        [
            PG_VERSION_13,
            PG_VERSION_14,
            PG_VERSION_15,
            PG_VERSION_16,
            PG_VERSION_17,
        ],

        &VM_DB_TEST =>
        [
            PG_VERSION_14,
            PG_VERSION_15,
            PG_VERSION_16,
        ],
    },

    # Fedora 41
    &VM_F41 =>
    {
        &VM_OS_BASE => VM_OS_BASE_RHEL,
        &VM_IMAGE => 'fedora:41',
        &VM_ARCH => VM_ARCH_AMD64,
        &VMDEF_PGSQL_BIN => '/usr/pgsql-{[version]}/bin',
        &VMDEF_COVERAGE_C => true,

        &VMDEF_DEBUG_INTEGRATION => false,
        &VMDEF_WITH_ZST => true,

        &VM_DB =>
        [
            PG_VERSION_13,
            PG_VERSION_14,
            PG_VERSION_15,
            PG_VERSION_16,
            PG_VERSION_17,
        ],

        &VM_DB_TEST =>
        [
            PG_VERSION_15,
        ],
    },

    # Ubuntu 20.04
    &VM_U20 =>
    {
        &VM_OS_BASE => VM_OS_BASE_DEBIAN,
        &VM_IMAGE => 'ubuntu:20.04',
        &VM_ARCH => VM_ARCH_AMD64,
        &VMDEF_COVERAGE_C => true,
        &VMDEF_PGSQL_BIN => '/usr/lib/postgresql/{[version]}/bin',

        &VMDEF_WITH_ZST => true,

        &VM_DB =>
        [
            PG_VERSION_95,
            PG_VERSION_96,
            PG_VERSION_10,
            PG_VERSION_11,
            PG_VERSION_12,
            PG_VERSION_13,
            PG_VERSION_14,
            PG_VERSION_15,
            PG_VERSION_16,
            PG_VERSION_17,
        ],

        &VM_DB_TEST =>
        [
            PG_VERSION_95,
            PG_VERSION_96,
        ],
    },

    # Ubuntu 22.04
    &VM_U22 =>
    {
        &VM_OS_BASE => VM_OS_BASE_DEBIAN,
        &VM_IMAGE => 'ubuntu:22.04',
        &VM_ARCH => VM_ARCH_AMD64,
        &VMDEF_COVERAGE_C => true,
        &VMDEF_PGSQL_BIN => '/usr/lib/postgresql/{[version]}/bin',

        &VMDEF_WITH_ZST => true,

        &VM_DB =>
        [
            PG_VERSION_95,
            PG_VERSION_96,
            PG_VERSION_10,
            PG_VERSION_11,
            PG_VERSION_12,
            PG_VERSION_13,
            PG_VERSION_14,
            PG_VERSION_15,
            PG_VERSION_16,
            PG_VERSION_17,
        ],

        &VM_DB_TEST =>
        [
            PG_VERSION_10,
            PG_VERSION_11,
            PG_VERSION_12,
            PG_VERSION_17,
        ],
    },
};

####################################################################################################################################
# Set VM_DB_TEST to VM_DB if it is not defined so it doesn't have to be checked everywhere
####################################################################################################################################
foreach my $strVm (sort(keys(%{$oyVm})))
{
    if (!defined($oyVm->{$strVm}{&VM_DB_TEST}))
    {
        $oyVm->{$strVm}{&VM_DB_TEST} = $oyVm->{$strVm}{&VM_DB};
    }
}

####################################################################################################################################
# Verify that each version of PostgreSQL is represented in one and only one default VM
####################################################################################################################################
foreach my $strPgVersion (versionSupport())
{
    my $strVmPgVersionRun;
    my $bVmCoverageC = false;

    foreach my $strVm (VM_LIST)
    {
        if (vmCoverageC($strVm))
        {
            $bVmCoverageC = true;
        }

        foreach my $strVmPgVersion (@{$oyVm->{$strVm}{&VM_DB_TEST}})
        {
            if ($strPgVersion eq $strVmPgVersion)
            {
                if (defined($strVmPgVersionRun))
                {
                    confess &log(ASSERT, "PostgreSQL $strPgVersion is already configured to run on default vm $strVm");
                }

                $strVmPgVersionRun = $strVm;
            }
        }
    }

    my $strErrorSuffix = 'is not configured to run on a default vm';

    if (!$bVmCoverageC)
    {
        confess &log(ASSERT, "C coverage ${strErrorSuffix}");
    }

    if (!defined($strVmPgVersionRun))
    {
        confess &log(ASSERT, "PostgreSQL ${strPgVersion} ${strErrorSuffix}");
    }
}

####################################################################################################################################
# vmValid
####################################################################################################################################
sub vmValid
{
    my $strVm = shift;

    if (!defined($oyVm->{$strVm}))
    {
        confess &log(ERROR, "no definition for vm '${strVm}'");
    }
}

push @EXPORT, qw(vmValid);

####################################################################################################################################
# vmPgRepo
####################################################################################################################################
sub vmPgRepo
{
    my $strVm = shift;

    vmValid($strVm);

    if (!defined($oyVm->{$strVm}{&VMDEF_PG_REPO}))
    {
        return true;
    }

    return $oyVm->{$strVm}{&VMDEF_PG_REPO};
}

push @EXPORT, qw(vmPgRepo);

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
# vmCoverageC
####################################################################################################################################
sub vmCoverageC
{
    my $strVm = shift;

    return $oyVm->{$strVm}{&VMDEF_COVERAGE_C} ? true : false;
}

push @EXPORT, qw(vmCoverageC);

####################################################################################################################################
# Get vm architecture
####################################################################################################################################
sub vmArch
{
    my $strVm = shift;

    return $oyVm->{$strVm}{&VM_ARCH};
}

push @EXPORT, qw(vmArch);

####################################################################################################################################
# Get vm architecture bits
####################################################################################################################################
sub vmArchBits
{
    my $strVm = shift;

    return (vmArch($strVm) eq VM_ARCH_I386 ? 32 : 64);
}

push @EXPORT, qw(vmArchBits);

####################################################################################################################################
# Get host architecture
####################################################################################################################################
my $strHostArch = undef;

sub hostArch
{
    if (!defined($strHostArch))
    {
        $strHostArch = trim(`uname -m`);

        # Mac M1 reports arm64 but we generally need aarch64 (which Linux reports)
        if ($strHostArch eq 'arm64')
        {
            $strHostArch = 'aarch64';
        }
    }

    return $strHostArch;
}

push @EXPORT, qw(hostArch);

####################################################################################################################################
# Does the VM support liblz4?
####################################################################################################################################
sub vmWithLz4
{
    my $strVm = shift;

    return (defined($oyVm->{$strVm}{&VMDEF_WITH_LZ4}) ? $oyVm->{$strVm}{&VMDEF_WITH_LZ4} : true);
}

push @EXPORT, qw(vmWithLz4);

####################################################################################################################################
# Does the VM support liblzst?
####################################################################################################################################
sub vmWithZst
{
    my $strVm = shift;

    return (defined($oyVm->{$strVm}{&VMDEF_WITH_ZST}) ? $oyVm->{$strVm}{&VMDEF_WITH_ZST} : false);
}

push @EXPORT, qw(vmWithZst);

####################################################################################################################################
# Will integration tests be run in debug mode?
####################################################################################################################################
sub vmDebugIntegration
{
    my $strVm = shift;

    return (defined($oyVm->{$strVm}{&VMDEF_DEBUG_INTEGRATION}) ? $oyVm->{$strVm}{&VMDEF_DEBUG_INTEGRATION} : true);
}

push @EXPORT, qw(vmDebugIntegration);

1;
