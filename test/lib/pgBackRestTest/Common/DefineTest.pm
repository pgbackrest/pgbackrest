####################################################################################################################################
# DefineTest.pm - Defines all tests that can be run
####################################################################################################################################
package pgBackRestTest::Common::DefineTest;

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();

use pgBackRest::Common::Log;
use pgBackRest::Common::String;

################################################################################################################################
# Test definition constants
################################################################################################################################
use constant TESTDEF_MODULE                                         => 'module';
    push @EXPORT, qw(TESTDEF_MODULE);
use constant TESTDEF_MODULE_NAME                                    => 'name';
    push @EXPORT, qw(TESTDEF_MODULE_NAME);

use constant TESTDEF_EXPECT                                         => 'expect';
    push @EXPORT, qw(TESTDEF_EXPECT);
use constant TESTDEF_TEST                                           => 'test';
    push @EXPORT, qw(TESTDEF_TEST);
use constant TESTDEF_TEST_ALL                                       => 'all';
    push @EXPORT, qw(TESTDEF_TEST_ALL);
# Determines coverage for the test
use constant TESTDEF_TEST_COVERAGE                                  => 'coverage';
    push @EXPORT, qw(TESTDEF_TEST_COVERAGE);
# Determines if each run in a test will be run in a new container
use constant TESTDEF_TEST_INDIVIDUAL                                => 'individual';
    push @EXPORT, qw(TESTDEF_TEST_INDIVIDUAL);
use constant TESTDEF_TEST_NAME                                      => 'name';
    push @EXPORT, qw(TESTDEF_TEST_NAME);
use constant TESTDEF_TEST_TOTAL                                     => 'total';
    push @EXPORT, qw(TESTDEF_TEST_TOTAL);
# Determines if the test will be run in a container or will create containers itself
use constant TESTDEF_TEST_CONTAINER                                 => 'container';
    push @EXPORT, qw(TESTDEF_TEST_CONTAINER);
# Determines if the test will be run with multiple processes
use constant TESTDEF_TEST_PROCESS                                   => 'process';
    push @EXPORT, qw(TESTDEF_TEST_PROCESS);
# Determines if the test will be run against multiple db versions
use constant TESTDEF_TEST_DB                                        => 'db';
    push @EXPORT, qw(TESTDEF_TEST_DB);

use constant TESTDEF_COVERAGE_FULL                                  => true;
    push @EXPORT, qw(TESTDEF_COVERAGE_FULL);
use constant TESTDEF_COVERAGE_PARTIAL                               => false;
    push @EXPORT, qw(TESTDEF_COVERAGE_PARTIAL);

use constant TESTDEF_MODULE_FILE                                    => 'File';
    push @EXPORT, qw(TESTDEF_MODULE_FILE);
use constant TESTDEF_MODULE_FILE_COMMON                             => TESTDEF_MODULE_FILE . 'Common';
    push @EXPORT, qw(TESTDEF_MODULE_FILE_COMMON);

use constant TESTDEF_MODULE_ARCHIVE                                 => 'Archive';
    push @EXPORT, qw(TESTDEF_MODULE_ARCHIVE);
use constant TESTDEF_MODULE_ARCHIVE_COMMON                          => TESTDEF_MODULE_ARCHIVE . '/ArchiveCommon';
    push @EXPORT, qw(TESTDEF_MODULE_ARCHIVE_COMMON);
use constant TESTDEF_MODULE_ARCHIVE_PUSH                            => TESTDEF_MODULE_ARCHIVE . '/ArchivePush';
    push @EXPORT, qw(TESTDEF_MODULE_ARCHIVE_PUSH);
use constant TESTDEF_MODULE_ARCHIVE_PUSH_ASYNC                      => TESTDEF_MODULE_ARCHIVE_PUSH . 'Async';
    push @EXPORT, qw(TESTDEF_MODULE_ARCHIVE_PUSH_ASYNC);
use constant TESTDEF_MODULE_ARCHIVE_PUSH_FILE                       => TESTDEF_MODULE_ARCHIVE_PUSH . 'File';
    push @EXPORT, qw(TESTDEF_MODULE_ARCHIVE_PUSH_FILE);

use constant TESTDEF_MODULE_BACKUP_COMMON                           => 'BackupCommon';
    push @EXPORT, qw(TESTDEF_MODULE_BACKUP_COMMON);

use constant TESTDEF_MODULE_INFO                                    => 'Info';
    push @EXPORT, qw(TESTDEF_MODULE_INFO);

use constant TESTDEF_MODULE_STANZA                                  => 'Stanza';
    push @EXPORT, qw(TESTDEF_MODULE_STANZA);

