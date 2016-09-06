####################################################################################################################################
# ConfigTest.pm - Unit Tests for pgBackRest::Param and pgBackRest::Config::Config
####################################################################################################################################
package pgBackRestTest::Config::ConfigTest;

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Cwd qw(abs_path);
use Exporter qw(import);

use pgBackRest::Common::Exception;
use pgBackRest::Common::Ini;
use pgBackRest::Common::Log;
use pgBackRest::Config::Config;

use pgBackRestTest::Backup::Common::HostBaseTest;
use pgBackRestTest::Common::HostGroupTest;
use pgBackRestTest::CommonTest;

sub optionSetTest
{
    my $oOption = shift;
    my $strKey = shift;
    my $strValue = shift;

    $$oOption{option}{$strKey} = $strValue;
}

sub optionBoolSetTest
{
    my $oOption = shift;
    my $strKey = shift;
    my $bValue = shift;

    $$oOption{boolean}{$strKey} = defined($bValue) ? $bValue : true;
}

sub commandSetTest
{
    my $oOption = shift;
    my $strCommand = shift;

    $$oOption{command} = $strCommand;
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
        foreach my $strKey (keys(%{$$oOption{boolean}}))
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
        foreach my $strKey (keys(%{$$oOption{option}}))
        {
            $ARGV[@ARGV] = "--${strKey}=$$oOption{option}{$strKey}";
        }
    }

    $ARGV[@ARGV] = $$oOption{command};

    &log(INFO, "    command line: " . join(" ", @ARGV));

    %$oOption = ();
}

