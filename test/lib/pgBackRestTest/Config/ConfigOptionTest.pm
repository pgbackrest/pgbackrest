####################################################################################################################################
# ConfigOptionTest.pm - Tests for command line options in Config.pm
####################################################################################################################################
package pgBackRestTest::Config::ConfigOptionTest;
use parent 'pgBackRestTest::Config::ConfigEnvTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Cwd qw(abs_path);

use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Config::Config;

use pgBackRestTest::Common::RunTest;

####################################################################################################################################
# run
####################################################################################################################################
sub run
{
    my $self = shift;

    my $oOption = {};
    my @oyArray;

    if ($self->begin('backup with no stanza'))
    {
        $self->optionSetTest($oOption, OPTION_DB_PATH, '/db');

        $self->configLoadExpect($oOption, CMD_BACKUP, ERROR_OPTION_REQUIRED, OPTION_STANZA);
    }

    if ($self->begin('backup with boolean stanza'))
    {
        $self->optionBoolSetTest($oOption, OPTION_STANZA);

        $self->configLoadExpect($oOption, CMD_BACKUP, ERROR_COMMAND_REQUIRED);
    }

    if ($self->begin('backup type defaults to ' . BACKUP_TYPE_INCR))
    {
        $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
        $self->optionSetTest($oOption, OPTION_DB_PATH, '/db');

        $self->configLoadExpect($oOption, CMD_BACKUP);
        $self->optionTestExpect(OPTION_TYPE, BACKUP_TYPE_INCR);
    }

    if ($self->begin(CMD_BACKUP . ' invalid option ' . OPTION_ARCHIVE_ASYNC))
    {
        $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
        $self->optionSetTest($oOption, OPTION_DB_PATH, '/db');
        $self->optionBoolSetTest($oOption, OPTION_ARCHIVE_ASYNC);

        $self->configLoadExpect($oOption, CMD_BACKUP, ERROR_OPTION_COMMAND, OPTION_ARCHIVE_ASYNC, CMD_BACKUP);
    }

    if ($self->begin('backup type set to ' . BACKUP_TYPE_FULL))
    {
        $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
        $self->optionSetTest($oOption, OPTION_DB_PATH, '/db');
        $self->optionSetTest($oOption, OPTION_TYPE, BACKUP_TYPE_FULL);

        $self->configLoadExpect($oOption, CMD_BACKUP);
        $self->optionTestExpect(OPTION_TYPE, BACKUP_TYPE_FULL);
    }

    if ($self->begin('backup type invalid'))
    {
        $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
        $self->optionSetTest($oOption, OPTION_DB_PATH, '/db');
        $self->optionSetTest($oOption, OPTION_TYPE, BOGUS);

        $self->configLoadExpect($oOption, CMD_BACKUP, ERROR_OPTION_INVALID_VALUE, BOGUS, OPTION_TYPE);
    }

    if ($self->begin('backup invalid force'))
    {
        $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
        $self->optionSetTest($oOption, OPTION_DB_PATH, '/db');
        $self->optionBoolSetTest($oOption, OPTION_FORCE);

        $self->configLoadExpect($oOption, CMD_BACKUP, ERROR_OPTION_INVALID, OPTION_FORCE, 'no-' . OPTION_ONLINE);
    }

    if ($self->begin('backup valid force'))
    {
        # $oOption = {};
        $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
        $self->optionSetTest($oOption, OPTION_DB_PATH, '/db');
        $self->optionBoolSetTest($oOption, OPTION_ONLINE, false);
        $self->optionBoolSetTest($oOption, OPTION_FORCE);

        $self->configLoadExpect($oOption, CMD_BACKUP);
        $self->optionTestExpect(OPTION_ONLINE, false);
        $self->optionTestExpect(OPTION_FORCE, true);
    }

    if ($self->begin('backup invalid value for ' . OPTION_TEST_DELAY))
    {
        $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
        $self->optionSetTest($oOption, OPTION_DB_PATH, '/db');
        $self->optionBoolSetTest($oOption, OPTION_TEST);
        $self->optionSetTest($oOption, OPTION_TEST_DELAY, BOGUS);

        $self->configLoadExpect($oOption, CMD_BACKUP, ERROR_OPTION_INVALID_VALUE, BOGUS, OPTION_TEST_DELAY);
    }

    if ($self->begin('backup invalid ' . OPTION_TEST_DELAY))
    {
        $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
        $self->optionSetTest($oOption, OPTION_DB_PATH, '/db');
        $self->optionSetTest($oOption, OPTION_TEST_DELAY, 5);

        $self->configLoadExpect($oOption, CMD_BACKUP, ERROR_OPTION_INVALID, OPTION_TEST_DELAY, OPTION_TEST);
    }

    if ($self->begin('backup check ' . OPTION_TEST_DELAY . ' undef'))
    {
        $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
        $self->optionSetTest($oOption, OPTION_DB_PATH, '/db');

        $self->configLoadExpect($oOption, CMD_BACKUP);
        $self->optionTestExpect(OPTION_TEST_DELAY);
    }

    if ($self->begin('restore invalid ' . OPTION_TARGET))
    {
        $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
        $self->optionSetTest($oOption, OPTION_DB_PATH, '/db');
        $self->optionSetTest($oOption, OPTION_TYPE, RECOVERY_TYPE_DEFAULT);
        $self->optionSetTest($oOption, OPTION_TARGET, BOGUS);

        @oyArray = (RECOVERY_TYPE_NAME, RECOVERY_TYPE_TIME, RECOVERY_TYPE_XID);
        $self->configLoadExpect($oOption, CMD_RESTORE, ERROR_OPTION_INVALID, OPTION_TARGET, OPTION_TYPE, \@oyArray);
    }

    if ($self->begin('restore ' . OPTION_TARGET))
    {
        $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
        $self->optionSetTest($oOption, OPTION_DB_PATH, '/db');
        $self->optionSetTest($oOption, OPTION_TYPE, RECOVERY_TYPE_NAME);
        $self->optionSetTest($oOption, OPTION_TARGET, BOGUS);

        $self->configLoadExpect($oOption, CMD_RESTORE);
        $self->optionTestExpect(OPTION_TYPE, RECOVERY_TYPE_NAME);
        $self->optionTestExpect(OPTION_TARGET, BOGUS);
        $self->optionTestExpect(OPTION_TARGET_TIMELINE);
    }

    if ($self->begin('invalid string ' . OPTION_PROCESS_MAX))
    {
        $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
        $self->optionSetTest($oOption, OPTION_DB_PATH, '/db');
        $self->optionSetTest($oOption, OPTION_PROCESS_MAX, BOGUS);

        $self->configLoadExpect($oOption, CMD_BACKUP, ERROR_OPTION_INVALID_VALUE, BOGUS, OPTION_PROCESS_MAX);
    }

    if ($self->begin('invalid float ' . OPTION_PROCESS_MAX))
    {
        $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
        $self->optionSetTest($oOption, OPTION_DB_PATH, '/db');
        $self->optionSetTest($oOption, OPTION_PROCESS_MAX, '0.0');

        $self->configLoadExpect($oOption, CMD_BACKUP, ERROR_OPTION_INVALID_VALUE, '0.0', OPTION_PROCESS_MAX);
    }

    if ($self->begin('valid ' . OPTION_PROCESS_MAX))
    {
        $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
        $self->optionSetTest($oOption, OPTION_DB_PATH, '/db');
        $self->optionSetTest($oOption, OPTION_PROCESS_MAX, '2');

        $self->configLoadExpect($oOption, CMD_BACKUP);
    }

    if ($self->begin('valid thread-max'))
    {
        $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
        $self->optionSetTest($oOption, OPTION_DB_PATH, '/db');
        $self->optionSetTest($oOption, 'thread-max', '2');

        $self->configLoadExpect($oOption, CMD_BACKUP);
        $self->optionTestExpect(OPTION_PROCESS_MAX, 2);
    }

    if ($self->begin('valid float ' . OPTION_TEST_DELAY))
    {
        $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
        $self->optionSetTest($oOption, OPTION_DB_PATH, '/db');
        $self->optionBoolSetTest($oOption, OPTION_TEST);
        $self->optionSetTest($oOption, OPTION_TEST_DELAY, '0.25');

        $self->configLoadExpect($oOption, CMD_BACKUP);
    }

    if ($self->begin('valid int ' . OPTION_TEST_DELAY))
    {
        $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
        $self->optionSetTest($oOption, OPTION_DB_PATH, '/db');
        $self->optionBoolSetTest($oOption, OPTION_TEST);
        $self->optionSetTest($oOption, OPTION_TEST_DELAY, 3);

        $self->configLoadExpect($oOption, CMD_BACKUP);
    }

    if ($self->begin('restore valid ' . OPTION_TARGET_TIMELINE))
    {
        $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
        $self->optionSetTest($oOption, OPTION_DB_PATH, '/db');
        $self->optionSetTest($oOption, OPTION_TARGET_TIMELINE, 2);

        $self->configLoadExpect($oOption, CMD_RESTORE);
    }

    if ($self->begin('invalid ' . OPTION_BUFFER_SIZE))
    {
        $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
        $self->optionSetTest($oOption, OPTION_BUFFER_SIZE, '512');

        $self->configLoadExpect($oOption, CMD_RESTORE, ERROR_OPTION_INVALID_RANGE, '512', OPTION_BUFFER_SIZE);
    }

    if ($self->begin(CMD_BACKUP . ' invalid value ' . OPTION_RETENTION_ARCHIVE_TYPE))
    {
        $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
        $self->optionSetTest($oOption, OPTION_DB_PATH, '/db');
        $self->optionSetTest($oOption, OPTION_RETENTION_ARCHIVE_TYPE, BOGUS);

        $self->configLoadExpect($oOption, CMD_BACKUP, ERROR_OPTION_INVALID_VALUE, BOGUS, OPTION_RETENTION_ARCHIVE_TYPE);
    }

    if ($self->begin(
        CMD_BACKUP . ' default value ' . OPTION_DEFAULT_RETENTION_ARCHIVE_TYPE . ' for ' . OPTION_RETENTION_ARCHIVE_TYPE .
        ' with valid ' . OPTION_RETENTION_FULL))
    {
        $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
        $self->optionSetTest($oOption, OPTION_DB_PATH, '/db');
        $self->optionSetTest($oOption, OPTION_RETENTION_FULL, 1);

        $self->configLoadExpect($oOption, CMD_BACKUP);
        $self->optionTestExpect(OPTION_RETENTION_ARCHIVE, 1);
        $self->optionTestExpect(OPTION_RETENTION_FULL, 1);
        $self->optionTestExpect(OPTION_RETENTION_DIFF, undef);
        # Default is FULL so this test will fail if the default is changed, alerting to the need to update configLoad.
        $self->optionTestExpect(OPTION_RETENTION_ARCHIVE_TYPE, BACKUP_TYPE_FULL);
    }

    if ($self->begin(
        CMD_BACKUP . ' valid value ' . OPTION_RETENTION_ARCHIVE . ' for ' . OPTION_RETENTION_ARCHIVE_TYPE . ' ' .
        BACKUP_TYPE_DIFF))
    {
        $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
        $self->optionSetTest($oOption, OPTION_DB_PATH, '/db');
        $self->optionSetTest($oOption, OPTION_RETENTION_DIFF, 1);
        $self->optionSetTest($oOption, OPTION_RETENTION_ARCHIVE_TYPE, BACKUP_TYPE_DIFF);

        $self->configLoadExpect($oOption, CMD_BACKUP);
        $self->optionTestExpect(OPTION_RETENTION_ARCHIVE, 1);
        $self->optionTestExpect(OPTION_RETENTION_DIFF, 1);
        $self->optionTestExpect(OPTION_RETENTION_FULL, undef);
        $self->optionTestExpect(OPTION_RETENTION_ARCHIVE_TYPE, BACKUP_TYPE_DIFF);
    }

    if ($self->begin(
        CMD_BACKUP . ' warn no valid value ' . OPTION_RETENTION_ARCHIVE . ' for ' . OPTION_RETENTION_ARCHIVE_TYPE . ' ' .
        BACKUP_TYPE_INCR))
    {
        $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
        $self->optionSetTest($oOption, OPTION_DB_PATH, '/db');
        $self->optionSetTest($oOption, OPTION_RETENTION_FULL, 2);
        $self->optionSetTest($oOption, OPTION_RETENTION_DIFF, 1);
        $self->optionSetTest($oOption, OPTION_RETENTION_ARCHIVE_TYPE, BACKUP_TYPE_INCR);

        $self->configLoadExpect($oOption, CMD_BACKUP);
        $self->optionTestExpect(OPTION_RETENTION_ARCHIVE, undef);
        $self->optionTestExpect(OPTION_RETENTION_ARCHIVE_TYPE, BACKUP_TYPE_INCR);
    }

    if ($self->begin(CMD_BACKUP . ' invalid value ' . OPTION_PROTOCOL_TIMEOUT))
    {
        $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
        $self->optionSetTest($oOption, OPTION_DB_PATH, '/db');
        $self->optionSetTest($oOption, OPTION_DB_TIMEOUT, 5);
        $self->optionSetTest($oOption, OPTION_PROTOCOL_TIMEOUT, 4);

        $self->configLoadExpect(
            $oOption, CMD_BACKUP, ERROR_OPTION_INVALID_VALUE, 4, OPTION_PROTOCOL_TIMEOUT,
            "'protocol-timeout' option should be greater than 'db-timeout' option");
    }

    if ($self->begin(CMD_RESTORE . ' invalid value ' . OPTION_RESTORE_RECOVERY_OPTION))
    {
        $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
        $self->optionSetTest($oOption, OPTION_DB_PATH, '/db');
        $self->optionSetTest($oOption, OPTION_RESTORE_RECOVERY_OPTION, '=');

        $self->configLoadExpect($oOption, CMD_RESTORE, ERROR_OPTION_INVALID_PAIR, '=', OPTION_RESTORE_RECOVERY_OPTION);
    }

    if ($self->begin(CMD_RESTORE . ' invalid value ' . OPTION_RESTORE_RECOVERY_OPTION))
    {
        $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
        $self->optionSetTest($oOption, OPTION_DB_PATH, '/db');
        $self->optionSetTest($oOption, OPTION_RESTORE_RECOVERY_OPTION, '=' . BOGUS);

        $self->configLoadExpect($oOption, CMD_RESTORE, ERROR_OPTION_INVALID_PAIR, '=' . BOGUS, OPTION_RESTORE_RECOVERY_OPTION);
    }

    if ($self->begin(CMD_RESTORE . ' invalid value ' . OPTION_RESTORE_RECOVERY_OPTION))
    {
        $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
        $self->optionSetTest($oOption, OPTION_DB_PATH, '/db');
        $self->optionSetTest($oOption, OPTION_RESTORE_RECOVERY_OPTION, BOGUS . '=');

        $self->configLoadExpect($oOption, CMD_RESTORE, ERROR_OPTION_INVALID_PAIR, BOGUS . '=', OPTION_RESTORE_RECOVERY_OPTION);
    }

    if ($self->begin(CMD_RESTORE . ' valid value ' . OPTION_RESTORE_RECOVERY_OPTION))
    {
        $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
        $self->optionSetTest($oOption, OPTION_DB_PATH, '/db');
        $self->optionSetTest($oOption, OPTION_RESTORE_RECOVERY_OPTION, 'primary-conn-info=db.domain.net');

        $self->configLoadExpect($oOption, CMD_RESTORE);
        $self->optionTestExpect(OPTION_RESTORE_RECOVERY_OPTION, 'db.domain.net', 'primary-conn-info');
    }

    if ($self->begin(CMD_RESTORE . ' values passed to ' . CMD_ARCHIVE_GET))
    {
        $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
        $self->optionSetTest($oOption, OPTION_DB_PATH, '/db path/main');
        $self->optionSetTest($oOption, OPTION_REPO_PATH, '/repo');
        $self->optionSetTest($oOption, OPTION_BACKUP_HOST, 'db.mydomain.com');

        $self->configLoadExpect($oOption, CMD_RESTORE);

        my $strCommand = commandWrite(CMD_ARCHIVE_GET);
        my $strExpectedCommand = abs_path($0) . " --backup-host=db.mydomain.com \"--db-path=/db path/main\"" .
                                 " --repo-path=/repo --stanza=app " . CMD_ARCHIVE_GET;

        if ($strCommand ne $strExpectedCommand)
        {
            confess "expected command '${strExpectedCommand}' but got '${strCommand}'";
        }
    }

    if ($self->begin(CMD_BACKUP . ' default value ' . OPTION_DB_CMD))
    {
        $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
        $self->optionSetTest($oOption, OPTION_DB_HOST, 'db');
        $self->optionSetTest($oOption, OPTION_DB_PATH, '/db');

        $self->configLoadExpect($oOption, CMD_BACKUP);
        $self->optionTestExpect(OPTION_DB_CMD, abs_path($0));
    }

    if ($self->begin(CMD_BACKUP . ' missing option ' . OPTION_DB_PATH))
    {
        $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());

        $self->configLoadExpect($oOption, CMD_BACKUP, ERROR_OPTION_REQUIRED, OPTION_DB_PATH, 'does this stanza exist?');
    }

    if ($self->begin(CMD_BACKUP . ' automatically increase ' . OPTION_PROTOCOL_TIMEOUT))
    {
        $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
        $self->optionSetTest($oOption, OPTION_DB_PATH, '/db');
        $self->optionSetTest($oOption, OPTION_DB_TIMEOUT, OPTION_DEFAULT_PROTOCOL_TIMEOUT + 30);

        $self->configLoadExpect($oOption, CMD_BACKUP);
        $self->optionTestExpect(OPTION_PROTOCOL_TIMEOUT, OPTION_DEFAULT_PROTOCOL_TIMEOUT + 60);
    }
}

####################################################################################################################################
# Getters
####################################################################################################################################
# Change this from the default so the same stanza is not used in all tests.
sub stanza {return 'app'};

1;