use constant TESTDEF_MODULE_EXPIRE                                  => 'Expire';
    push @EXPORT, qw(TESTDEF_MODULE_EXPIRE);

################################################################################################################################
# Define tests
################################################################################################################################
my $oTestDef =
{
    &TESTDEF_MODULE =>
    [
        # Help tests
        {
            &TESTDEF_MODULE_NAME => 'help',
            &TESTDEF_TEST_CONTAINER => true,
            &TESTDEF_EXPECT => true,

            &TESTDEF_TEST =>
            [
                {
                    &TESTDEF_TEST_NAME => 'help',
                    &TESTDEF_TEST_TOTAL => 1,
                    &TESTDEF_TEST_INDIVIDUAL => false,
                }
            ]
        },
        # Config tests
        {
            &TESTDEF_MODULE_NAME => 'config',
            &TESTDEF_TEST_CONTAINER => true,

            &TESTDEF_TEST =>
            [
                {
                    &TESTDEF_TEST_NAME => 'unit',
                    &TESTDEF_TEST_TOTAL => 7,
                    &TESTDEF_TEST_INDIVIDUAL => false,
                },
                {
                    &TESTDEF_TEST_NAME => 'option',
                    &TESTDEF_TEST_TOTAL => 34,
                    &TESTDEF_TEST_INDIVIDUAL => false,
                },
                {
                    &TESTDEF_TEST_NAME => 'config',
                    &TESTDEF_TEST_TOTAL => 25,
                    &TESTDEF_TEST_INDIVIDUAL => false,
                }
            ]
        },
        # File tests
        {
            &TESTDEF_MODULE_NAME => 'file',
            &TESTDEF_TEST_CONTAINER => true,

            &TESTDEF_TEST_COVERAGE =>
            {
                &TESTDEF_MODULE_FILE => TESTDEF_COVERAGE_FULL,
                &TESTDEF_MODULE_FILE_COMMON => TESTDEF_COVERAGE_FULL,
            },

            &TESTDEF_TEST =>
            [
                {
                    &TESTDEF_TEST_NAME => 'unit',
                    &TESTDEF_TEST_TOTAL => 1,
                    &TESTDEF_TEST_INDIVIDUAL => false,
                },
                {
                    &TESTDEF_TEST_NAME => 'owner',
                    &TESTDEF_TEST_TOTAL => 8,
                    &TESTDEF_TEST_INDIVIDUAL => false,
                },
                {
                    &TESTDEF_TEST_NAME => 'path-create',
                    &TESTDEF_TEST_TOTAL => 8,
                    &TESTDEF_TEST_INDIVIDUAL => false,
                },
                {
                    &TESTDEF_TEST_NAME => 'move',
                    &TESTDEF_TEST_TOTAL => 24,
                    &TESTDEF_TEST_INDIVIDUAL => false,
                },
                {
                    &TESTDEF_TEST_NAME => 'compress',
                    &TESTDEF_TEST_TOTAL => 4,
                    &TESTDEF_TEST_INDIVIDUAL => false,
                },
                {
                    &TESTDEF_TEST_NAME => 'wait',
                    &TESTDEF_TEST_TOTAL => 2,
                    &TESTDEF_TEST_INDIVIDUAL => false,
                },
                {
                    &TESTDEF_TEST_NAME => 'link',
                    &TESTDEF_TEST_TOTAL => 1,
                    &TESTDEF_TEST_INDIVIDUAL => false,
                },
                {
                    &TESTDEF_TEST_NAME => 'stat',
                    &TESTDEF_TEST_TOTAL => 1,
                    &TESTDEF_TEST_INDIVIDUAL => false,
                },
                {
                    &TESTDEF_TEST_NAME => 'manifest',
                    &TESTDEF_TEST_TOTAL => 5,
                    &TESTDEF_TEST_INDIVIDUAL => false,
                },
                {
                    &TESTDEF_TEST_NAME => 'list',
                    &TESTDEF_TEST_TOTAL => 72,
                    &TESTDEF_TEST_INDIVIDUAL => false,
                },
                {
                    &TESTDEF_TEST_NAME => 'remove',
                    &TESTDEF_TEST_TOTAL => 32,
                    &TESTDEF_TEST_INDIVIDUAL => false,
                },
                {
                    &TESTDEF_TEST_NAME => 'hash',
                    &TESTDEF_TEST_TOTAL => 16,
                    &TESTDEF_TEST_INDIVIDUAL => false,
                },
                {
                    &TESTDEF_TEST_NAME => 'exists',
                    &TESTDEF_TEST_TOTAL => 6,
                    &TESTDEF_TEST_INDIVIDUAL => false,
                },
                {
                    &TESTDEF_TEST_NAME => 'copy',
                    &TESTDEF_TEST_TOTAL => 144,
                    &TESTDEF_TEST_INDIVIDUAL => false,
                }
            ]
        },
        # Stanza tests
        {
            &TESTDEF_MODULE_NAME => 'stanza',
            &TESTDEF_TEST_CONTAINER => false,
            &TESTDEF_EXPECT => true,

            &TESTDEF_TEST_COVERAGE =>
            {
                &TESTDEF_MODULE_STANZA => TESTDEF_COVERAGE_FULL,
            },

            &TESTDEF_TEST =>
            [
                {
                    &TESTDEF_TEST_NAME => 'unit',
                    &TESTDEF_EXPECT => false,
                    &TESTDEF_TEST_TOTAL => 2,
                },
                {
                    &TESTDEF_TEST_NAME => 'create',
                    &TESTDEF_TEST_TOTAL => 2
                },
                {
                    &TESTDEF_TEST_NAME => 'upgrade',
                    &TESTDEF_TEST_TOTAL => 2
                },
            ]
        },
        # Archive tests
        {
            &TESTDEF_MODULE_NAME => 'archive',
            &TESTDEF_TEST_CONTAINER => false,
            &TESTDEF_EXPECT => true,

            &TESTDEF_TEST =>
            [
                {
                    &TESTDEF_TEST_NAME => 'unit',
                    &TESTDEF_TEST_TOTAL => 4,
                    &TESTDEF_TEST_CONTAINER => true,
                    &TESTDEF_TEST_INDIVIDUAL => false,
                    &TESTDEF_EXPECT => false,

                    &TESTDEF_TEST_COVERAGE =>
                    {
                        &TESTDEF_TEST_ALL =>
                        {
                            &TESTDEF_MODULE_ARCHIVE_COMMON => TESTDEF_COVERAGE_PARTIAL,
                        }
                    },
                },
                {
                    &TESTDEF_TEST_NAME => 'push-unit',
                    &TESTDEF_TEST_TOTAL => 7,
                    &TESTDEF_TEST_CONTAINER => true,
                    &TESTDEF_TEST_INDIVIDUAL => false,
                    &TESTDEF_EXPECT => false,

                    &TESTDEF_TEST_COVERAGE =>
                    {
                        &TESTDEF_TEST_ALL =>
                        {
                            &TESTDEF_MODULE_ARCHIVE_PUSH => TESTDEF_COVERAGE_FULL,
                            &TESTDEF_MODULE_ARCHIVE_PUSH_ASYNC => TESTDEF_COVERAGE_FULL,
                            &TESTDEF_MODULE_ARCHIVE_PUSH_FILE => TESTDEF_COVERAGE_PARTIAL,
                        }
                    },
                },
                {
                    &TESTDEF_TEST_NAME => 'push',
                    &TESTDEF_TEST_TOTAL => 8,
                    &TESTDEF_TEST_PROCESS => true,
                },
                {
                    &TESTDEF_TEST_NAME => 'stop',
                    &TESTDEF_TEST_TOTAL => 6
                },
                {
                    &TESTDEF_TEST_NAME => 'get',
                    &TESTDEF_TEST_TOTAL => 8
                },
            ]
        },
        # Backup tests
        {
            &TESTDEF_MODULE_NAME => 'backup',
            &TESTDEF_TEST_CONTAINER => false,
            &TESTDEF_EXPECT => false,

            &TESTDEF_TEST_COVERAGE =>
            {
                &TESTDEF_MODULE_BACKUP_COMMON => TESTDEF_COVERAGE_FULL,
            },

            &TESTDEF_TEST =>
            [
                {
                    &TESTDEF_TEST_NAME => 'unit',
                    &TESTDEF_TEST_TOTAL => 3,
                    &TESTDEF_TEST_INDIVIDUAL => false,
                },
            ]
        },
        # Expire tests
        {
            &TESTDEF_MODULE_NAME => 'expire',
            &TESTDEF_TEST_CONTAINER => false,
            &TESTDEF_EXPECT => true,

            &TESTDEF_TEST_COVERAGE =>
            {
                &TESTDEF_MODULE_EXPIRE => TESTDEF_COVERAGE_FULL,
            },

            &TESTDEF_TEST =>
            [
                {
                    &TESTDEF_TEST_NAME => 'expire',
                    &TESTDEF_TEST_TOTAL => 2,
                },
            ]
        },
        # Info tests
        {
            &TESTDEF_MODULE_NAME => 'info',
            &TESTDEF_TEST_CONTAINER => false,
            &TESTDEF_EXPECT => true,

            &TESTDEF_TEST =>
            [
                {
                    &TESTDEF_TEST_NAME => 'unit',
                    &TESTDEF_TEST_CONTAINER => true,
                    &TESTDEF_TEST_INDIVIDUAL => false,
                    &TESTDEF_EXPECT => false,
                    &TESTDEF_TEST_TOTAL => 1,

                    &TESTDEF_TEST_COVERAGE =>
                    {
                        &TESTDEF_TEST_ALL =>
                        {
                            &TESTDEF_MODULE_INFO => TESTDEF_COVERAGE_FULL,
                        }
                    },
                },
            ]
        },
        # Full tests
        {
            &TESTDEF_MODULE_NAME => 'full',
            &TESTDEF_TEST_CONTAINER => false,
            &TESTDEF_EXPECT => true,

            &TESTDEF_TEST =>
            [
                {
                    &TESTDEF_TEST_NAME => 'synthetic',
                    &TESTDEF_TEST_TOTAL => 8,
                    &TESTDEF_TEST_PROCESS => true
                },
                {
                    &TESTDEF_TEST_NAME => 'real',
                    &TESTDEF_TEST_TOTAL => 11,
                    &TESTDEF_TEST_PROCESS => true,
                    &TESTDEF_TEST_DB => true
                }
            ]
        },
    ]
};

