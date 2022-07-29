####################################################################################################################################
# ListTest.pm - Creates a list of tests to be run based on input criteria
####################################################################################################################################
package pgBackRestTest::Common::ListTest;

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();

use pgBackRestDoc::Common::Log;
use pgBackRestDoc::Common::String;

use pgBackRestTest::Common::DefineTest;
use pgBackRestTest::Common::VmTest;

################################################################################################################################
# Test constants
################################################################################################################################
use constant TEST_DB                                                => 'db';
    push @EXPORT, qw(TEST_DB);
use constant TEST_C                                                 => 'c';
    push @EXPORT, qw(TEST_C);
use constant TEST_CONTAINER                                         => 'container';
    push @EXPORT, qw(TEST_CONTAINER);
use constant TEST_MODULE                                            => 'module';
    push @EXPORT, qw(TEST_MODULE);
use constant TEST_NAME                                              => 'test';
    push @EXPORT, qw(TEST_NAME);
use constant TEST_BIN_REQ                                           => 'bin-req';
    push @EXPORT, qw(TEST_BIN_REQ);
use constant TEST_PGSQL_BIN                                         => 'pgsql-bin';
    push @EXPORT, qw(TEST_PGSQL_BIN);
use constant TEST_INTEGRATION                                       => 'integration';
    push @EXPORT, qw(TEST_INTEGRATION);
use constant TEST_TYPE                                              => 'type';
    push @EXPORT, qw(TEST_TYPE);
use constant TEST_RUN                                               => 'run';
    push @EXPORT, qw(TEST_RUN);
use constant TEST_VM                                                => 'os';
    push @EXPORT, qw(TEST_VM);

