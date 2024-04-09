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

use pgBackRestDoc::Common::Log;
use pgBackRestDoc::Common::String;

use pgBackRestTest::Common::VmTest;

################################################################################################################################
# Test definition constants
#
# Documentation for these constants is in test/define.yaml.
################################################################################################################################
use constant TESTDEF_INTEGRATION                                    => 'integration';
    push @EXPORT, qw(TESTDEF_INTEGRATION);
use constant TESTDEF_PERFORMANCE                                    => 'performance';
    push @EXPORT, qw(TESTDEF_PERFORMANCE);
use constant TESTDEF_UNIT                                           => 'unit';
    push @EXPORT, qw(TESTDEF_UNIT);

use constant TESTDEF_MODULE                                         => 'module';
    push @EXPORT, qw(TESTDEF_MODULE);
use constant TESTDEF_NAME                                           => 'name';
    push @EXPORT, qw(TESTDEF_NAME);
use constant TESTDEF_TEST                                           => 'test';
    push @EXPORT, qw(TESTDEF_TEST);

use constant TESTDEF_DB                                             => 'db';
    push @EXPORT, qw(TESTDEF_DB);
use constant TESTDEF_CONTAINER                                      => 'container';
    push @EXPORT, qw(TESTDEF_CONTAINER);
use constant TESTDEF_CONTAINER_REQUIRED                             => 'containerReq';
    push @EXPORT, qw(TESTDEF_CONTAINER_REQUIRED);
use constant TESTDEF_COVERAGE                                       => 'coverage';
    push @EXPORT, qw(TESTDEF_COVERAGE);
use constant TESTDEF_C                                              => 'c';
    push @EXPORT, qw(TESTDEF_C);
use constant TESTDEF_INCLUDE                                        => 'include';
    push @EXPORT, qw(TESTDEF_INCLUDE);
use constant TESTDEF_INDIVIDUAL                                     => 'individual';
    push @EXPORT, qw(TESTDEF_INDIVIDUAL);
use constant TESTDEF_TOTAL                                          => 'total';
    push @EXPORT, qw(TESTDEF_TOTAL);
use constant TESTDEF_TYPE                                           => 'type';
    push @EXPORT, qw(TESTDEF_TYPE);
use constant TESTDEF_BIN_REQ                                        => 'binReq';
    push @EXPORT, qw(TESTDEF_BIN_REQ);
use constant TESTDEF_VM                                             => 'vm';
    push @EXPORT, qw(TESTDEF_VM);

use constant TESTDEF_COVERAGE_FULL                                  => 'full';
    push @EXPORT, qw(TESTDEF_COVERAGE_FULL);
use constant TESTDEF_COVERAGE_NOCODE                                => 'noCode';
    push @EXPORT, qw(TESTDEF_COVERAGE_NOCODE);

####################################################################################################################################
# Process normalized data into a more queryable form
####################################################################################################################################
my $hTestDefHash;                                                   # An easier way to query hash version of the above
my @stryModule;                                                     # Ordered list of modules
my $hModuleTest;                                                    # Ordered list of tests for each module
my $hCoverageType;                                                  # Coverage type for each code module (full/partial)
my $hCoverageList;                                                  # Tests required for full code module coverage (if type full)