####################################################################################################################################
# Process normalized data into a more queryable form
####################################################################################################################################
my $hTestDefHash;                                                   # An easier way to query hash version of the above
my @stryModule;                                                     # Ordered list of modules
my $hModuleTest;                                                    # Ordered list of tests for each module

# Iterate each module
foreach my $hModule (@{$oTestDef->{&TESTDEF_MODULE}})
{
    # Push the module onto the ordered list
    my $strModule = $hModule->{&TESTDEF_MODULE_NAME};
    push(@stryModule, $strModule);

    # Iterate each test
    my @stryModuleTest;

    foreach my $hModuleTest (@{$hModule->{&TESTDEF_TEST}})
    {
        # Push the test on the order list
        my $strTest = $hModuleTest->{&TESTDEF_TEST_NAME};
        push(@stryModuleTest, $strTest);

        # Resolve variables that can be set in the module or the test
        foreach my $strVar (
            TESTDEF_TEST_CONTAINER, TESTDEF_EXPECT, TESTDEF_TEST_PROCESS, TESTDEF_TEST_DB, TESTDEF_TEST_INDIVIDUAL)
        {
            $hTestDefHash->{$strModule}{$strTest}{$strVar} = coalesce(
                $hModuleTest->{$strVar}, coalesce($hModule->{$strVar}, false));
        }

        # Set test count
        $hTestDefHash->{$strModule}{$strTest}{&TESTDEF_TEST_TOTAL} = $hModuleTest->{&TESTDEF_TEST_TOTAL};

        # Iterate each run
        for (my $iRun = 1; $iRun <= $hModuleTest->{&TESTDEF_TEST_TOTAL}; $iRun++)
        {
        }
    }

    $hModuleTest->{$strModule} = \@stryModuleTest;
}

