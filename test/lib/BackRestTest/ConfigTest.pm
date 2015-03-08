#!/usr/bin/perl
####################################################################################################################################
# ConfigTest.pl - Unit Tests for BackRest::Param and BackRest::Config
####################################################################################################################################
package BackRestTest::ConfigTest;

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use File::Basename qw(dirname);
use Scalar::Util 'blessed';
#use Data::Dumper qw(Dumper);
#use Scalar::Util qw(blessed);
# use Test::More qw(no_plan);
# use Test::Deep;

use lib dirname($0) . '/../lib';
use BackRest::Exception;
use BackRest::Utility;
use BackRest::Param;

use BackRestTest::CommonTest;

use Exporter qw(import);
our @EXPORT = qw(BackRestTestConfig_Test);

sub optionSetTest
{
    my $oOption = shift;
    my $strKey = shift;
    my $strValue = shift;

    $$oOption{option}{$strKey} = $strValue;
}

sub optionSetBoolTest
{
    my $oOption = shift;
    my $strKey = shift;

    $$oOption{boolean}{$strKey} = true;
}

sub operationSetTest
{
    my $oOption = shift;
    my $strOperation = shift;

    $$oOption{operation} = $strOperation;
}

sub optionRemoveTest
{
    my $oOption = shift;
    my $strKey = shift;

    delete($$oOption{option}{$strKey});
    delete($$oOption{boolean}{$strKey});
}

sub argvWriteTest
{
    my $oOption = shift;

    @ARGV = ();

    if (defined($$oOption{boolean}))
    {
        foreach my $strKey (keys $$oOption{boolean})
        {
            if ($$oOption{boolean}{$strKey})
            {
                $ARGV[@ARGV] = "--${strKey}";
            }
            else
            {
                $ARGV[@ARGV] = "--no-${strKey}";
            }
        }
    }

    if (defined($$oOption{option}))
    {
        foreach my $strKey (keys $$oOption{option})
        {
            $ARGV[@ARGV] = "--${strKey}=";

            if (defined($$oOption{option}{$strKey}))
            {
                $ARGV[@ARGV - 1] .= $$oOption{option}{$strKey};
            }
        }
    }

    $ARGV[@ARGV] = $$oOption{operation};

    &log(INFO, "    command line: " . join(" ", @ARGV));

    %$oOption = ();
}

sub configLoadExpectError
{
    my $oOption = shift;
    my $strOperation = shift;
    my $iExpectedError = shift;
    my $strErrorParam1 = shift;
    my $strErrorParam2 = shift;
    my $strErrorParam3 = shift;

    my $oOptionRuleExpected = optionRuleGet();

    operationSetTest($oOption, $strOperation);
    argvWriteTest($oOption);

    eval
    {
        configLoad();
    };

    if ($@)
    {
        if (!defined($iExpectedError))
        {
            confess $@;
        }

        my $oMessage = $@;

        if (blessed($oMessage) && $oMessage->isa('BackRest::Exception'))
        {
            if ($oMessage->code() != $iExpectedError)
            {
                confess "expected error ${iExpectedError} from configLoad but got " . $oMessage->code();
            }

            my $strError;

            if ($iExpectedError == ERROR_OPTION_REQUIRED)
            {
                $strError = "backup operation requires option: ${strErrorParam1}";
            }
            elsif ($iExpectedError == ERROR_OPERATION_REQUIRED)
            {
                $strError = "operation must be specified";
            }
            elsif ($iExpectedError == ERROR_OPTION_INVALID)
            {
                $strError = "option '${strErrorParam1}' not valid without option '${strErrorParam2}'";

                if (defined($strErrorParam3))
                {
                    $strError .= @{$strErrorParam3} == 1 ? " = '$$strErrorParam3[0]'" :
                                 " in ('" . join("', '",@{ $strErrorParam3}) . "')";
                }
            }
            elsif ($iExpectedError == ERROR_OPTION_INVALID_VALUE)
            {
                $strError = "'${strErrorParam1}' is not valid for '${strErrorParam2}' option";
            }
            else
            {
                confess "must construct message for error ${iExpectedError}, use this as an example: '" . $oMessage->message() . "'";
            }

            if ($oMessage->message() ne $strError)
            {
                confess "expected error message \"${strError}\" from configLoad but got \"" . $oMessage->message() . "\"";
            }
        }
        else
        {
            confess "configLoad should throw BackRest::Exception:\n$oMessage";
        }
    }
    else
    {
        if (defined($iExpectedError))
        {
            confess "expected error ${iExpectedError} from configLoad but got success";
        }
    }

    # cmp_deeply(OPTION_rule_get(), $oOptionRuleExpected, 'compare original and new rule hashes')
    #     or die 'comparison failed';
}

