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

use pgBackRest::Common::Log;

use pgBackRestTest::Common::DefineTest;
use pgBackRestTest::Common::VmTest;

################################################################################################################################
# Test constants
################################################################################################################################
use constant TEST_DB                                                => 'db';
    push @EXPORT, qw(TEST_DB);
use constant TEST_CONTAINER                                         => 'container';
    push @EXPORT, qw(TEST_CONTAINER);
use constant TEST_MODULE                                            => 'module';
    push @EXPORT, qw(TEST_MODULE);
use constant TEST_NAME                                              => 'test';
    push @EXPORT, qw(TEST_NAME);
use constant TEST_PGSQL_BIN                                         => 'pgsql-bin';
    push @EXPORT, qw(TEST_PGSQL_BIN);
use constant TEST_RUN                                               => 'run';
    push @EXPORT, qw(TEST_RUN);
use constant TEST_PROCESS                                           => 'process';
    push @EXPORT, qw(TEST_PROCESS);
use constant TEST_VM                                                => 'os';
    push @EXPORT, qw(TEST_VM);
use constant TEST_PERL_ARCH_PATH                                    => VMDEF_PERL_ARCH_PATH;
    push @EXPORT, qw(TEST_PERL_ARCH_PATH);

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
    my $iProcessMax = shift;
    my $bCoverage = shift;

    my $oTestDef = testDefGet();
    my $oyVm = vmGet();
    my $oyTestRun = [];

    if ($strVm ne 'all' && !defined($${oyVm}{$strVm}))
    {
        confess &log(ERROR, "${strVm} is not a valid VM");
    }

    my $stryTestOS = [];

    if ($strVm eq 'all')
    {
        $stryTestOS = [VM_CO6, VM_U16, VM_D8, VM_CO7, VM_U14, VM_U12];
    }
    else
    {
        $stryTestOS = [$strVm];
    }

    foreach my $strTestOS (@{$stryTestOS})
    {
        foreach my $oModule (@{$$oTestDef{&TESTDEF_MODULE}})
        {
            if (@{$stryModule} == 0 || grep(/^$$oModule{&TESTDEF_MODULE_NAME}$/i, @{$stryModule}))
            {
                foreach my $oTest (@{$$oModule{test}})
                {
                    if (@{$stryModuleTest} == 0 || grep(/^$$oTest{&TESTDEF_TEST_NAME}$/i, @{$stryModuleTest}))
                    {
                        my $iDbVersionMin = -1;
                        my $iDbVersionMax = -1;

                        # By default test every db version that is supported for each OS
                        my $strDbVersionKey = 'db';

                        # Run a reduced set of tests where each PG version is only tested on a single OS
                        if ($strDbVersion eq 'minimal')
                        {
                            $strDbVersionKey = &VM_DB_MINIMAL;
                        }

                        if (defined($$oTest{&TESTDEF_TEST_DB}) && $$oTest{&TESTDEF_TEST_DB})
                        {
                            $iDbVersionMin = 0;
                            $iDbVersionMax = @{$$oyVm{$strTestOS}{$strDbVersionKey}} - 1;
                        }

                        my $bFirstDbVersion = true;

                        for (my $iDbVersionIdx = $iDbVersionMax; $iDbVersionIdx >= $iDbVersionMin; $iDbVersionIdx--)
                        {
                            if ($iDbVersionIdx == -1 || $strDbVersion eq 'all' || $strDbVersion eq 'minimal' ||
                                ($strDbVersion ne 'all' &&
                                    $strDbVersion eq ${$$oyVm{$strTestOS}{$strDbVersionKey}}[$iDbVersionIdx]))
                            {
                                # Individual tests will be each be run in a separate container.  This is the default.
                                my $bTestIndividual =
                                    !defined($$oTest{&TESTDEF_TEST_INDIVIDUAL}) || $$oTest{&TESTDEF_TEST_INDIVIDUAL} ? true : false;

                                my $iTestRunMin = $bTestIndividual ? 1 : -1;
                                my $iTestRunMax = $bTestIndividual ? $$oTest{&TESTDEF_TEST_TOTAL} : -1;

                                for (my $iTestRunIdx = $iTestRunMin; $iTestRunIdx <= $iTestRunMax; $iTestRunIdx++)
                                {
                                    # Skip this run if a list was provided and this test is not in the list
                                    next if (
                                        $bTestIndividual && @{$iyModuleTestRun} != 0 &&
                                            !grep(/^$iTestRunIdx$/i, @{$iyModuleTestRun}));

                                    # Skip this run if coverage is requested and this test does not provide coverage
                                    next if (
                                        $bCoverage &&
                                        (($bTestIndividual && !defined($oTest->{&TESTDEF_TEST_COVERAGE}{$iTestRunIdx})) ||
                                         (!$bTestIndividual && !defined($oTest->{&TESTDEF_TEST_COVERAGE}{&TESTDEF_TEST_ALL}))) &&
                                        !defined($oModule->{&TESTDEF_TEST_COVERAGE}));

                                    my $iyProcessMax = [defined($iProcessMax) ? $iProcessMax : 1];

                                    if (defined($$oTest{&TESTDEF_TEST_PROCESS}) && $$oTest{&TESTDEF_TEST_PROCESS} &&
                                        !defined($iProcessMax) && $bFirstDbVersion)
                                    {
                                        $iyProcessMax = [1, 4];
                                    }

                                    foreach my $iProcessTestMax (@{$iyProcessMax})
                                    {
                                        my $strDbVersion = $iDbVersionIdx == -1 ? undef :
                                                               ${$$oyVm{$strTestOS}{$strDbVersionKey}}[$iDbVersionIdx];

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
                                            &TEST_VM => $strTestOS,
                                            &TEST_CONTAINER => $$oModule{&TESTDEF_TEST_CONTAINER},
                                            &TEST_PGSQL_BIN => $strPgSqlBin,
                                            &TEST_PERL_ARCH_PATH => $$oyVm{$strTestOS}{&VMDEF_PERL_ARCH_PATH},
                                            &TEST_MODULE => $$oModule{&TESTDEF_MODULE_NAME},
                                            &TEST_NAME => $$oTest{&TESTDEF_TEST_NAME},
                                            &TEST_RUN =>
                                                $iTestRunIdx == -1 ? (@{$iyModuleTestRun} == 0 ? undef : $iyModuleTestRun) :
                                                    [$iTestRunIdx],
                                            &TEST_PROCESS => $iProcessTestMax,
                                            &TEST_DB => $strDbVersion
                                        };

                                        push(@{$oyTestRun}, $oTestRun);
                                    }
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
