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

################################################################################################################################
# Test definition constants
################################################################################################################################
use constant TESTDEF_MODULE                                         => 'module';
    push @EXPORT, qw(TESTDEF_MODULE);
use constant TESTDEF_MODULE_NAME                                    => 'name';
    push @EXPORT, qw(TESTDEF_MODULE_NAME);

use constant TESTDEF_TEST                                           => 'test';
    push @EXPORT, qw(TESTDEF_TEST);
use constant TESTDEF_TEST_NAME                                      => 'name';
    push @EXPORT, qw(TESTDEF_TEST_NAME);
use constant TESTDEF_TEST_TOTAL                                     => 'total';
    push @EXPORT, qw(TESTDEF_TEST_TOTAL);
use constant TESTDEF_TEST_CONTAINER                                 => 'container';
    push @EXPORT, qw(TESTDEF_TEST_CONTAINER);
use constant TESTDEF_TEST_THREAD                                    => 'thread';
    push @EXPORT, qw(TESTDEF_TEST_THREAD);
use constant TESTDEF_TEST_DB                                        => 'db';
    push @EXPORT, qw(TESTDEF_TEST_DB);

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

            &TESTDEF_TEST =>
            [
                {
                    &TESTDEF_TEST_NAME => 'help'
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
                    &TESTDEF_TEST_NAME => 'option'
                },
                {
                    &TESTDEF_TEST_NAME => 'config'
                }
            ]
        },
        # File tests
        {
            &TESTDEF_MODULE_NAME => 'file',
            &TESTDEF_TEST_CONTAINER => true,

            &TESTDEF_TEST =>
            [
                {
                    &TESTDEF_TEST_NAME => 'path_create'
                },
                {
                    &TESTDEF_TEST_NAME => 'move'
                },
                {
                    &TESTDEF_TEST_NAME => 'compress'
                },
                {
                    &TESTDEF_TEST_NAME => 'wait'
                },
                {
                    &TESTDEF_TEST_NAME => 'manifest'
                },
                {
                    &TESTDEF_TEST_NAME => 'list'
                },
                {
                    &TESTDEF_TEST_NAME => 'remove'
                },
                {
                    &TESTDEF_TEST_NAME => 'hash'
                },
                {
                    &TESTDEF_TEST_NAME => 'exists'
                },
                {
                    &TESTDEF_TEST_NAME => 'copy'
                }
            ]
        },
        # Backup tests
        {
            &TESTDEF_MODULE_NAME => 'backup',
            &TESTDEF_TEST_CONTAINER => false,

            &TESTDEF_TEST =>
            [
                {
                    &TESTDEF_TEST_NAME => 'archive-push',
                    &TESTDEF_TEST_TOTAL => 8
                },
                {
                    &TESTDEF_TEST_NAME => 'archive-stop',
                    &TESTDEF_TEST_TOTAL => 6
                },
                {
                    &TESTDEF_TEST_NAME => 'archive-get',
                    &TESTDEF_TEST_TOTAL => 8
                },
                {
                    &TESTDEF_TEST_NAME => 'expire',
                    &TESTDEF_TEST_TOTAL => 1
                },
                {
                    &TESTDEF_TEST_NAME => 'synthetic',
                    &TESTDEF_TEST_TOTAL => 8,
                    &TESTDEF_TEST_THREAD => true
                },
                {
                    &TESTDEF_TEST_NAME => 'full',
                    &TESTDEF_TEST_TOTAL => 8,
                    &TESTDEF_TEST_THREAD => true,
                    &TESTDEF_TEST_DB => true
                }
            ]
        }
    ]
};

####################################################################################################################################
# testDefGet
####################################################################################################################################
sub testDefGet
{
    return $oTestDef;
}

push @EXPORT, qw(testDefGet);

1;