sub optionTestExpect
{
    my $strOption = shift;
    my $strExpectedValue = shift;

    if (defined($strExpectedValue))
    {
        my $strActualValue = optionGet($strOption);

        $strActualValue eq $strExpectedValue
            or confess "expected option ${strOption} to have value ${strExpectedValue}, but ${strActualValue} found instead";
    }
    elsif (optionTest($strOption))
    {
        confess "expected option ${strOption} to be [undef], but " . optionGet($strOption) . ' found instead';
    }
}

####################################################################################################################################
# BackRestTestConfig_Test
####################################################################################################################################
sub BackRestTestConfig_Test
{
    my $strTest = shift;

    # Setup test variables
    my $iRun;
    my $bCreate;
    my $strStanza = 'main';
    my $oOption = {};
    my @oyArray;
    use constant BOGUS => 'bogus';

    # Print test banner
    &log(INFO, 'CONFIG MODULE ******************************************************************');

    #-------------------------------------------------------------------------------------------------------------------------------
    # Test config
    #-------------------------------------------------------------------------------------------------------------------------------
    if ($strTest eq 'all' || $strTest eq 'option')
    {
        &log(INFO, "Option module\n");

        if (BackRestTestCommon_Run(++$iRun, 'backup with no stanza'))
        {
            configLoadExpectError($oOption, OP_BACKUP , ERROR_OPTION_REQUIRED, OPTION_STANZA);
        }

        if (BackRestTestCommon_Run(++$iRun, 'backup with boolean stanza'))
        {
            optionSetBoolTest($oOption, OPTION_STANZA);

            configLoadExpectError($oOption, OP_BACKUP, , ERROR_OPERATION_REQUIRED);
        }

        if (BackRestTestCommon_Run(++$iRun, 'backup type defaults to ' . BACKUP_TYPE_INCR))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);

            configLoadExpectError($oOption, OP_BACKUP);
            optionTestExpect(OPTION_TYPE, BACKUP_TYPE_INCR);
        }

        if (BackRestTestCommon_Run(++$iRun, 'backup type set to ' . BACKUP_TYPE_FULL))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_TYPE, BACKUP_TYPE_FULL);

            configLoadExpectError($oOption, OP_BACKUP);
            optionTestExpect(OPTION_TYPE, BACKUP_TYPE_FULL);
        }

        if (BackRestTestCommon_Run(++$iRun, 'backup type invalid'))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_TYPE, BOGUS);

            configLoadExpectError($oOption, OP_BACKUP , ERROR_OPTION_INVALID_VALUE, BOGUS, OPTION_TYPE);
        }

        if (BackRestTestCommon_Run(++$iRun, 'backup invalid force'))
        {
#            $oOption = {};
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetBoolTest($oOption, OPTION_FORCE);

            configLoadExpectError($oOption, OP_BACKUP, ERROR_OPTION_INVALID, OPTION_FORCE, OPTION_NO_START_STOP);
        }

        if (BackRestTestCommon_Run(++$iRun, 'backup valid force'))
        {
            # $oOption = {};
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetBoolTest($oOption, OPTION_NO_START_STOP);
            optionSetBoolTest($oOption, OPTION_FORCE);

            configLoadExpectError($oOption, OP_BACKUP);
            optionTestExpect(OPTION_NO_START_STOP, true);
            optionTestExpect(OPTION_FORCE, true);
        }

        if (BackRestTestCommon_Run(++$iRun, 'backup invalid value for ' . OPTION_TEST_DELAY))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetBoolTest($oOption, OPTION_TEST);
            optionSetTest($oOption, OPTION_TEST_DELAY, BOGUS);

            configLoadExpectError($oOption, OP_BACKUP , ERROR_OPTION_INVALID_VALUE, BOGUS, OPTION_TEST_DELAY);
        }

        if (BackRestTestCommon_Run(++$iRun, 'backup invalid ' . OPTION_TEST_DELAY))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_TEST_DELAY, 5);

            configLoadExpectError($oOption, OP_BACKUP , ERROR_OPTION_INVALID, OPTION_TEST_DELAY, OPTION_TEST);
        }

        if (BackRestTestCommon_Run(++$iRun, 'backup check ' . OPTION_TEST_DELAY . ' undef'))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);

            configLoadExpectError($oOption, OP_BACKUP);
            optionTestExpect(OPTION_TEST_DELAY);
        }

        if (BackRestTestCommon_Run(++$iRun, 'restore invalid ' . OPTION_TARGET))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_TYPE, RECOVERY_TYPE_DEFAULT);
            optionSetTest($oOption, OPTION_TARGET, BOGUS);

            @oyArray = (RECOVERY_TYPE_NAME, RECOVERY_TYPE_TIME, RECOVERY_TYPE_XID);
            configLoadExpectError($oOption, OP_RESTORE , ERROR_OPTION_INVALID, OPTION_TARGET, OPTION_TYPE, \@oyArray);
        }

        if (BackRestTestCommon_Run(++$iRun, 'restore ' . OPTION_TARGET))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_TYPE, RECOVERY_TYPE_NAME);
            optionSetTest($oOption, OPTION_TARGET, BOGUS);

            configLoadExpectError($oOption, OP_RESTORE);
            optionTestExpect(OPTION_TYPE, RECOVERY_TYPE_NAME);
            optionTestExpect(OPTION_TARGET, BOGUS);
            optionTestExpect(OPTION_TARGET_TIMELINE);
        }

        if (BackRestTestCommon_Run(++$iRun, 'invalid string ' . OPTION_THREAD_MAX))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_THREAD_MAX, BOGUS);

            configLoadExpectError($oOption, OP_BACKUP , ERROR_OPTION_INVALID_VALUE, BOGUS, OPTION_THREAD_MAX);
        }

        if (BackRestTestCommon_Run(++$iRun, 'invalid float ' . OPTION_THREAD_MAX))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_THREAD_MAX, '0.0');

            configLoadExpectError($oOption, OP_BACKUP , ERROR_OPTION_INVALID_VALUE, '0.0', OPTION_THREAD_MAX);
        }

        if (BackRestTestCommon_Run(++$iRun, 'valid ' . OPTION_THREAD_MAX))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_THREAD_MAX, '2');

            configLoadExpectError($oOption, OP_BACKUP);
        }

        if (BackRestTestCommon_Run(++$iRun, 'valid float ' . OPTION_TEST_DELAY))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetBoolTest($oOption, OPTION_TEST);
            optionSetTest($oOption, OPTION_TEST_DELAY, '0.25');

            configLoadExpectError($oOption, OP_BACKUP);
        }

        if (BackRestTestCommon_Run(++$iRun, 'valid int ' . OPTION_TEST_DELAY))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetBoolTest($oOption, OPTION_TEST);
            optionSetTest($oOption, OPTION_TEST_DELAY, 3);

            configLoadExpectError($oOption, OP_BACKUP);
        }

        if (BackRestTestCommon_Run(++$iRun, 'restore valid ' . OPTION_TARGET_TIMELINE))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_TARGET_TIMELINE, 2);

            configLoadExpectError($oOption, OP_RESTORE);
        }
    }
}

1;