sub testDefLoad
{
    my $strDefineYaml = shift;

    # Load test definitions from yaml
    require YAML::XS;
    YAML::XS->import(qw(Load));

    my $hTestDef = Load($strDefineYaml);

    # Keep a list of all harnesses added so far. These will make up the harness list for subsequent tests.
    my @rhyHarnessFile = ();

    # Keep a list of all modules added for coverage so far. These will make up the core list for subsequent tests.
    my @stryCoreFile = ();

    # Keep a list of modules that are test before this one so we know what is available
    my $strTestDefine = '';

    # Iterate each test type
    foreach my $strModuleType (TESTDEF_UNIT, TESTDEF_INTEGRATION, TESTDEF_PERFORMANCE)
    {
        my $hModuleType = $hTestDef->{$strModuleType};

        my $bContainer = true;                                      # By default run tests in a single container
        my $bIndividual = false;                                    # By default runs are all executed in the same container

        if ($strModuleType eq TESTDEF_INTEGRATION)
        {
            $bContainer = false;                                    # Integration tests can run in multiple containers
            $bIndividual = true;                                    # Integration tests can change containers on each run
        }

        # Iterate each module
        foreach my $hModule (@{$hModuleType})
        {
            # Push the module onto the ordered list
            my $strModule = $hModule->{&TESTDEF_NAME};
            push(@stryModule, $strModule);

            # Iterate each test
            my @stryModuleTest;

            foreach my $hModuleTest (@{$hModule->{&TESTDEF_TEST}})
            {
                # Push the test on the order list
                my $strTest = $hModuleTest->{&TESTDEF_NAME};
                push(@stryModuleTest, $strTest);

                # Resolve variables that can be set in the module or the test
                foreach my $strVar (
                    TESTDEF_DB, TESTDEF_BIN_REQ, TESTDEF_VM, TESTDEF_CONTAINER_REQUIRED)
                {
                    $hTestDefHash->{$strModule}{$strTest}{$strVar} = coalesce(
                        $hModuleTest->{$strVar}, $hModule->{$strVar}, $strVar eq TESTDEF_VM ? undef : false);

                    # Make false = 0 for debugging
                    if ($strVar ne TESTDEF_VM && $hTestDefHash->{$strModule}{$strTest}{$strVar} eq '')
                    {
                        $hTestDefHash->{$strModule}{$strTest}{$strVar} = false;
                    }
                }

                # Set module type variables
                $hTestDefHash->{$strModule}{$strTest}{&TESTDEF_TYPE} = $strModuleType;
                $hTestDefHash->{$strModule}{$strTest}{&TESTDEF_C} =
                    $strModuleType ne TESTDEF_INTEGRATION && $strTest !~ /perl$/ ? true : false;
                $hTestDefHash->{$strModule}{$strTest}{&TESTDEF_INTEGRATION} = $strModuleType eq TESTDEF_INTEGRATION ? true : false;
                $hTestDefHash->{$strModule}{$strTest}{&TESTDEF_CONTAINER} = $bContainer;
                $hTestDefHash->{$strModule}{$strTest}{&TESTDEF_INDIVIDUAL} = $bIndividual;

                # Set test count
                $hTestDefHash->{$strModule}{$strTest}{&TESTDEF_TOTAL} = $hModuleTest->{&TESTDEF_TOTAL};

                # If this is a C test then add the test module to coverage
                if ($hModuleTest->{&TESTDEF_C})
                {
                    my $strTestFile = "module/${strModule}/${strTest}Test";

                    $hModuleTest->{&TESTDEF_COVERAGE}{$strTestFile} = TESTDEF_COVERAGE_FULL;
                }

                # Concatenate coverage for tests
                foreach my $xCodeModule (@{$hModuleTest->{&TESTDEF_COVERAGE}})
                {
                    my $strCodeModule = undef;
                    my $strCoverage = undef;

                    if (ref($xCodeModule))
                    {
                        $strCodeModule = (keys(%{$xCodeModule}))[0];
                        $strCoverage = $xCodeModule->{$strCodeModule};
                    }
                    else
                    {
                        $strCodeModule = $xCodeModule;
                        $strCoverage = TESTDEF_COVERAGE_FULL;
                    }

                    $hTestDefHash->{$strModule}{$strTest}{&TESTDEF_COVERAGE}{$strCodeModule} = $strCoverage;

                    # Build coverage type hash and make sure coverage type does not change
                    if (!defined($hCoverageType->{$strCodeModule}))
                    {
                        $hCoverageType->{$strCodeModule} = $strCoverage;
                    }
                    elsif ($hCoverageType->{$strCodeModule} ne $strCoverage)
                    {
                        confess &log(ASSERT, "cannot mix coverage types for ${strCodeModule}");
                    }

                    # Add to coverage list
                    push(@{$hCoverageList->{$strCodeModule}}, {strModule=> $strModule, strTest => $strTest});

                    # Check if this module is already in the core list
                    if (!$hTestDefHash->{$strModule}{$strTest}{&TESTDEF_INTEGRATION} && !grep(/^$strCodeModule$/i, @stryCoreFile))
                    {
                        push(@stryCoreFile, $strCodeModule);
                    }
                }

                # Set include list
                @{$hTestDefHash->{$strModule}{$strTest}{&TESTDEF_INCLUDE}} = ();

                if (defined($hModuleTest->{&TESTDEF_INCLUDE}))
                {
                    push(@{$hTestDefHash->{$strModule}{$strTest}{&TESTDEF_INCLUDE}}, @{$hModuleTest->{&TESTDEF_INCLUDE}});
                }
            }

            $hModuleTest->{$strModule} = \@stryModuleTest;
        }
    }
}

push @EXPORT, qw(testDefLoad);

####################################################################################################################################
# testDefModuleList
####################################################################################################################################
sub testDefModuleList
{
    return @stryModule;
}

push @EXPORT, qw(testDefModuleList);

####################################################################################################################################
# testDefModule
####################################################################################################################################
sub testDefModule
{
    my $strModule = shift;

    if (!defined($hTestDefHash->{$strModule}))
    {
        confess &log(ASSERT, "unable to find module ${strModule}");
    }

    return $hTestDefHash->{$strModule};
}

push @EXPORT, qw(testDefModule);

####################################################################################################################################
# testDefModuleTestList
####################################################################################################################################
sub testDefModuleTestList
{
    my $strModule = shift;

    if (!defined($hModuleTest->{$strModule}))
    {
        confess &log(ASSERT, "unable to find module ${strModule}");
    }

    return @{$hModuleTest->{$strModule}};
}

push @EXPORT, qw(testDefModuleTestList);

####################################################################################################################################
# testDefModuleTest
####################################################################################################################################
sub testDefModuleTest
{
    my $strModule = shift;
    my $strModuleTest = shift;

    if (!defined($hTestDefHash->{$strModule}{$strModuleTest}))
    {
        confess &log(ASSERT, "unable to find module ${strModule}, test ${strModuleTest}");
    }

    return $hTestDefHash->{$strModule}{$strModuleTest};
}

push @EXPORT, qw(testDefModuleTest);

####################################################################################################################################
# testDefCoverageType
####################################################################################################################################
sub testDefCoverageType
{
    return $hCoverageType;
}

push @EXPORT, qw(testDefCoverageType);

####################################################################################################################################
# testDefCoverageList
####################################################################################################################################
sub testDefCoverageList
{
    return $hCoverageList;
}

push @EXPORT, qw(testDefCoverageList);

1;
