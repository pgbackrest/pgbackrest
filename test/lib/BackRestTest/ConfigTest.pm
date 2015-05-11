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
use Cwd qw(abs_path);
use Scalar::Util 'blessed';
#use Data::Dumper qw(Dumper);
#use Scalar::Util qw(blessed);
# use Test::More qw(no_plan);
# use Test::Deep;

use lib dirname($0) . '/../lib';
use BackRest::Exception;
use BackRest::Utility;
use BackRest::Config;

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
    my $bValue = shift;

    $$oOption{boolean}{$strKey} = defined($bValue) ? $bValue : true;
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
            $ARGV[@ARGV] = "--${strKey}=$$oOption{option}{$strKey}";
        }
    }

    $ARGV[@ARGV] = $$oOption{operation};

    &log(INFO, "    command line: " . join(" ", @ARGV));

    %$oOption = ();
}

sub configLoadExpect
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
                confess "expected error ${iExpectedError} from configLoad but got " . $oMessage->code() .
                        " '" . $oMessage->message() . "'";
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
            elsif ($iExpectedError == ERROR_OPTION_INVALID_RANGE)
            {
                $strError = "'${strErrorParam1}' is not valid for '${strErrorParam2}' option";
            }
            elsif ($iExpectedError == ERROR_OPTION_INVALID_PAIR)
            {
                $strError = "'${strErrorParam1}' not valid key/value for '${strErrorParam2}' option";
            }
            elsif ($iExpectedError == ERROR_OPTION_NEGATE)
            {
                $strError = "option '${strErrorParam1}' cannot be both set and negated";
            }
            elsif ($iExpectedError == ERROR_FILE_INVALID)
            {
                $strError = "'${strErrorParam1}' is not a file";
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
    my $strExpectedKey = shift;

    if (defined($strExpectedValue))
    {
        my $strActualValue = optionGet($strOption);

        if (defined($strExpectedKey))
        {
            # use Data::Dumper;
            # &log(INFO, Dumper($strActualValue));
            # exit 0;

            $strActualValue = $$strActualValue{$strExpectedKey};
        }

        if (!defined($strActualValue))
        {
            confess "expected option ${strOption} to have value ${strExpectedValue} but [undef] found instead";
        }

        $strActualValue eq $strExpectedValue
            or confess "expected option ${strOption} to have value ${strExpectedValue} but ${strActualValue} found instead";
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
    my $oConfig = {};
    my @oyArray;
    my $strConfigFile = BackRestTestCommon_TestPathGet() . '/pg_backrest.conf';

    use constant BOGUS => 'bogus';

    # Print test banner
    &log(INFO, 'CONFIG MODULE ******************************************************************');
    BackRestTestCommon_Drop();

    #-------------------------------------------------------------------------------------------------------------------------------
    # Test command-line options
    #-------------------------------------------------------------------------------------------------------------------------------
    if ($strTest eq 'all' || $strTest eq 'option')
    {
        $iRun = 0;
        &log(INFO, "Option module\n");

        if (BackRestTestCommon_Run(++$iRun, 'backup with no stanza'))
        {
            optionSetTest($oOption, OPTION_DB_PATH, '/db');

            configLoadExpect($oOption, OP_BACKUP, ERROR_OPTION_REQUIRED, OPTION_STANZA);
        }

        if (BackRestTestCommon_Run(++$iRun, 'backup with boolean stanza'))
        {
            optionSetBoolTest($oOption, OPTION_STANZA);

            configLoadExpect($oOption, OP_BACKUP, ERROR_OPERATION_REQUIRED);
        }

        if (BackRestTestCommon_Run(++$iRun, 'backup type defaults to ' . BACKUP_TYPE_INCR))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');

            configLoadExpect($oOption, OP_BACKUP);
            optionTestExpect(OPTION_TYPE, BACKUP_TYPE_INCR);
        }

        if (BackRestTestCommon_Run(++$iRun, 'backup type set to ' . BACKUP_TYPE_FULL))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetTest($oOption, OPTION_TYPE, BACKUP_TYPE_FULL);

            configLoadExpect($oOption, OP_BACKUP);
            optionTestExpect(OPTION_TYPE, BACKUP_TYPE_FULL);
        }

        if (BackRestTestCommon_Run(++$iRun, 'backup type invalid'))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetTest($oOption, OPTION_TYPE, BOGUS);

            configLoadExpect($oOption, OP_BACKUP, ERROR_OPTION_INVALID_VALUE, BOGUS, OPTION_TYPE);
        }

        if (BackRestTestCommon_Run(++$iRun, 'backup invalid force'))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetBoolTest($oOption, OPTION_FORCE);

            configLoadExpect($oOption, OP_BACKUP, ERROR_OPTION_INVALID, OPTION_FORCE, OPTION_NO_START_STOP);
        }

        if (BackRestTestCommon_Run(++$iRun, 'backup valid force'))
        {
            # $oOption = {};
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetBoolTest($oOption, OPTION_NO_START_STOP);
            optionSetBoolTest($oOption, OPTION_FORCE);

            configLoadExpect($oOption, OP_BACKUP);
            optionTestExpect(OPTION_NO_START_STOP, true);
            optionTestExpect(OPTION_FORCE, true);
        }

        if (BackRestTestCommon_Run(++$iRun, 'backup invalid value for ' . OPTION_TEST_DELAY))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetBoolTest($oOption, OPTION_TEST);
            optionSetTest($oOption, OPTION_TEST_DELAY, BOGUS);

            configLoadExpect($oOption, OP_BACKUP, ERROR_OPTION_INVALID_VALUE, BOGUS, OPTION_TEST_DELAY);
        }

        if (BackRestTestCommon_Run(++$iRun, 'backup invalid ' . OPTION_TEST_DELAY))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetTest($oOption, OPTION_TEST_DELAY, 5);

            configLoadExpect($oOption, OP_BACKUP, ERROR_OPTION_INVALID, OPTION_TEST_DELAY, OPTION_TEST);
        }

        if (BackRestTestCommon_Run(++$iRun, 'backup check ' . OPTION_TEST_DELAY . ' undef'))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');

            configLoadExpect($oOption, OP_BACKUP);
            optionTestExpect(OPTION_TEST_DELAY);
        }

        if (BackRestTestCommon_Run(++$iRun, 'restore invalid ' . OPTION_TARGET))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_TYPE, RECOVERY_TYPE_DEFAULT);
            optionSetTest($oOption, OPTION_TARGET, BOGUS);

            @oyArray = (RECOVERY_TYPE_NAME, RECOVERY_TYPE_TIME, RECOVERY_TYPE_XID);
            configLoadExpect($oOption, OP_RESTORE, ERROR_OPTION_INVALID, OPTION_TARGET, OPTION_TYPE, \@oyArray);
        }

        if (BackRestTestCommon_Run(++$iRun, 'restore ' . OPTION_TARGET))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_TYPE, RECOVERY_TYPE_NAME);
            optionSetTest($oOption, OPTION_TARGET, BOGUS);

            configLoadExpect($oOption, OP_RESTORE);
            optionTestExpect(OPTION_TYPE, RECOVERY_TYPE_NAME);
            optionTestExpect(OPTION_TARGET, BOGUS);
            optionTestExpect(OPTION_TARGET_TIMELINE);
        }

        if (BackRestTestCommon_Run(++$iRun, 'invalid string ' . OPTION_THREAD_MAX))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetTest($oOption, OPTION_THREAD_MAX, BOGUS);

            configLoadExpect($oOption, OP_BACKUP, ERROR_OPTION_INVALID_VALUE, BOGUS, OPTION_THREAD_MAX);
        }

        if (BackRestTestCommon_Run(++$iRun, 'invalid float ' . OPTION_THREAD_MAX))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetTest($oOption, OPTION_THREAD_MAX, '0.0');

            configLoadExpect($oOption, OP_BACKUP, ERROR_OPTION_INVALID_VALUE, '0.0', OPTION_THREAD_MAX);
        }

        if (BackRestTestCommon_Run(++$iRun, 'valid ' . OPTION_THREAD_MAX))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetTest($oOption, OPTION_THREAD_MAX, '2');

            configLoadExpect($oOption, OP_BACKUP);
        }

        if (BackRestTestCommon_Run(++$iRun, 'valid float ' . OPTION_TEST_DELAY))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetBoolTest($oOption, OPTION_TEST);
            optionSetTest($oOption, OPTION_TEST_DELAY, '0.25');

            configLoadExpect($oOption, OP_BACKUP);
        }

        if (BackRestTestCommon_Run(++$iRun, 'valid int ' . OPTION_TEST_DELAY))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetBoolTest($oOption, OPTION_TEST);
            optionSetTest($oOption, OPTION_TEST_DELAY, 3);

            configLoadExpect($oOption, OP_BACKUP);
        }

        if (BackRestTestCommon_Run(++$iRun, 'restore valid ' . OPTION_TARGET_TIMELINE))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_TARGET_TIMELINE, 2);

            configLoadExpect($oOption, OP_RESTORE);
        }

        if (BackRestTestCommon_Run(++$iRun, 'invalid ' . OPTION_BUFFER_SIZE))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_BUFFER_SIZE, '512');

            configLoadExpect($oOption, OP_RESTORE, ERROR_OPTION_INVALID_RANGE, '512', OPTION_BUFFER_SIZE);
        }

        if (BackRestTestCommon_Run(++$iRun, OP_BACKUP . ' invalid option' . OPTION_RETENTION_ARCHIVE_TYPE))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetTest($oOption, OPTION_RETENTION_ARCHIVE_TYPE, BOGUS);

            configLoadExpect($oOption, OP_BACKUP, ERROR_OPTION_INVALID, OPTION_RETENTION_ARCHIVE_TYPE, OPTION_RETENTION_ARCHIVE);
        }

        if (BackRestTestCommon_Run(++$iRun, OP_BACKUP . ' invalid value ' . OPTION_RETENTION_ARCHIVE_TYPE))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetTest($oOption, OPTION_RETENTION_ARCHIVE, 3);
            optionSetTest($oOption, OPTION_RETENTION_ARCHIVE_TYPE, BOGUS);

            configLoadExpect($oOption, OP_BACKUP, ERROR_OPTION_INVALID_VALUE, BOGUS, OPTION_RETENTION_ARCHIVE_TYPE);
        }

        if (BackRestTestCommon_Run(++$iRun, OP_BACKUP . ' valid value ' . OPTION_RETENTION_ARCHIVE_TYPE))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetTest($oOption, OPTION_RETENTION_ARCHIVE, 1);
            optionSetTest($oOption, OPTION_RETENTION_ARCHIVE_TYPE, BACKUP_TYPE_FULL);

            configLoadExpect($oOption, OP_BACKUP);
            optionTestExpect(OPTION_RETENTION_ARCHIVE, 1);
            optionTestExpect(OPTION_RETENTION_ARCHIVE_TYPE, BACKUP_TYPE_FULL);
        }

        if (BackRestTestCommon_Run(++$iRun, OP_RESTORE . ' invalid value ' . OPTION_RESTORE_RECOVERY_SETTING))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_RESTORE_RECOVERY_SETTING, '=');

            configLoadExpect($oOption, OP_RESTORE, ERROR_OPTION_INVALID_PAIR, '=', OPTION_RESTORE_RECOVERY_SETTING);
        }

        if (BackRestTestCommon_Run(++$iRun, OP_RESTORE . ' invalid value ' . OPTION_RESTORE_RECOVERY_SETTING))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_RESTORE_RECOVERY_SETTING, '=' . BOGUS);

            configLoadExpect($oOption, OP_RESTORE, ERROR_OPTION_INVALID_PAIR, '=' . BOGUS, OPTION_RESTORE_RECOVERY_SETTING);
        }

        if (BackRestTestCommon_Run(++$iRun, OP_RESTORE . ' invalid value ' . OPTION_RESTORE_RECOVERY_SETTING))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_RESTORE_RECOVERY_SETTING, BOGUS . '=');

            configLoadExpect($oOption, OP_RESTORE, ERROR_OPTION_INVALID_PAIR, BOGUS . '=', OPTION_RESTORE_RECOVERY_SETTING);
        }

        if (BackRestTestCommon_Run(++$iRun, OP_RESTORE . ' valid value ' . OPTION_RESTORE_RECOVERY_SETTING))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_RESTORE_RECOVERY_SETTING, 'primary-conn-info=db.domain.net');

            configLoadExpect($oOption, OP_RESTORE);
            optionTestExpect(OPTION_RESTORE_RECOVERY_SETTING, 'db.domain.net', 'primary-conn-info');
        }

        if (BackRestTestCommon_Run(++$iRun, OP_RESTORE . ' values passed to ' . OP_ARCHIVE_GET))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db path/main');
            optionSetTest($oOption, OPTION_REPO_PATH, '/repo');
            optionSetTest($oOption, OPTION_BACKUP_HOST, 'db.mydomain.com');

            configLoadExpect($oOption, OP_RESTORE);

            my $strCommand = operationWrite(OP_ARCHIVE_GET);
            my $strExpectedCommand = abs_path($0) . " --backup-host=db.mydomain.com \"--db-path=/db path/main\"" .
                                     " --repo-path=/repo --stanza=main " . OP_ARCHIVE_GET;

            if ($strCommand ne $strExpectedCommand)
            {
                confess "expected command '${strExpectedCommand}' but got '${strCommand}'";
            }
        }

        if (BackRestTestCommon_Run(++$iRun, OP_BACKUP . ' valid value ' . OPTION_COMMAND_PSQL))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetTest($oOption, OPTION_COMMAND_PSQL, '/psql -X %option%');
            optionSetTest($oOption, OPTION_COMMAND_PSQL_OPTION, '--port 5432');

            configLoadExpect($oOption, OP_BACKUP);
            optionTestExpect(OPTION_COMMAND_PSQL, '/psql -X --port 5432');
        }

        if (BackRestTestCommon_Run(++$iRun, OP_BACKUP . ' default value ' . OPTION_COMMAND_REMOTE))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetTest($oOption, OPTION_COMMAND_PSQL, '/psql -X %option%');
            optionSetTest($oOption, OPTION_COMMAND_PSQL_OPTION, '--port 5432');

            configLoadExpect($oOption, OP_BACKUP);
            optionTestExpect(OPTION_COMMAND_REMOTE, dirname(abs_path($0)) . '/pg_backrest_remote.pl');
        }
    }

    #-------------------------------------------------------------------------------------------------------------------------------
    # Test mixed command-line/config
    #-------------------------------------------------------------------------------------------------------------------------------
    if ($strTest eq 'all' || $strTest eq 'config')
    {
        $iRun = 0;
        &log(INFO, "Config module\n");

        BackRestTestCommon_Create();

        if (BackRestTestCommon_Run(++$iRun, 'set and negate option ' . OPTION_CONFIG))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetTest($oOption, OPTION_CONFIG, '/dude/dude.conf');
            optionSetBoolTest($oOption, OPTION_CONFIG, false);

            configLoadExpect($oOption, OP_BACKUP, ERROR_OPTION_NEGATE, OPTION_CONFIG);
        }

        if (BackRestTestCommon_Run(++$iRun, 'option ' . OPTION_CONFIG))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetBoolTest($oOption, OPTION_CONFIG, false);

            configLoadExpect($oOption, OP_BACKUP);
            optionTestExpect(OPTION_CONFIG);
        }

        if (BackRestTestCommon_Run(++$iRun, 'default option ' . OPTION_CONFIG))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');

            configLoadExpect($oOption, OP_BACKUP);
            optionTestExpect(OPTION_CONFIG, OPTION_DEFAULT_CONFIG);
        }

        if (BackRestTestCommon_Run(++$iRun, 'config file is a path'))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetTest($oOption, OPTION_CONFIG, BackRestTestCommon_TestPathGet());

            configLoadExpect($oOption, OP_BACKUP, ERROR_FILE_INVALID, BackRestTestCommon_TestPathGet());
        }

        if (BackRestTestCommon_Run(++$iRun, 'load from config stanza section - option ' . OPTION_THREAD_MAX))
        {
            $oConfig = {};
            $$oConfig{"$strStanza:" . &OP_BACKUP}{&OPTION_THREAD_MAX} = 2;
            ini_save($strConfigFile, $oConfig);

            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetTest($oOption, OPTION_CONFIG, $strConfigFile);

            configLoadExpect($oOption, OP_BACKUP);
            optionTestExpect(OPTION_THREAD_MAX, 2);
        }

        if (BackRestTestCommon_Run(++$iRun, 'load from config stanza inherited section - option ' . OPTION_THREAD_MAX))
        {
            $oConfig = {};
            $$oConfig{"$strStanza:" . &CONFIG_SECTION_GENERAL}{&OPTION_THREAD_MAX} = 3;
            ini_save($strConfigFile, $oConfig);

            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetTest($oOption, OPTION_CONFIG, $strConfigFile);

            configLoadExpect($oOption, OP_BACKUP);
            optionTestExpect(OPTION_THREAD_MAX, 3);
        }


        if (BackRestTestCommon_Run(++$iRun, 'load from config global section - option ' . OPTION_THREAD_MAX))
        {
            $oConfig = {};
            $$oConfig{&CONFIG_GLOBAL . ':' . &OP_BACKUP}{&OPTION_THREAD_MAX} = 2;
            ini_save($strConfigFile, $oConfig);

            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetTest($oOption, OPTION_CONFIG, $strConfigFile);

            configLoadExpect($oOption, OP_BACKUP);
            optionTestExpect(OPTION_THREAD_MAX, 2);
        }

        if (BackRestTestCommon_Run(++$iRun, 'load from config global inherited section - option ' . OPTION_THREAD_MAX))
        {
            $oConfig = {};
            $$oConfig{&CONFIG_GLOBAL . ':' . &CONFIG_SECTION_GENERAL}{&OPTION_THREAD_MAX} = 5;
            ini_save($strConfigFile, $oConfig);

            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetTest($oOption, OPTION_CONFIG, $strConfigFile);

            configLoadExpect($oOption, OP_BACKUP);
            optionTestExpect(OPTION_THREAD_MAX, 5);
        }

        if (BackRestTestCommon_Run(++$iRun, 'default - option ' . OPTION_THREAD_MAX))
        {
            $oConfig = {};
            ini_save($strConfigFile, $oConfig);

            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetTest($oOption, OPTION_CONFIG, $strConfigFile);

            configLoadExpect($oOption, OP_BACKUP);
            optionTestExpect(OPTION_THREAD_MAX, 1);
        }

        if (BackRestTestCommon_Run(++$iRun, 'command-line override - option ' . OPTION_THREAD_MAX))
        {
            $oConfig = {};
            $$oConfig{&CONFIG_GLOBAL . ':' . &CONFIG_SECTION_GENERAL}{&OPTION_THREAD_MAX} = 9;
            ini_save($strConfigFile, $oConfig);

            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetTest($oOption, OPTION_THREAD_MAX, 7);
            optionSetTest($oOption, OPTION_CONFIG, $strConfigFile);

            configLoadExpect($oOption, OP_BACKUP);
            optionTestExpect(OPTION_THREAD_MAX, 7);
        }

        if (BackRestTestCommon_Run(++$iRun, 'invalid boolean - option ' . OPTION_HARDLINK))
        {
            $oConfig = {};
            $$oConfig{&CONFIG_GLOBAL . ':' . &OP_BACKUP}{&OPTION_HARDLINK} = 'Y';
            ini_save($strConfigFile, $oConfig);

            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetTest($oOption, OPTION_CONFIG, $strConfigFile);

            configLoadExpect($oOption, OP_BACKUP, ERROR_OPTION_INVALID_VALUE, 'Y', OPTION_HARDLINK);
        }

        if (BackRestTestCommon_Run(++$iRun, 'invalid value - option ' . OPTION_LOG_LEVEL_CONSOLE))
        {
            $oConfig = {};
            $$oConfig{&CONFIG_GLOBAL . ':' . &CONFIG_SECTION_LOG}{&OPTION_LOG_LEVEL_CONSOLE} = BOGUS;
            ini_save($strConfigFile, $oConfig);

            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetTest($oOption, OPTION_CONFIG, $strConfigFile);

            configLoadExpect($oOption, OP_BACKUP, ERROR_OPTION_INVALID_VALUE, BOGUS, OPTION_LOG_LEVEL_CONSOLE);
        }

        if (BackRestTestCommon_Run(++$iRun, 'valid value - option ' . OPTION_LOG_LEVEL_CONSOLE))
        {
            $oConfig = {};
            $$oConfig{&CONFIG_GLOBAL . ':' . &CONFIG_SECTION_LOG}{&OPTION_LOG_LEVEL_CONSOLE} = lc(INFO);
            ini_save($strConfigFile, $oConfig);

            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_CONFIG, $strConfigFile);

            configLoadExpect($oOption, OP_RESTORE);
        }

        if (BackRestTestCommon_Run(++$iRun, 'archive-push - option ' . OPTION_LOG_LEVEL_CONSOLE))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_CONFIG, $strConfigFile);

            configLoadExpect($oOption, OP_ARCHIVE_PUSH);
        }

        if (BackRestTestCommon_Run(++$iRun, OP_EXPIRE . ' ' . OPTION_RETENTION_FULL))
        {
            $oConfig = {};
            $$oConfig{&CONFIG_GLOBAL . ':' . &CONFIG_SECTION_EXPIRE}{&OPTION_RETENTION_FULL} = 2;
            ini_save($strConfigFile, $oConfig);

            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_CONFIG, $strConfigFile);

            configLoadExpect($oOption, OP_EXPIRE);
            optionTestExpect(OPTION_RETENTION_FULL, 2);
        }

        if (BackRestTestCommon_Run(++$iRun, OP_BACKUP . ' option ' . OPTION_COMPRESS))
        {
            $oConfig = {};
            $$oConfig{&CONFIG_GLOBAL . ':' . &CONFIG_SECTION_BACKUP}{&OPTION_COMPRESS} = 'n';
            ini_save($strConfigFile, $oConfig);

            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetTest($oOption, OPTION_CONFIG, $strConfigFile);

            configLoadExpect($oOption, OP_BACKUP);
            optionTestExpect(OPTION_COMPRESS, false);
        }

        if (BackRestTestCommon_Run(++$iRun, OP_RESTORE . ' option ' . OPTION_RESTORE_RECOVERY_SETTING))
        {
            $oConfig = {};
            $$oConfig{&CONFIG_GLOBAL . ':' . &CONFIG_SECTION_RESTORE_RECOVERY_SETTING}{'archive-command'} = '/path/to/pg_backrest.pl';
            ini_save($strConfigFile, $oConfig);

            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_CONFIG, $strConfigFile);

            configLoadExpect($oOption, OP_RESTORE);
            optionTestExpect(OPTION_RESTORE_RECOVERY_SETTING, '/path/to/pg_backrest.pl', 'archive-command');
        }

        if (BackRestTestCommon_Run(++$iRun, OP_RESTORE . ' option ' . OPTION_RESTORE_RECOVERY_SETTING))
        {
            $oConfig = {};
            $$oConfig{$strStanza . ':' . &CONFIG_SECTION_RESTORE_RECOVERY_SETTING}{'standby-mode'} = 'on';
            ini_save($strConfigFile, $oConfig);

            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_CONFIG, $strConfigFile);

            configLoadExpect($oOption, OP_ARCHIVE_GET);
        }

        if (BackRestTestCommon_Run(++$iRun, OP_BACKUP . ' option ' . OPTION_DB_PATH))
        {
            $oConfig = {};
            $$oConfig{$strStanza}{&OPTION_DB_PATH} = '/path/to/db';
            ini_save($strConfigFile, $oConfig);

            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_CONFIG, $strConfigFile);

            configLoadExpect($oOption, OP_BACKUP);
            optionTestExpect(OPTION_DB_PATH, '/path/to/db');
        }

        if (BackRestTestCommon_Run(++$iRun, OP_ARCHIVE_PUSH . ' option ' . OPTION_DB_PATH))
        {
            $oConfig = {};
            $$oConfig{$strStanza}{&OPTION_DB_PATH} = '/path/to/db';
            ini_save($strConfigFile, $oConfig);

            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_CONFIG, $strConfigFile);

            configLoadExpect($oOption, OP_ARCHIVE_PUSH);
            optionTestExpect(OPTION_DB_PATH, '/path/to/db');
        }

        if (BackRestTestCommon_Run(++$iRun, OP_BACKUP . ' option ' . OPTION_REPO_PATH))
        {
            $oConfig = {};
            $$oConfig{&CONFIG_GLOBAL . ':' . &CONFIG_SECTION_GENERAL}{&OPTION_REPO_PATH} = '/repo';
            ini_save($strConfigFile, $oConfig);

            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetTest($oOption, OPTION_CONFIG, $strConfigFile);

            configLoadExpect($oOption, OP_BACKUP);
            optionTestExpect(OPTION_REPO_PATH, '/repo');
        }

        if (BackRestTestCommon_Run(++$iRun, OP_BACKUP . ' valid value ' . OPTION_COMMAND_PSQL))
        {
            $oConfig = {};
            $$oConfig{&CONFIG_GLOBAL . ':' . &CONFIG_SECTION_COMMAND}{&OPTION_COMMAND_PSQL} = '/psql -X %option%';
            $$oConfig{&CONFIG_GLOBAL . ':' . &CONFIG_SECTION_COMMAND}{&OPTION_COMMAND_PSQL_OPTION} = '--port=5432';
            ini_save($strConfigFile, $oConfig);

            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetTest($oOption, OPTION_CONFIG, $strConfigFile);

            configLoadExpect($oOption, OP_BACKUP);
            optionTestExpect(OPTION_COMMAND_PSQL, '/psql -X --port=5432');
        }

        # Cleanup
        if (BackRestTestCommon_Cleanup())
        {
            &log(INFO, 'cleanup');
            BackRestTestCommon_Drop(true);
        }
    }
}

1;
