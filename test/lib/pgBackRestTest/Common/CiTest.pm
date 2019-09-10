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
use pgBackRest::Version;

use pgBackRestTest::Common::BuildTest;
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
        $self->{oStorage},
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'oStorage'},
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

    # Iterate each OS
    foreach my $strVm (VM_LIST)
    {
        $strConfig .= "  - PGB_CI=\"--vm=${strVm} test\"\n";
    }

    # $strConfig .= "  - PGB_CI=\"doc\"\n";

    # Configure install and script
    $strConfig .=
        "\n" .
        "before_install:\n" .
        "  - sudo apt-get -qq update && sudo apt-get install libxml-checker-perl libdbd-pg-perl libyaml-libyaml-perl python-pip" .
            " lcov libperl-dev\n" .
        "  - |\n" .
        "    # Install & Configure AWS CLI\n" .
        "    pip install --upgrade --user awscli\n" .
        "    aws configure set region us-east-1\n" .
        "    aws configure set aws_access_key_id accessKey1\n" .
        "    aws configure set aws_secret_access_key verySecretKey1\n" .
        "    aws help --version\n" .
        "    aws configure list\n" .
        "\n" .
        "install:\n" .
        "  - |\n" .
        "    # User Configuration\n" .
        "    sudo adduser --ingroup=\${USER?} --uid=5001 --disabled-password --gecos \"\" " . BACKREST_USER . "\n" .
        "    umask 0022\n" .
        "    cd ~ && pwd && whoami && umask && groups\n" .
        "    mv \${TRAVIS_BUILD_DIR?} " . PROJECT_EXE . "\n" .
        "    rm -rf \${TRAVIS_BUILD_DIR?}\n" .
        "\n" .
        "script:\n" .
        "  - " . PROJECT_EXE . "/test/travis.pl \${PGB_CI?}\n";

    buildPutDiffers($self->{oStorage}, '.travis.yml', $strConfig);

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

1;