sub configLoadExpect
{
    my $oOption = shift;
    my $strCommand = shift;
    my $iExpectedError = shift;
    my $strErrorParam1 = shift;
    my $strErrorParam2 = shift;
    my $strErrorParam3 = shift;

    my $oOptionRuleExpected = optionRuleGet();

    commandSetTest($oOption, $strCommand);
    argvWriteTest($oOption);

    my $bErrorFound = false;

    eval
    {
        configLoad();
        return true;
    }
    or do
    {
        my $oException = $EVAL_ERROR;

        if (!defined($iExpectedError))
        {
            confess $oException;
        }

        $bErrorFound = true;

        if (isException($oException))
        {
            if ($oException->code() != $iExpectedError)
            {
                confess "expected error ${iExpectedError} from configLoad but got " . $oException->code() .
                        " '" . $oException->message() . "'";
            }

            my $strError;

            if ($iExpectedError == ERROR_OPTION_REQUIRED)
            {
                $strError = "${strCommand} command requires option: ${strErrorParam1}" .
                            (defined($strErrorParam2) ? "\nHINT: ${strErrorParam2}" : '');
            }
            elsif ($iExpectedError == ERROR_COMMAND_REQUIRED)
            {
                $strError = "command must be specified";
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
                $strError = "'${strErrorParam1}' is not valid for '${strErrorParam2}' option" .
                            (defined($strErrorParam3) ? "\nHINT: ${strErrorParam3}." : '');
            }
            elsif ($iExpectedError == ERROR_OPTION_MULTIPLE_VALUE)
            {
                $strError = "option '${strErrorParam1}' cannot be specified multiple times";
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
                confess
                    "must construct message for error ${iExpectedError}, use this as an example: '" . $oException->message() . "'";
            }

            if ($oException->message() ne $strError)
            {
                confess "expected error message \"${strError}\" from configLoad but got \"" . $oException->message() . "\"";
            }
        }
        else
        {
            confess "configLoad should throw pgBackRest::Common::Exception:\n$oException";
        }
    };

    if (!$bErrorFound && defined($iExpectedError))
    {
        confess "expected error ${iExpectedError} from configLoad but got success";
    }
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
# configTestRun
####################################################################################################################################
sub configTestRun
{
    my $strTest = shift;
    my $bVmOut = shift;

    # Setup test variables
    my $oHostGroup = hostGroupGet();

    my $iRun;
    my $bCreate;
    my $strStanza = 'main';
    my $oOption = {};
    my $oConfig = {};
    my @oyArray;
    my $strTestPath = $oHostGroup->paramGet(HOST_PARAM_TEST_PATH);
    my $strConfigFile = "${strTestPath}/pgbackrest.conf";

    use constant BOGUS => 'bogus';

    # Print test banner
    if (!$bVmOut)
    {
        &log(INFO, 'CONFIG MODULE ******************************************************************');
    }

    #-------------------------------------------------------------------------------------------------------------------------------
    # Test command-line options
    #-------------------------------------------------------------------------------------------------------------------------------
    my $strThisTest = 'option';

    if ($strTest eq 'all' || $strTest eq $strThisTest)
    {
        $iRun = 0;

        if (!$bVmOut)
        {
            &log(INFO, "Test ${strThisTest}\n");
        }

        if (testRun(++$iRun, 'backup with no stanza'))
        {
            optionSetTest($oOption, OPTION_DB_PATH, '/db');

            configLoadExpect($oOption, CMD_BACKUP, ERROR_OPTION_REQUIRED, OPTION_STANZA);
        }

        if (testRun(++$iRun, 'backup with boolean stanza'))
        {
            optionBoolSetTest($oOption, OPTION_STANZA);

            configLoadExpect($oOption, CMD_BACKUP, ERROR_COMMAND_REQUIRED);
        }

        if (testRun(++$iRun, 'backup type defaults to ' . BACKUP_TYPE_INCR))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');

            configLoadExpect($oOption, CMD_BACKUP);
            optionTestExpect(OPTION_TYPE, BACKUP_TYPE_INCR);
        }

        if (testRun(++$iRun, 'backup type set to ' . BACKUP_TYPE_FULL))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetTest($oOption, OPTION_TYPE, BACKUP_TYPE_FULL);

            configLoadExpect($oOption, CMD_BACKUP);
            optionTestExpect(OPTION_TYPE, BACKUP_TYPE_FULL);
        }

        if (testRun(++$iRun, 'backup type invalid'))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetTest($oOption, OPTION_TYPE, BOGUS);

            configLoadExpect($oOption, CMD_BACKUP, ERROR_OPTION_INVALID_VALUE, BOGUS, OPTION_TYPE);
        }

        if (testRun(++$iRun, 'backup invalid force'))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionBoolSetTest($oOption, OPTION_FORCE);

            configLoadExpect($oOption, CMD_BACKUP, ERROR_OPTION_INVALID, OPTION_FORCE, 'no-' . OPTION_ONLINE);
        }

        if (testRun(++$iRun, 'backup valid force'))
        {
            # $oOption = {};
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionBoolSetTest($oOption, OPTION_ONLINE, false);
            optionBoolSetTest($oOption, OPTION_FORCE);

            configLoadExpect($oOption, CMD_BACKUP);
            optionTestExpect(OPTION_ONLINE, false);
            optionTestExpect(OPTION_FORCE, true);
        }

        if (testRun(++$iRun, 'backup invalid value for ' . OPTION_TEST_DELAY))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionBoolSetTest($oOption, OPTION_TEST);
            optionSetTest($oOption, OPTION_TEST_DELAY, BOGUS);

            configLoadExpect($oOption, CMD_BACKUP, ERROR_OPTION_INVALID_VALUE, BOGUS, OPTION_TEST_DELAY);
        }

        if (testRun(++$iRun, 'backup invalid ' . OPTION_TEST_DELAY))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetTest($oOption, OPTION_TEST_DELAY, 5);

            configLoadExpect($oOption, CMD_BACKUP, ERROR_OPTION_INVALID, OPTION_TEST_DELAY, OPTION_TEST);
        }

        if (testRun(++$iRun, 'backup check ' . OPTION_TEST_DELAY . ' undef'))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');

            configLoadExpect($oOption, CMD_BACKUP);
            optionTestExpect(OPTION_TEST_DELAY);
        }

        if (testRun(++$iRun, 'restore invalid ' . OPTION_TARGET))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetTest($oOption, OPTION_TYPE, RECOVERY_TYPE_DEFAULT);
            optionSetTest($oOption, OPTION_TARGET, BOGUS);

            @oyArray = (RECOVERY_TYPE_NAME, RECOVERY_TYPE_TIME, RECOVERY_TYPE_XID);
            configLoadExpect($oOption, CMD_RESTORE, ERROR_OPTION_INVALID, OPTION_TARGET, OPTION_TYPE, \@oyArray);
        }

        if (testRun(++$iRun, 'restore ' . OPTION_TARGET))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetTest($oOption, OPTION_TYPE, RECOVERY_TYPE_NAME);
            optionSetTest($oOption, OPTION_TARGET, BOGUS);

            configLoadExpect($oOption, CMD_RESTORE);
            optionTestExpect(OPTION_TYPE, RECOVERY_TYPE_NAME);
            optionTestExpect(OPTION_TARGET, BOGUS);
            optionTestExpect(OPTION_TARGET_TIMELINE);
        }

        if (testRun(++$iRun, 'invalid string ' . OPTION_PROCESS_MAX))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetTest($oOption, OPTION_PROCESS_MAX, BOGUS);

            configLoadExpect($oOption, CMD_BACKUP, ERROR_OPTION_INVALID_VALUE, BOGUS, OPTION_PROCESS_MAX);
        }

        if (testRun(++$iRun, 'invalid float ' . OPTION_PROCESS_MAX))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetTest($oOption, OPTION_PROCESS_MAX, '0.0');

            configLoadExpect($oOption, CMD_BACKUP, ERROR_OPTION_INVALID_VALUE, '0.0', OPTION_PROCESS_MAX);
        }

        if (testRun(++$iRun, 'valid ' . OPTION_PROCESS_MAX))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetTest($oOption, OPTION_PROCESS_MAX, '2');

            configLoadExpect($oOption, CMD_BACKUP);
        }

        if (testRun(++$iRun, 'valid thread-max'))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetTest($oOption, 'thread-max', '2');

            configLoadExpect($oOption, CMD_BACKUP);
            optionTestExpect(OPTION_PROCESS_MAX, 2);
        }

        if (testRun(++$iRun, 'valid float ' . OPTION_TEST_DELAY))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionBoolSetTest($oOption, OPTION_TEST);
            optionSetTest($oOption, OPTION_TEST_DELAY, '0.25');

            configLoadExpect($oOption, CMD_BACKUP);
        }

        if (testRun(++$iRun, 'valid int ' . OPTION_TEST_DELAY))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionBoolSetTest($oOption, OPTION_TEST);
            optionSetTest($oOption, OPTION_TEST_DELAY, 3);

            configLoadExpect($oOption, CMD_BACKUP);
        }

        if (testRun(++$iRun, 'restore valid ' . OPTION_TARGET_TIMELINE))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetTest($oOption, OPTION_TARGET_TIMELINE, 2);

            configLoadExpect($oOption, CMD_RESTORE);
        }

        if (testRun(++$iRun, 'invalid ' . OPTION_BUFFER_SIZE))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_BUFFER_SIZE, '512');

            configLoadExpect($oOption, CMD_RESTORE, ERROR_OPTION_INVALID_RANGE, '512', OPTION_BUFFER_SIZE);
        }

        if (testRun(++$iRun, CMD_BACKUP . ' invalid option ' . OPTION_RETENTION_ARCHIVE_TYPE))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetTest($oOption, OPTION_RETENTION_ARCHIVE_TYPE, BOGUS);

            configLoadExpect($oOption, CMD_BACKUP, ERROR_OPTION_INVALID, OPTION_RETENTION_ARCHIVE_TYPE, OPTION_RETENTION_ARCHIVE);
        }

        if (testRun(++$iRun, CMD_BACKUP . ' invalid value ' . OPTION_RETENTION_ARCHIVE_TYPE))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetTest($oOption, OPTION_RETENTION_ARCHIVE, 3);
            optionSetTest($oOption, OPTION_RETENTION_ARCHIVE_TYPE, BOGUS);

            configLoadExpect($oOption, CMD_BACKUP, ERROR_OPTION_INVALID_VALUE, BOGUS, OPTION_RETENTION_ARCHIVE_TYPE);
        }

        if (testRun(++$iRun, CMD_BACKUP . ' invalid value ' . OPTION_PROTOCOL_TIMEOUT))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetTest($oOption, OPTION_DB_TIMEOUT, 5);
            optionSetTest($oOption, OPTION_PROTOCOL_TIMEOUT, 4);

            configLoadExpect(
                $oOption, CMD_BACKUP, ERROR_OPTION_INVALID_VALUE, 4, OPTION_PROTOCOL_TIMEOUT,
                "'protocol-timeout' option should be greater than 'db-timeout' option");
        }

        if (testRun(++$iRun, CMD_BACKUP . ' valid value ' . OPTION_RETENTION_ARCHIVE_TYPE))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetTest($oOption, OPTION_RETENTION_ARCHIVE, 1);
            optionSetTest($oOption, OPTION_RETENTION_ARCHIVE_TYPE, BACKUP_TYPE_FULL);

            configLoadExpect($oOption, CMD_BACKUP);
            optionTestExpect(OPTION_RETENTION_ARCHIVE, 1);
            optionTestExpect(OPTION_RETENTION_ARCHIVE_TYPE, BACKUP_TYPE_FULL);
        }

        if (testRun(++$iRun, CMD_RESTORE . ' invalid value ' . OPTION_RESTORE_RECOVERY_OPTION))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetTest($oOption, OPTION_RESTORE_RECOVERY_OPTION, '=');

            configLoadExpect($oOption, CMD_RESTORE, ERROR_OPTION_INVALID_PAIR, '=', OPTION_RESTORE_RECOVERY_OPTION);
        }

        if (testRun(++$iRun, CMD_RESTORE . ' invalid value ' . OPTION_RESTORE_RECOVERY_OPTION))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetTest($oOption, OPTION_RESTORE_RECOVERY_OPTION, '=' . BOGUS);

            configLoadExpect($oOption, CMD_RESTORE, ERROR_OPTION_INVALID_PAIR, '=' . BOGUS, OPTION_RESTORE_RECOVERY_OPTION);
        }

        if (testRun(++$iRun, CMD_RESTORE . ' invalid value ' . OPTION_RESTORE_RECOVERY_OPTION))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetTest($oOption, OPTION_RESTORE_RECOVERY_OPTION, BOGUS . '=');

            configLoadExpect($oOption, CMD_RESTORE, ERROR_OPTION_INVALID_PAIR, BOGUS . '=', OPTION_RESTORE_RECOVERY_OPTION);
        }

        if (testRun(++$iRun, CMD_RESTORE . ' valid value ' . OPTION_RESTORE_RECOVERY_OPTION))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetTest($oOption, OPTION_RESTORE_RECOVERY_OPTION, 'primary-conn-info=db.domain.net');

            configLoadExpect($oOption, CMD_RESTORE);
            optionTestExpect(OPTION_RESTORE_RECOVERY_OPTION, 'db.domain.net', 'primary-conn-info');
        }

        if (testRun(++$iRun, CMD_RESTORE . ' values passed to ' . CMD_ARCHIVE_GET))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db path/main');
            optionSetTest($oOption, OPTION_REPO_PATH, '/repo');
            optionSetTest($oOption, OPTION_BACKUP_HOST, 'db.mydomain.com');

            configLoadExpect($oOption, CMD_RESTORE);

            my $strCommand = commandWrite(CMD_ARCHIVE_GET);
            my $strExpectedCommand = abs_path($0) . " --backup-host=db.mydomain.com \"--db-path=/db path/main\"" .
                                     " --repo-path=/repo --stanza=main " . CMD_ARCHIVE_GET;

            if ($strCommand ne $strExpectedCommand)
            {
                confess "expected command '${strExpectedCommand}' but got '${strCommand}'";
            }
        }

        if (testRun(++$iRun, CMD_BACKUP . ' default value ' . OPTION_DB_CMD))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_HOST, 'db');
            optionSetTest($oOption, OPTION_DB_PATH, '/db');

            configLoadExpect($oOption, CMD_BACKUP);
            optionTestExpect(OPTION_DB_CMD, abs_path($0));
        }

        if (testRun(++$iRun, CMD_BACKUP . ' missing option ' . OPTION_DB_PATH))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);

            configLoadExpect($oOption, CMD_BACKUP, ERROR_OPTION_REQUIRED, OPTION_DB_PATH, 'does this stanza exist?');
        }

        if (testRun(++$iRun, CMD_BACKUP . ' automatically increase ' . OPTION_PROTOCOL_TIMEOUT))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetTest($oOption, OPTION_DB_TIMEOUT, OPTION_DEFAULT_PROTOCOL_TIMEOUT + 30);

            configLoadExpect($oOption, CMD_BACKUP);
            optionTestExpect(OPTION_PROTOCOL_TIMEOUT, OPTION_DEFAULT_PROTOCOL_TIMEOUT + 60);
        }
    }

    #-------------------------------------------------------------------------------------------------------------------------------
    # Test mixed command-line/config
    #-------------------------------------------------------------------------------------------------------------------------------
    $strThisTest = 'config';

    if ($strTest eq 'all' || $strTest eq $strThisTest)
    {
        $iRun = 0;

        if (!$bVmOut)
        {
            &log(INFO, "Test ${strThisTest}\n");
        }

        if (testRun(++$iRun, 'set and negate option ' . OPTION_CONFIG))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetTest($oOption, OPTION_CONFIG, '/dude/dude.conf');
            optionBoolSetTest($oOption, OPTION_CONFIG, false);

            configLoadExpect($oOption, CMD_BACKUP, ERROR_OPTION_NEGATE, OPTION_CONFIG);
        }

        if (testRun(++$iRun, 'option ' . OPTION_CONFIG))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionBoolSetTest($oOption, OPTION_CONFIG, false);

            configLoadExpect($oOption, CMD_BACKUP);
            optionTestExpect(OPTION_CONFIG);
        }

        if (testRun(++$iRun, 'default option ' . OPTION_CONFIG))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');

            configLoadExpect($oOption, CMD_BACKUP);
            optionTestExpect(OPTION_CONFIG, OPTION_DEFAULT_CONFIG);
        }

        if (testRun(++$iRun, 'config file is a path'))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetTest($oOption, OPTION_CONFIG, $strTestPath);

            configLoadExpect($oOption, CMD_BACKUP, ERROR_FILE_INVALID, $strTestPath);
        }

        if (testRun(++$iRun, 'load from config stanza command section - option ' . OPTION_PROCESS_MAX))
        {
            $oConfig = {};
            $$oConfig{"${strStanza}:" . &CMD_BACKUP}{&OPTION_PROCESS_MAX} = 2;
            iniSave($strConfigFile, $oConfig, true);

            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetTest($oOption, OPTION_CONFIG, $strConfigFile);

            configLoadExpect($oOption, CMD_BACKUP);
            optionTestExpect(OPTION_PROCESS_MAX, 2);
        }

        if (testRun(++$iRun, 'load from config stanza section - option ' . OPTION_PROCESS_MAX))
        {
            $oConfig = {};
            $$oConfig{$strStanza}{&OPTION_PROCESS_MAX} = 3;
            iniSave($strConfigFile, $oConfig, true);

            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetTest($oOption, OPTION_CONFIG, $strConfigFile);

            configLoadExpect($oOption, CMD_BACKUP);
            optionTestExpect(OPTION_PROCESS_MAX, 3);
        }

        if (testRun(++$iRun, 'load from config global command section - option thread-max'))
        {
            $oConfig = {};
            $$oConfig{&CONFIG_SECTION_GLOBAL . ':' . &CMD_BACKUP}{'thread-max'} = 2;
            iniSave($strConfigFile, $oConfig, true);

            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetTest($oOption, OPTION_CONFIG, $strConfigFile);

            configLoadExpect($oOption, CMD_BACKUP);
            optionTestExpect(OPTION_PROCESS_MAX, 2);
        }

        if (testRun(++$iRun, 'load from config global section - option ' . OPTION_PROCESS_MAX))
        {
            $oConfig = {};
            $$oConfig{&CONFIG_SECTION_GLOBAL}{&OPTION_PROCESS_MAX} = 5;
            iniSave($strConfigFile, $oConfig, true);

            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetTest($oOption, OPTION_CONFIG, $strConfigFile);

            configLoadExpect($oOption, CMD_BACKUP);
            optionTestExpect(OPTION_PROCESS_MAX, 5);
        }

        if (testRun(++$iRun, 'default - option ' . OPTION_PROCESS_MAX))
        {
            $oConfig = {};
            iniSave($strConfigFile, $oConfig, true);

            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetTest($oOption, OPTION_CONFIG, $strConfigFile);

            configLoadExpect($oOption, CMD_BACKUP);
            optionTestExpect(OPTION_PROCESS_MAX, 1);
        }

        if (testRun(++$iRun, 'command-line override - option ' . OPTION_PROCESS_MAX))
        {
            $oConfig = {};
            $$oConfig{&CONFIG_SECTION_GLOBAL}{&OPTION_PROCESS_MAX} = 9;
            iniSave($strConfigFile, $oConfig, true);

            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetTest($oOption, OPTION_PROCESS_MAX, 7);
            optionSetTest($oOption, OPTION_CONFIG, $strConfigFile);

            configLoadExpect($oOption, CMD_BACKUP);
            optionTestExpect(OPTION_PROCESS_MAX, 7);
        }

        if (testRun(++$iRun, 'invalid boolean - option ' . OPTION_HARDLINK))
        {
            $oConfig = {};
            $$oConfig{&CONFIG_SECTION_GLOBAL . ':' . &CMD_BACKUP}{&OPTION_HARDLINK} = 'Y';
            iniSave($strConfigFile, $oConfig, true);

            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetTest($oOption, OPTION_CONFIG, $strConfigFile);

            configLoadExpect($oOption, CMD_BACKUP, ERROR_OPTION_INVALID_VALUE, 'Y', OPTION_HARDLINK);
        }

        if (testRun(++$iRun, 'invalid value - option ' . OPTION_LOG_LEVEL_CONSOLE))
        {
            $oConfig = {};
            $$oConfig{&CONFIG_SECTION_GLOBAL}{&OPTION_LOG_LEVEL_CONSOLE} = BOGUS;
            iniSave($strConfigFile, $oConfig, true);

            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetTest($oOption, OPTION_CONFIG, $strConfigFile);

            configLoadExpect($oOption, CMD_BACKUP, ERROR_OPTION_INVALID_VALUE, BOGUS, OPTION_LOG_LEVEL_CONSOLE);
        }

        if (testRun(++$iRun, 'valid value - option ' . OPTION_LOG_LEVEL_CONSOLE))
        {
            $oConfig = {};
            $$oConfig{&CONFIG_SECTION_GLOBAL}{&OPTION_LOG_LEVEL_CONSOLE} = lc(INFO);
            iniSave($strConfigFile, $oConfig, true);

            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetTest($oOption, OPTION_CONFIG, $strConfigFile);

            configLoadExpect($oOption, CMD_RESTORE);
        }

        if (testRun(++$iRun, 'archive-push - option ' . OPTION_LOG_LEVEL_CONSOLE))
        {
            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_CONFIG, $strConfigFile);

            configLoadExpect($oOption, CMD_ARCHIVE_PUSH);
        }

        if (testRun(++$iRun, CMD_EXPIRE . ' ' . OPTION_RETENTION_FULL))
        {
            $oConfig = {};
            $$oConfig{"${strStanza}:" . &CMD_EXPIRE}{&OPTION_RETENTION_FULL} = 2;
            iniSave($strConfigFile, $oConfig, true);

            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_CONFIG, $strConfigFile);

            configLoadExpect($oOption, CMD_EXPIRE);
            optionTestExpect(OPTION_RETENTION_FULL, 2);
        }

        if (testRun(++$iRun, CMD_BACKUP . ' option ' . OPTION_COMPRESS))
        {
            $oConfig = {};
            $$oConfig{&CONFIG_SECTION_GLOBAL . ':' . &CMD_BACKUP}{&OPTION_COMPRESS} = 'n';
            iniSave($strConfigFile, $oConfig, true);

            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetTest($oOption, OPTION_CONFIG, $strConfigFile);

            configLoadExpect($oOption, CMD_BACKUP);
            optionTestExpect(OPTION_COMPRESS, false);
        }

        if (testRun(++$iRun, CMD_RESTORE . ' global option ' . OPTION_RESTORE_RECOVERY_OPTION . ' error'))
        {
            $oConfig = {};
            $$oConfig{&CONFIG_SECTION_GLOBAL . ':' . &CMD_RESTORE}{&OPTION_RESTORE_RECOVERY_OPTION} = 'bogus=';
            iniSave($strConfigFile, $oConfig, true);

            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetTest($oOption, OPTION_CONFIG, $strConfigFile);

            configLoadExpect($oOption, CMD_RESTORE, ERROR_OPTION_INVALID_VALUE, 'bogus=', OPTION_RESTORE_RECOVERY_OPTION);
        }

        if (testRun(++$iRun, CMD_RESTORE . ' global option ' . OPTION_RESTORE_RECOVERY_OPTION . ' error'))
        {
            $oConfig = {};
            $$oConfig{&CONFIG_SECTION_GLOBAL . ':' . &CMD_RESTORE}{&OPTION_RESTORE_RECOVERY_OPTION} = '=bogus';
            iniSave($strConfigFile, $oConfig, true);

            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetTest($oOption, OPTION_CONFIG, $strConfigFile);

            configLoadExpect($oOption, CMD_RESTORE, ERROR_OPTION_INVALID_VALUE, '=bogus', OPTION_RESTORE_RECOVERY_OPTION);
        }

        if (testRun(++$iRun, CMD_RESTORE . ' global option ' . OPTION_RESTORE_RECOVERY_OPTION))
        {
            $oConfig = {};
            $$oConfig{&CONFIG_SECTION_GLOBAL . ':' . &CMD_RESTORE}{&OPTION_RESTORE_RECOVERY_OPTION} =
                'archive-command=/path/to/pgbackrest';
            iniSave($strConfigFile, $oConfig, true);

            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetTest($oOption, OPTION_CONFIG, $strConfigFile);

            configLoadExpect($oOption, CMD_RESTORE);
            optionTestExpect(OPTION_RESTORE_RECOVERY_OPTION, '/path/to/pgbackrest', 'archive-command');
        }

        if (testRun(++$iRun, CMD_RESTORE . ' stanza option ' . OPTION_RESTORE_RECOVERY_OPTION))
        {
            $oConfig = {};
            $$oConfig{$strStanza}{&OPTION_RESTORE_RECOVERY_OPTION} = ['standby-mode=on', 'a=b'];
            iniSave($strConfigFile, $oConfig, true);

            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetTest($oOption, OPTION_CONFIG, $strConfigFile);

            configLoadExpect($oOption, CMD_RESTORE);
            optionTestExpect(OPTION_RESTORE_RECOVERY_OPTION, 'b', 'a');
            optionTestExpect(OPTION_RESTORE_RECOVERY_OPTION, 'on', 'standby-mode');
        }

        if (testRun(++$iRun, CMD_BACKUP . ' option ' . OPTION_DB_PATH))
        {
            $oConfig = {};
            $$oConfig{$strStanza}{&OPTION_DB_PATH} = '/path/to/db';
            iniSave($strConfigFile, $oConfig, true);

            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_CONFIG, $strConfigFile);

            configLoadExpect($oOption, CMD_BACKUP);
            optionTestExpect(OPTION_DB_PATH, '/path/to/db');
        }

        if (testRun(++$iRun, CMD_BACKUP . ' option ' . OPTION_BACKUP_ARCHIVE_CHECK))
        {
            $oConfig = {};
            $$oConfig{$strStanza}{&OPTION_DB_PATH} = '/path/to/db';
            iniSave($strConfigFile, $oConfig, true);

            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_CONFIG, $strConfigFile);
            optionBoolSetTest($oOption, OPTION_BACKUP_ARCHIVE_CHECK, false);

            configLoadExpect($oOption, CMD_BACKUP);
            optionTestExpect(OPTION_ONLINE, true);
            optionTestExpect(OPTION_BACKUP_ARCHIVE_CHECK, false);
        }

        if (testRun(++$iRun, CMD_ARCHIVE_PUSH . ' option ' . OPTION_DB_PATH))
        {
            $oConfig = {};
            $$oConfig{$strStanza}{&OPTION_DB_PATH} = '/path/to/db';
            iniSave($strConfigFile, $oConfig, true);

            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_CONFIG, $strConfigFile);

            configLoadExpect($oOption, CMD_ARCHIVE_PUSH);
            optionTestExpect(OPTION_DB_PATH, '/path/to/db');
        }

        if (testRun(++$iRun, CMD_BACKUP . ' option ' . OPTION_REPO_PATH))
        {
            $oConfig = {};
            $$oConfig{&CONFIG_SECTION_GLOBAL}{&OPTION_REPO_PATH} = '/repo';
            iniSave($strConfigFile, $oConfig, true);

            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetTest($oOption, OPTION_CONFIG, $strConfigFile);

            configLoadExpect($oOption, CMD_BACKUP);
            optionTestExpect(OPTION_REPO_PATH, '/repo');
        }

        if (testRun(++$iRun, CMD_BACKUP . ' option ' . OPTION_REPO_PATH . ' multiple times'))
        {
            $oConfig = {};
            $$oConfig{&CONFIG_SECTION_GLOBAL}{&OPTION_REPO_PATH} = ['/repo', '/repo2'];
            iniSave($strConfigFile, $oConfig, true);

            optionSetTest($oOption, OPTION_STANZA, $strStanza);
            optionSetTest($oOption, OPTION_DB_PATH, '/db');
            optionSetTest($oOption, OPTION_CONFIG, $strConfigFile);

            configLoadExpect($oOption, CMD_BACKUP, ERROR_OPTION_MULTIPLE_VALUE, OPTION_REPO_PATH);
        }

        # Cleanup
        testCleanup();
    }
}

our @EXPORT = qw(configTestRun);

1;