####################################################################################################################################
# testListGet
####################################################################################################################################
sub testListGet
{
    my $strVm = shift;
    my $stryModule = shift;
    my $stryModuleTest = shift;
    my $iyModuleTestRun = shift;
    my $strDbVersion = shift;
    my $bCoverageOnly = shift;
    my $bCOnly = shift;
    my $bContainerOnly = shift;
    my $bNoPerformance = shift;

    my $oyVm = vmGet();
    my $oyTestRun = [];

    if ($strVm ne 'all' && !defined($${oyVm}{$strVm}))
    {
        confess &log(ERROR, "${strVm} is not a valid VM");
    }

    my @stryTestOS = VM_LIST;

    if ($strVm ne 'all')
    {
        @stryTestOS = ($strVm);
    }

    foreach my $strTestOS (@stryTestOS)
    {
        foreach my $strModule (testDefModuleList())
        {
            my $hModule = testDefModule($strModule);

            if (@{$stryModule} == 0 || grep(/^$strModule$/i, @{$stryModule}))
            {
                foreach my $strModuleTest (testDefModuleTestList($strModule))
                {
                    my $hTest = testDefModuleTest($strModule, $strModuleTest);

                    if (@{$stryModuleTest} == 0 || grep(/^$strModuleTest$/i, @{$stryModuleTest}))
                    {
                        my $iDbVersionMin = -1;
                        my $iDbVersionMax = -1;

                        # Database versions to test
                        if (defined($hTest->{&TESTDEF_DB}) && $hTest->{&TESTDEF_DB})
                        {
                            $iDbVersionMin = 0;
                            $iDbVersionMax = @{$$oyVm{$strTestOS}{&VM_DB_TEST}} - 1;
                        }

                        my $bFirstDbVersion = true;

                        # Skip this test if it can't run on this VM
                        next if (defined($hTest->{&TESTDEF_VM}) && grep(/^$strTestOS$/i, @{$hTest->{&TESTDEF_VM}}) == 0);

                        # Skip this test if only C tests are requested and this is not a C test
                        next if ($bCOnly && !$hTest->{&TESTDEF_C});

                        # Skip this test if it is integration and vm=none
                        next if ($strVm eq VM_NONE && $hTest->{&TESTDEF_TYPE} eq TESTDEF_INTEGRATION);

                        # Skip this test if it is performance and no performance is specified
                        next if ($bNoPerformance && $hTest->{&TESTDEF_TYPE} eq TESTDEF_PERFORMANCE);

                        # Skip this test if it is not C and vm=none.  Perl tests require Docker which is not supported.
                        next if ($strVm eq VM_NONE && !$hTest->{&TESTDEF_C});

                        # Skip this test if a container is required and vm=none.
                        next if ($strVm eq VM_NONE && $hTest->{&TESTDEF_CONTAINER_REQUIRED});

                        # Skip this if it does not require a container and container only tests are required.
                        next if ($bContainerOnly && $hTest->{&TESTDEF_C} && !$hTest->{&TESTDEF_CONTAINER_REQUIRED});

                        for (my $iDbVersionIdx = $iDbVersionMax; $iDbVersionIdx >= $iDbVersionMin; $iDbVersionIdx--)
                        {
                            if ($iDbVersionIdx == -1 || $strDbVersion eq 'all' || $strDbVersion eq 'minimal' ||
                                ($strDbVersion ne 'all' &&
                                    $strDbVersion eq ${$$oyVm{$strTestOS}{&VM_DB_TEST}}[$iDbVersionIdx]))
                            {
                                # Individual tests will be each be run in a separate container.  This is the default.
                                my $bTestIndividual =
                                    !defined($hTest->{&TESTDEF_INDIVIDUAL}) || $hTest->{&TESTDEF_INDIVIDUAL} ? true : false;

                                my $iTestRunMin = $bTestIndividual ? 1 : -1;
                                my $iTestRunMax = $bTestIndividual ? $hTest->{&TESTDEF_TOTAL} : -1;

                                for (my $iTestRunIdx = $iTestRunMin; $iTestRunIdx <= $iTestRunMax; $iTestRunIdx++)
                                {
                                    # Skip this run if a list was provided and this test is not in the list
                                    next if (
                                        $bTestIndividual && @{$iyModuleTestRun} != 0 &&
                                            !grep(/^$iTestRunIdx$/i, @{$iyModuleTestRun}));

                                    # Skip this run if only coverage tests are requested and this test does not provide coverage
                                    next if ($bCoverageOnly && !defined($hTest->{&TESTDEF_COVERAGE}));

                                    my $strDbVersion = $iDbVersionIdx == -1 ? undef :
                                                           ${$$oyVm{$strTestOS}{&VM_DB_TEST}}[$iDbVersionIdx];

                                    my $strPgSqlBin = $$oyVm{$strTestOS}{&VMDEF_PGSQL_BIN};

                                    if (defined($strDbVersion))
                                    {
                                        $strPgSqlBin =~ s/\{\[version\]\}/$strDbVersion/g;
                                    }
                                    else
                                    {
                                        $strPgSqlBin =~ s/\{\[version\]\}/9\.4/g;
                                    }

                                    my $oTestRun =
                                    {
                                        &TEST_TYPE => $hTest->{&TESTDEF_TYPE},
                                        &TEST_VM => $strTestOS,
                                        &TEST_C => coalesce($hTest->{&TESTDEF_C}, $hModule->{&TESTDEF_C}, false),
                                        &TEST_CONTAINER => defined($hTest->{&TESTDEF_CONTAINER}) ?
                                            $hTest->{&TESTDEF_CONTAINER} : $hModule->{&TESTDEF_CONTAINER},
                                        &TEST_PGSQL_BIN => $strPgSqlBin,
                                        &TEST_BIN_REQ => $hTest->{&TESTDEF_BIN_REQ},
                                        &TEST_INTEGRATION => $hTest->{&TESTDEF_INTEGRATION},
                                        &TEST_MODULE => $strModule,
                                        &TEST_NAME => $strModuleTest,
                                        &TEST_RUN =>
                                            $iTestRunIdx == -1 ? (@{$iyModuleTestRun} == 0 ? undef : $iyModuleTestRun) :
                                                [$iTestRunIdx],
                                        &TEST_DB => $strDbVersion
                                    };

                                    push(@{$oyTestRun}, $oTestRun);
                                }

                                $bFirstDbVersion = false;
                            }
                        }
                    }
                }
            }
        }
    }

    return $oyTestRun;
}

push @EXPORT, qw(testListGet);

1;
