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
use constant VM_OS_REPO                                             => 'os-repo';
    push @EXPORT, qw(VM_OS_REPO);
use constant VMDEF_PGSQL_BIN                                        => 'pgsql-bin';
    push @EXPORT, qw(VMDEF_PGSQL_BIN);
use constant VMDEF_LCOV_VERSION                                     => 'lcov-version';
    push @EXPORT, qw(VMDEF_LCOV_VERSION);
use constant VMDEF_WITH_BACKTRACE                                   => 'with-backtrace';
    push @EXPORT, qw(VMDEF_WITH_BACKTRACE);
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

use constant VM_RH7                                                 => 'rh7';
    push @EXPORT, qw(VM_RH7);
use constant VM_RH8                                                 => 'rh8';
    push @EXPORT, qw(VM_RH8);
use constant VM_F33                                                 => 'f33';
    push @EXPORT, qw(VM_F33);
use constant VM_U18                                                 => 'u18';
    push @EXPORT, qw(VM_U18);
use constant VM_U20                                                 => 'u20';
    push @EXPORT, qw(VM_U20);
use constant VM_D9                                                  => 'd9';
    push @EXPORT, qw(VM_D9);

# Defines the vm that will be used for expect testing
use constant VM_EXPECT                                              => VM_RH7;
    push @EXPORT, qw(VM_EXPECT);

# VM aliases for run matrices (numbered oldest to newest)
use constant VM2                                                    => VM_D9;
    push @EXPORT, qw(VM2);
use constant VM3                                                    => VM_RH7;
    push @EXPORT, qw(VM3);
use constant VM4                                                    => VM_U20;
    push @EXPORT, qw(VM4);

# List of default test VMs
use constant VM_LIST                                                => (VM2, VM3, VM4);
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

    # RHEL 7
    &VM_RH7 =>
    {
        &VM_OS_BASE => VM_OS_BASE_RHEL,
        &VM_IMAGE => 'centos:7',
        &VM_ARCH => VM_ARCH_AMD64,
        &VMDEF_PGSQL_BIN => '/usr/pgsql-{[version]}/bin',

        &VMDEF_DEBUG_INTEGRATION => false,
        &VMDEF_WITH_ZST => true,

        &VM_DB =>
        [
            PG_VERSION_95,
            PG_VERSION_96,
            PG_VERSION_10,
            PG_VERSION_11,
            PG_VERSION_12,
            PG_VERSION_13,
        ],

        &VM_DB_TEST =>
        [
            PG_VERSION_96,
        ],
    },

    # Fedora 33
    &VM_F33 =>
    {
        &VM_OS_BASE => VM_OS_BASE_RHEL,
        &VM_IMAGE => 'fedora:33',
        &VM_ARCH => VM_ARCH_AMD64,
        &VMDEF_PGSQL_BIN => '/usr/pgsql-{[version]}/bin',
        &VMDEF_COVERAGE_C => true,

        &VMDEF_DEBUG_INTEGRATION => false,
        &VMDEF_WITH_ZST => true,

        &VM_DB =>
        [
            PG_VERSION_96,
            PG_VERSION_10,
            PG_VERSION_11,
            PG_VERSION_12,
            PG_VERSION_13,
        ],

        &VM_DB_TEST =>
        [
            PG_VERSION_12,
        ],
    },

    # Debian 9
    &VM_D9 =>
    {
        &VM_OS_BASE => VM_OS_BASE_DEBIAN,
        &VM_OS_REPO => 'stretch',
        &VM_IMAGE => 'i386/debian:9',
        &VM_ARCH => VM_ARCH_I386,
        &VMDEF_PGSQL_BIN => '/usr/lib/postgresql/{[version]}/bin',

        &VM_DB =>
        [
            PG_VERSION_83,
            PG_VERSION_84,
            PG_VERSION_90,
            PG_VERSION_91,
            PG_VERSION_92,
        ],

        &VM_DB_TEST =>
        [
            PG_VERSION_83,
            PG_VERSION_84,
            PG_VERSION_90,
        ],
    },

    # Ubuntu 18.04
    &VM_U18 =>
    {
        &VM_OS_BASE => VM_OS_BASE_DEBIAN,
        &VM_OS_REPO => 'bionic',
        &VM_IMAGE => 'ubuntu:18.04',
        &VM_ARCH => VM_ARCH_AMD64,
        &VMDEF_COVERAGE_C => true,
        &VMDEF_PGSQL_BIN => '/usr/lib/postgresql/{[version]}/bin',

        &VMDEF_WITH_BACKTRACE => true,
        &VMDEF_WITH_ZST => true,

        &VM_DB =>
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
            PG_VERSION_10,
            PG_VERSION_11,
            PG_VERSION_12,
            PG_VERSION_13,
        ],

        &VM_DB_TEST =>
        [
            PG_VERSION_93,
            PG_VERSION_94,
            PG_VERSION_95,
            PG_VERSION_10,
            PG_VERSION_11,
            PG_VERSION_12,
            PG_VERSION_13,
        ],
    },

    # Ubuntu 20.04
    &VM_U20 =>
    {
        &VM_OS_BASE => VM_OS_BASE_DEBIAN,
        &VM_OS_REPO => 'focal',
        &VM_IMAGE => 'ubuntu:20.04',
        &VM_ARCH => VM_ARCH_AMD64,
        &VMDEF_COVERAGE_C => true,
        &VMDEF_PGSQL_BIN => '/usr/lib/postgresql/{[version]}/bin',

        &VMDEF_WITH_BACKTRACE => true,
        &VMDEF_WITH_ZST => true,

        &VM_DB =>
        [
            PG_VERSION_90,
            PG_VERSION_91,
            PG_VERSION_92,
            PG_VERSION_93,
            PG_VERSION_94,
            PG_VERSION_95,
            PG_VERSION_96,
            PG_VERSION_10,
            PG_VERSION_11,
            PG_VERSION_12,
            PG_VERSION_13,
            PG_VERSION_14,
        ],

        &VM_DB_TEST =>
        [
            PG_VERSION_91,
            PG_VERSION_92,
            PG_VERSION_93,
            PG_VERSION_94,
            PG_VERSION_95,
            PG_VERSION_10,
            PG_VERSION_11,
            PG_VERSION_12,
            PG_VERSION_13,
            PG_VERSION_14,
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
# Which vm to use for the test matrix.  If one of the standard four, then use that, else use VM4.
####################################################################################################################################
sub vmTest
{
    my $strVm = shift;

    if (grep(/^$strVm$/, VM_LIST))
    {
        return $strVm;
    }

    return VM4;
}

push @EXPORT, qw(vmTest);

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
# Get vm architecture bits
####################################################################################################################################
sub vmArchBits
{
    my $strVm = shift;

    return ($oyVm->{$strVm}{&VM_ARCH} eq VM_ARCH_AMD64 ? 64 : 32);
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
# Does the VM support libbacktrace?
####################################################################################################################################
sub vmWithBackTrace
{
    my $strVm = shift;

    return ($oyVm->{$strVm}{&VMDEF_WITH_BACKTRACE} ? true : false);
}

push @EXPORT, qw(vmWithBackTrace);

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