use Data::Dumper; confess Dumper($hTestDefHash);

####################################################################################################################################
# testDefGet
####################################################################################################################################
sub testDefGet
{
    return $oTestDef;
}

push @EXPORT, qw(testDefGet);

####################################################################################################################################
# testDefModuleGet
####################################################################################################################################
sub testDefModuleGet
{
    my $strModule = shift;

    # Find the module
    foreach my $hModule (@{$oTestDef->{&TESTDEF_MODULE}})
    {
        if ($hModule->{&TESTDEF_MODULE_NAME} eq $strModule)
        {
            return $hModule;
        }
    }

    confess &log(ASSERT, "unable to find module ${strModule}");
}

push @EXPORT, qw(testDefModuleGet);

####################################################################################################################################
# testDefModuleTestGet
####################################################################################################################################
sub testDefModuleTestGet
{
    my $hModule = shift;
    my $strModuleTest = shift;

    foreach my $hModuleTest (@{$hModule->{&TESTDEF_TEST}})
    {
        if ($hModuleTest->{&TESTDEF_TEST_NAME} eq $strModuleTest)
        {
            return $hModuleTest;
        }
    }

    confess &log(ASSERT, "unable to find module test ${strModuleTest}");
}

push @EXPORT, qw(testDefModuleTestGet);

1;
