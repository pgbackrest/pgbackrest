####################################################################################################################################
# CiTest.pm - Create Travis configuration file for continuous integration testing
####################################################################################################################################
package pgBackRestTest::Common::CiTest;

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Cwd qw(abs_path);
use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(dirname);
use POSIX qw(ceil);
use Time::HiRes qw(gettimeofday);

use pgBackRest::DbVersion;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::FileCommon;
use pgBackRest::Version;

use pgBackRestTest::Common::ContainerTest;
use pgBackRestTest::Common::DefineTest;
use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::ListTest;
use pgBackRestTest::Common::VmTest;

####################################################################################################################################
# new
####################################################################################################################################
sub new
{
    my $class = shift;          # Class name

    # Create the class hash
    my $self = {};
    bless $self, $class;

    # Assign function parameters, defaults, and log debug info
    (
        my $strOperation,
        $self->{strBackRestBase},
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'strBackRestBase'},
        );

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self, trace => true}
    );
}

####################################################################################################################################
# process
####################################################################################################################################
sub process
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    (my $strOperation) = logDebugParam (__PACKAGE__ . '->process', \@_,);

    # Configure environment
    my $strConfig =
        "branches:\n" .
        "  only:\n" .
        "    - master\n" .
        "    - integration\n" .
        "    - /-ci\$/\n" .
        "\n" .
        "dist: trusty\n" .
        "sudo: required\n" .
        "\n" .
        "language: c\n" .
        "\n" .
        "services:\n" .
        "  - docker\n" .
        "\n" .
        "env:\n";

    my $bFirst = true;

    # Iterate each OS
    foreach my $strVm (VM_LIST)
    {
        my $hTestDef = testDefGet();
        my $hVm = vmGet();
        my @stryModule;
        my $hFullModule = undef;

        # Get all modules but full to break up the tests
        foreach my $hModule (@{$hTestDef->{&TESTDEF_MODULE}})
        {
            my $strModule = $hModule->{&TESTDEF_MODULE_NAME};

            if ($strModule ne 'full')
            {
                push(@stryModule, $strModule);
            }
            else
            {
                $hFullModule = $hModule;
            }
        }

        # Add config options for tests that are not the very first one
        my $strConfigNotFirst = '--no-lint';

        $strConfig .=
            "  - PGB_TEST_VM=\"${strVm}\" PGB_BUILD_PARAM=\"--db=none\" PGB_TEST_PARAM=\"--module=" .
                join(' --module=', @stryModule) . ($bFirst ? '' : " ${strConfigNotFirst}") . "\"\n";
        $bFirst = false;

        # Now generate full tests
        my $hRealTest = undef;

        if (!defined($hFullModule))
        {
            confess "the full module is not defined, has the name changed?";
        }

        foreach my $hTest (@{$hFullModule->{&TESTDEF_TEST}})
        {
            my $strTest = $hTest->{&TESTDEF_TEST_NAME};

            if ($strTest eq 'real')
            {
                $hRealTest = $hTest;

                foreach my $strDbVersion (sort {$b cmp $a} @{$hVm->{$strVm}{&VM_DB_MINIMAL}})
                {
                    $strConfig .=
                        "  - PGB_TEST_VM=\"${strVm}\" PGB_BUILD_PARAM=\"--db=${strDbVersion}\"" .
                            " PGB_TEST_PARAM=\"--module=full --test=real --db=${strDbVersion}" .
                            " --process-max=2 ${strConfigNotFirst}\"\n";
                }
            }
            else
            {
                $strConfig .=
                    "  - PGB_TEST_VM=\"${strVm}\" PGB_BUILD_PARAM=\"--db=none\"" .
                        " PGB_TEST_PARAM=\"--module=full --test=${strTest} ${strConfigNotFirst}\"\n";
            }
        }

        if (!defined($hRealTest))
        {
            confess 'real test not found in full module, has the name changed?';
        }
    }

    # Configure install and script
    $strConfig .=
        "\n" .
        "before_install:\n" .
        "  - sudo apt-get -qq update\n" .
        "  - sudo apt-get install libxml-checker-perl libdbd-pg-perl libperl-critic-perl libdevel-cover-perl\n" .
        "\n" .
        "install:\n" .
        "  - sudo adduser --ingroup=\${USER?} --disabled-password --gecos \"\" " . BACKREST_USER . "\n" .
        "  - umask 0022\n" .
        "  - cd ~ && pwd && whoami && umask && groups\n" .
        "  - mv \${TRAVIS_BUILD_DIR?} " . BACKREST_EXE . "\n" .
        "  - rm -rf \${TRAVIS_BUILD_DIR?}\n" .
        "  - " . BACKREST_EXE . "/test/test.pl --vm-build --vm=\${PGB_TEST_VM?} \${PGB_BUILD_PARAM?}\n" .
        "\n" .
        "script:\n" .
        "  - " . BACKREST_EXE . "/test/test.pl --vm-host=u14 --vm=\${PGB_TEST_VM?} --no-package \${PGB_TEST_PARAM?}\n";

    fileStringWrite("$self->{strBackRestBase}/.travis.yml", $strConfig, false);

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

1;
