####################################################################################################################################
# ConfigOptionTest.pm - Tests for command line options in Config.pm
####################################################################################################################################
package pgBackRestTest::Module::Config::ConfigOptionTest;
use parent 'pgBackRestTest::Env::ConfigEnvTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

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

    my @oyArray;

    if ($self->begin('backup with no stanza'))
    {
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');

        $self->configTestLoadExpect(cfgCommandName(CFGCMD_BACKUP), ERROR_OPTION_REQUIRED, cfgOptionName(CFGOPT_STANZA));
    }

    if ($self->begin('backup with boolean stanza'))
    {
        $self->optionTestSetBool(CFGOPT_STANZA);

        $self->configTestLoadExpect(cfgCommandName(CFGCMD_BACKUP), ERROR_COMMAND_REQUIRED);
    }

    if ($self->begin('backup type defaults to ' . CFGOPTVAL_BACKUP_TYPE_INCR))
    {
        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');

        $self->configTestLoadExpect(cfgCommandName(CFGCMD_BACKUP));
        $self->optionTestExpect(CFGOPT_TYPE, CFGOPTVAL_BACKUP_TYPE_INCR);
    }

    if ($self->begin(cfgCommandName(CFGCMD_BACKUP) . ' invalid option ' . cfgOptionName(CFGOPT_ARCHIVE_ASYNC)))
    {
        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSetBool(CFGOPT_ARCHIVE_ASYNC);

        $self->configTestLoadExpect(
            cfgCommandName(CFGCMD_BACKUP), ERROR_OPTION_COMMAND, cfgOptionName(CFGOPT_ARCHIVE_ASYNC),
            cfgCommandName(CFGCMD_BACKUP));
    }

    if ($self->begin('backup type set to ' . CFGOPTVAL_BACKUP_TYPE_FULL))
    {
        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSet(CFGOPT_TYPE, CFGOPTVAL_BACKUP_TYPE_FULL);

        $self->configTestLoadExpect(cfgCommandName(CFGCMD_BACKUP));
        $self->optionTestExpect(CFGOPT_TYPE, CFGOPTVAL_BACKUP_TYPE_FULL);
    }

    if ($self->begin('backup type invalid'))
    {
        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSet(CFGOPT_TYPE, BOGUS);

        $self->configTestLoadExpect(cfgCommandName(CFGCMD_BACKUP), ERROR_OPTION_INVALID_VALUE, BOGUS, cfgOptionName(CFGOPT_TYPE));
    }

    if ($self->begin('backup invalid force'))
    {
        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSetBool(CFGOPT_FORCE);

        $self->configTestLoadExpect(
            cfgCommandName(CFGCMD_BACKUP), ERROR_OPTION_INVALID, cfgOptionName(CFGOPT_FORCE), 'no-' . cfgOptionName(CFGOPT_ONLINE));
    }

    if ($self->begin('backup valid force'))
    {
        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSetBool(CFGOPT_ONLINE, false);
        $self->optionTestSetBool(CFGOPT_FORCE);

        $self->configTestLoadExpect(cfgCommandName(CFGCMD_BACKUP));
        $self->optionTestExpect(CFGOPT_ONLINE, false);
        $self->optionTestExpect(CFGOPT_FORCE, true);
    }

    if ($self->begin('backup invalid value for ' . cfgOptionName(CFGOPT_TEST_DELAY)))
    {
        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSetBool(CFGOPT_TEST);
        $self->optionTestSet(CFGOPT_TEST_DELAY, BOGUS);

        $self->configTestLoadExpect(
            cfgCommandName(CFGCMD_BACKUP), ERROR_OPTION_INVALID_VALUE, BOGUS, cfgOptionName(CFGOPT_TEST_DELAY));
    }

    if ($self->begin('backup invalid ' . cfgOptionName(CFGOPT_TEST_DELAY)))
    {
        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSet(CFGOPT_TEST_DELAY, 5);

        $self->configTestLoadExpect(
            cfgCommandName(CFGCMD_BACKUP), ERROR_OPTION_INVALID, cfgOptionName(CFGOPT_TEST_DELAY), cfgOptionName(CFGOPT_TEST));
    }

    if ($self->begin('backup check ' . cfgOptionName(CFGOPT_TEST_DELAY) . ' undef'))
    {
        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');

        $self->configTestLoadExpect(cfgCommandName(CFGCMD_BACKUP));
        $self->optionTestExpect(CFGOPT_TEST_DELAY);
    }

    if ($self->begin('restore invalid ' . cfgOptionName(CFGOPT_TARGET)))
    {
        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSet(CFGOPT_TYPE, cfgDefOptionDefault(CFGCMD_RESTORE, CFGOPT_TYPE));
        $self->optionTestSet(CFGOPT_TARGET, BOGUS);

        @oyArray = (CFGOPTVAL_RESTORE_TYPE_NAME, CFGOPTVAL_RESTORE_TYPE_TIME, CFGOPTVAL_RESTORE_TYPE_XID);
        $self->configTestLoadExpect(
            cfgCommandName(CFGCMD_RESTORE), ERROR_OPTION_INVALID, cfgOptionName(CFGOPT_TARGET),
            cfgOptionName(CFGOPT_TYPE), \@oyArray);
    }

    if ($self->begin('restore ' . cfgOptionName(CFGOPT_TARGET)))
    {
        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSet(CFGOPT_TYPE, CFGOPTVAL_RESTORE_TYPE_NAME);
        $self->optionTestSet(CFGOPT_TARGET, BOGUS);

        $self->configTestLoadExpect(cfgCommandName(CFGCMD_RESTORE));
        $self->optionTestExpect(CFGOPT_TYPE, CFGOPTVAL_RESTORE_TYPE_NAME);
        $self->optionTestExpect(CFGOPT_TARGET, BOGUS);
        $self->optionTestExpect(CFGOPT_TARGET_TIMELINE);
    }

    if ($self->begin('invalid string ' . cfgOptionName(CFGOPT_PROCESS_MAX)))
    {
        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSet(CFGOPT_PROCESS_MAX, BOGUS);

        $self->configTestLoadExpect(
            cfgCommandName(CFGCMD_BACKUP), ERROR_OPTION_INVALID_VALUE, BOGUS, cfgOptionName(CFGOPT_PROCESS_MAX));
    }

    if ($self->begin('invalid float ' . cfgOptionName(CFGOPT_PROCESS_MAX)))
    {
        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSet(CFGOPT_PROCESS_MAX, '0.0');

        $self->configTestLoadExpect(
            cfgCommandName(CFGCMD_BACKUP), ERROR_OPTION_INVALID_VALUE, '0.0', cfgOptionName(CFGOPT_PROCESS_MAX));
    }

    if ($self->begin('valid ' . cfgOptionName(CFGOPT_PROCESS_MAX)))
    {
        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSet(CFGOPT_PROCESS_MAX, '2');

        $self->configTestLoadExpect(cfgCommandName(CFGCMD_BACKUP));
    }

    if ($self->begin('valid thread-max'))
    {
        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSetByName('thread-max', '2');

        $self->configTestLoadExpect(cfgCommandName(CFGCMD_BACKUP));
        $self->optionTestExpect(CFGOPT_PROCESS_MAX, 2);
    }

    if ($self->begin('valid float ' . cfgOptionName(CFGOPT_TEST_DELAY)))
    {
        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSetBool(CFGOPT_TEST);
        $self->optionTestSet(CFGOPT_TEST_DELAY, '0.25');

        $self->configTestLoadExpect(cfgCommandName(CFGCMD_BACKUP));
    }

    if ($self->begin('valid int ' . cfgOptionName(CFGOPT_TEST_DELAY)))
    {
        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSetBool(CFGOPT_TEST);
        $self->optionTestSet(CFGOPT_TEST_DELAY, 3);

        $self->configTestLoadExpect(cfgCommandName(CFGCMD_BACKUP));
    }

    if ($self->begin('restore valid ' . cfgOptionName(CFGOPT_TARGET_TIMELINE)))
    {
        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSet(CFGOPT_TARGET_TIMELINE, 2);

        $self->configTestLoadExpect(cfgCommandName(CFGCMD_RESTORE));
    }

    if ($self->begin('invalid ' . cfgOptionName(CFGOPT_COMPRESS_LEVEL)))
    {
        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_COMPRESS_LEVEL, '12');

        $self->configTestLoadExpect(
            cfgCommandName(CFGCMD_RESTORE), ERROR_OPTION_INVALID_RANGE, '12', cfgOptionName(CFGOPT_COMPRESS_LEVEL));
    }

    if ($self->begin(cfgCommandName(CFGCMD_BACKUP) . ' invalid value ' . cfgOptionName(CFGOPT_RETENTION_ARCHIVE_TYPE)))
    {
        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSet(CFGOPT_RETENTION_ARCHIVE_TYPE, BOGUS);

        $self->configTestLoadExpect(
            cfgCommandName(CFGCMD_BACKUP), ERROR_OPTION_INVALID_VALUE, BOGUS, cfgOptionName(CFGOPT_RETENTION_ARCHIVE_TYPE));
    }

    if ($self->begin(
        cfgCommandName(CFGCMD_BACKUP) . ' default value ' . cfgDefOptionDefault(CFGCMD_BACKUP, CFGOPT_RETENTION_ARCHIVE_TYPE) .
        ' for ' . cfgOptionName(CFGOPT_RETENTION_ARCHIVE_TYPE) . ' with valid ' . cfgOptionName(CFGOPT_RETENTION_FULL)))
    {
        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSet(CFGOPT_RETENTION_FULL, 1);

        $self->configTestLoadExpect(cfgCommandName(CFGCMD_BACKUP));
        $self->optionTestExpect(CFGOPT_RETENTION_ARCHIVE, 1);
        $self->optionTestExpect(CFGOPT_RETENTION_FULL, 1);
        $self->optionTestExpect(CFGOPT_RETENTION_DIFF, undef);
        # Default is FULL so this test will fail if the default is changed, alerting to the need to update configLoad.
        $self->optionTestExpect(CFGOPT_RETENTION_ARCHIVE_TYPE, CFGOPTVAL_BACKUP_TYPE_FULL);
    }

    if ($self->begin(
        cfgCommandName(CFGCMD_BACKUP) . ' valid value ' . cfgOptionName(CFGOPT_RETENTION_ARCHIVE) . ' for ' .
        cfgOptionName(CFGOPT_RETENTION_ARCHIVE_TYPE) . ' ' . CFGOPTVAL_BACKUP_TYPE_DIFF))
    {
        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSet(CFGOPT_RETENTION_DIFF, 1);
        $self->optionTestSet(CFGOPT_RETENTION_ARCHIVE_TYPE, CFGOPTVAL_BACKUP_TYPE_DIFF);

        $self->configTestLoadExpect(cfgCommandName(CFGCMD_BACKUP));
        $self->optionTestExpect(CFGOPT_RETENTION_ARCHIVE, 1);
        $self->optionTestExpect(CFGOPT_RETENTION_DIFF, 1);
        $self->optionTestExpect(CFGOPT_RETENTION_FULL, undef);
        $self->optionTestExpect(CFGOPT_RETENTION_ARCHIVE_TYPE, CFGOPTVAL_BACKUP_TYPE_DIFF);
    }

    if ($self->begin(
        cfgCommandName(CFGCMD_BACKUP) . ' warn no valid value ' . cfgOptionName(CFGOPT_RETENTION_ARCHIVE) . ' for ' .
        cfgOptionName(CFGOPT_RETENTION_ARCHIVE_TYPE) . ' ' . CFGOPTVAL_BACKUP_TYPE_INCR))
    {
        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSet(CFGOPT_RETENTION_FULL, 2);
        $self->optionTestSet(CFGOPT_RETENTION_DIFF, 1);
        $self->optionTestSet(CFGOPT_RETENTION_ARCHIVE_TYPE, CFGOPTVAL_BACKUP_TYPE_INCR);

        $self->configTestLoadExpect(cfgCommandName(CFGCMD_BACKUP));
        $self->optionTestExpect(CFGOPT_RETENTION_ARCHIVE, undef);
        $self->optionTestExpect(CFGOPT_RETENTION_ARCHIVE_TYPE, CFGOPTVAL_BACKUP_TYPE_INCR);
    }

    if ($self->begin(cfgCommandName(CFGCMD_BACKUP) . ' invalid value ' . cfgOptionName(CFGOPT_PROTOCOL_TIMEOUT)))
    {
        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSet(CFGOPT_DB_TIMEOUT, 5);
        $self->optionTestSet(CFGOPT_PROTOCOL_TIMEOUT, 4);

        $self->configTestLoadExpect(
            cfgCommandName(CFGCMD_BACKUP), ERROR_OPTION_INVALID_VALUE, 4, cfgOptionName(CFGOPT_PROTOCOL_TIMEOUT),
            "'protocol-timeout' option (4) should be greater than 'db-timeout' option (5)");
    }

    if ($self->begin(cfgCommandName(CFGCMD_RESTORE) . ' invalid value ' . cfgOptionName(CFGOPT_RECOVERY_OPTION)))
    {
        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSet(CFGOPT_RECOVERY_OPTION, '=');

        $self->configTestLoadExpect(
            cfgCommandName(CFGCMD_RESTORE), ERROR_OPTION_INVALID_PAIR, '=', cfgOptionName(CFGOPT_RECOVERY_OPTION));
    }

    if ($self->begin(cfgCommandName(CFGCMD_RESTORE) . ' invalid value ' . cfgOptionName(CFGOPT_RECOVERY_OPTION)))
    {
        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSet(CFGOPT_RECOVERY_OPTION, '=' . BOGUS);

        $self->configTestLoadExpect(
            cfgCommandName(CFGCMD_RESTORE), ERROR_OPTION_INVALID_PAIR, '=' . BOGUS, cfgOptionName(CFGOPT_RECOVERY_OPTION));
    }

    if ($self->begin(cfgCommandName(CFGCMD_RESTORE) . ' invalid value ' . cfgOptionName(CFGOPT_RECOVERY_OPTION)))
    {
        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSet(CFGOPT_RECOVERY_OPTION, BOGUS . '=');

        $self->configTestLoadExpect(
            cfgCommandName(CFGCMD_RESTORE), ERROR_OPTION_INVALID_PAIR, BOGUS . '=', cfgOptionName(CFGOPT_RECOVERY_OPTION));
    }

    if ($self->begin(cfgCommandName(CFGCMD_RESTORE) . ' valid value ' . cfgOptionName(CFGOPT_RECOVERY_OPTION)))
    {
        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSet(CFGOPT_RECOVERY_OPTION, 'primary-conn-info=db.domain.net');

        $self->configTestLoadExpect(cfgCommandName(CFGCMD_RESTORE));
        $self->optionTestExpect(&CFGOPT_RECOVERY_OPTION, 'db.domain.net', 'primary-conn-info');
    }

    if ($self->begin(cfgCommandName(CFGCMD_RESTORE) . ' values passed to ' . cfgCommandName(CFGCMD_ARCHIVE_GET)))
    {
        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db path/main');
        $self->optionTestSet(CFGOPT_REPO_PATH, '/repo');
        $self->optionTestSet(CFGOPT_BACKUP_HOST, 'db.mydomain.com');

        $self->configTestLoadExpect(cfgCommandName(CFGCMD_RESTORE));

        my $strCommand = cfgCommandWrite(CFGCMD_ARCHIVE_GET);
        my $strExpectedCommand = abs_path($0) . " --backup-host=db.mydomain.com \"--db1-path=/db path/main\"" .
                                 " --repo-path=/repo --stanza=app " . cfgCommandName(CFGCMD_ARCHIVE_GET);

        if ($strCommand ne $strExpectedCommand)
        {
            confess "expected command '${strExpectedCommand}' but got '${strCommand}'";
        }
    }

    if ($self->begin(cfgCommandName(CFGCMD_BACKUP) . ' default value ' . cfgOptionName(CFGOPT_DB_CMD)))
    {
        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_HOST, 'db');
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');

        $self->configTestLoadExpect(cfgCommandName(CFGCMD_BACKUP));
        $self->optionTestExpect(CFGOPT_DB_CMD, abs_path($0));
    }

    if ($self->begin(cfgCommandName(CFGCMD_BACKUP) . ' missing option ' . cfgOptionName(CFGOPT_DB_PATH)))
    {
        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());

        $self->configTestLoadExpect(cfgCommandName(CFGCMD_BACKUP), ERROR_OPTION_REQUIRED, 'db1-path', 'does this stanza exist?');
    }

    if ($self->begin(cfgCommandName(CFGCMD_BACKUP) . ' automatically increase ' . cfgOptionName(CFGOPT_PROTOCOL_TIMEOUT)))
    {
        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSet(CFGOPT_DB_TIMEOUT, cfgDefOptionDefault(CFGCMD_BACKUP, CFGOPT_PROTOCOL_TIMEOUT) + 30);

        $self->configTestLoadExpect(cfgCommandName(CFGCMD_BACKUP));
        $self->optionTestExpect(CFGOPT_PROTOCOL_TIMEOUT, cfgDefOptionDefault(CFGCMD_BACKUP, CFGOPT_PROTOCOL_TIMEOUT) + 60);
    }
}

####################################################################################################################################
# Getters
####################################################################################################################################
# Change this from the default so the same stanza is not used in all tests.
sub stanza {return 'app'};

1;
