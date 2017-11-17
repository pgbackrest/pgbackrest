####################################################################################################################################
# ConfigConfigTest.pm - Tests for mixed command line and config file options in Config.pm
####################################################################################################################################
package pgBackRestTest::Module::Config::ConfigConfigPerlTest;
use parent 'pgBackRestTest::Env::ConfigEnvTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use pgBackRest::Common::Exception;
use pgBackRest::Common::Ini;
use pgBackRest::Common::Log;
use pgBackRest::Config::Config;

use pgBackRestTest::Common::RunTest;

####################################################################################################################################
# run
####################################################################################################################################
sub run
{
    my $self = shift;

    my $oConfig = {};
    my $strConfigFile = $self->testPath() . '/pgbackrest.conf';

    if ($self->begin('set and negate option ' . cfgOptionName(CFGOPT_CONFIG)))
    {
        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSet(CFGOPT_CONFIG, '/dude/dude.conf');
        $self->optionTestSetBool(CFGOPT_CONFIG, false);

        $self->configTestLoadExpect(cfgCommandName(CFGCMD_BACKUP), ERROR_OPTION_NEGATE, cfgOptionName(CFGOPT_CONFIG));
    }

    if ($self->begin('option ' . cfgOptionName(CFGOPT_CONFIG)))
    {
        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSetBool(CFGOPT_CONFIG, false);

        $self->configTestLoadExpect(cfgCommandName(CFGCMD_BACKUP));
        $self->optionTestExpect(CFGOPT_CONFIG);
    }

    if ($self->begin('default option ' . cfgOptionName(CFGOPT_CONFIG)))
    {
        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');

        $self->configTestLoadExpect(cfgCommandName(CFGCMD_BACKUP));
        $self->optionTestExpect(CFGOPT_CONFIG, cfgDefOptionDefault(CFGCMD_BACKUP, CFGOPT_CONFIG));
    }

    if ($self->begin('config file is a path'))
    {
        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSet(CFGOPT_CONFIG, $self->testPath());

        $self->configTestLoadExpect(cfgCommandName(CFGCMD_BACKUP), ERROR_FILE_INVALID, $self->testPath());
    }

    if ($self->begin('load from config stanza command section - option ' . cfgOptionName(CFGOPT_PROCESS_MAX)))
    {
        $oConfig = {};
        $$oConfig{$self->stanza() . ':' . cfgCommandName(CFGCMD_BACKUP)}{cfgOptionName(CFGOPT_PROCESS_MAX)} = 2;
        storageTest()->put($strConfigFile, iniRender($oConfig, true));

        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSet(CFGOPT_CONFIG, $strConfigFile);

        $self->configTestLoadExpect(cfgCommandName(CFGCMD_BACKUP));
        $self->optionTestExpect(CFGOPT_PROCESS_MAX, 2);
    }

    if ($self->begin('load from config stanza section - option ' . cfgOptionName(CFGOPT_PROCESS_MAX)))
    {
        $oConfig = {};
        $$oConfig{$self->stanza()}{cfgOptionName(CFGOPT_PROCESS_MAX)} = 3;
        storageTest()->put($strConfigFile, iniRender($oConfig, true));

        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSet(CFGOPT_CONFIG, $strConfigFile);

        $self->configTestLoadExpect(cfgCommandName(CFGCMD_BACKUP));
        $self->optionTestExpect(CFGOPT_PROCESS_MAX, 3);
    }

    if ($self->begin('load from config global command section - option thread-max'))
    {
        $oConfig = {};
        $$oConfig{&CFGDEF_SECTION_GLOBAL . ':' . cfgCommandName(CFGCMD_BACKUP)}{'thread-max'} = 2;
        storageTest()->put($strConfigFile, iniRender($oConfig, true));

        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSet(CFGOPT_CONFIG, $strConfigFile);

        $self->configTestLoadExpect(cfgCommandName(CFGCMD_BACKUP));
        $self->optionTestExpect(CFGOPT_PROCESS_MAX, 2);
    }

    if ($self->begin('load from config global section - option ' . cfgOptionName(CFGOPT_PROCESS_MAX)))
    {
        $oConfig = {};
        $$oConfig{&CFGDEF_SECTION_GLOBAL}{cfgOptionName(CFGOPT_PROCESS_MAX)} = 5;
        storageTest()->put($strConfigFile, iniRender($oConfig, true));

        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSet(CFGOPT_CONFIG, $strConfigFile);

        $self->configTestLoadExpect(cfgCommandName(CFGCMD_BACKUP));
        $self->optionTestExpect(CFGOPT_PROCESS_MAX, 5);
    }

    if ($self->begin('default - option ' . cfgOptionName(CFGOPT_PROCESS_MAX)))
    {
        $oConfig = {};
        storageTest()->put($strConfigFile, iniRender($oConfig, true));

        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSet(CFGOPT_CONFIG, $strConfigFile);

        $self->configTestLoadExpect(cfgCommandName(CFGCMD_BACKUP));
        $self->optionTestExpect(CFGOPT_PROCESS_MAX, 1);
    }

    if ($self->begin('command-line override - option ' . cfgOptionName(CFGOPT_PROCESS_MAX)))
    {
        $oConfig = {};
        $$oConfig{&CFGDEF_SECTION_GLOBAL}{cfgOptionName(CFGOPT_PROCESS_MAX)} = 9;
        storageTest()->put($strConfigFile, iniRender($oConfig, true));

        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSet(CFGOPT_PROCESS_MAX, 7);
        $self->optionTestSet(CFGOPT_CONFIG, $strConfigFile);

        $self->configTestLoadExpect(cfgCommandName(CFGCMD_BACKUP));
        $self->optionTestExpect(CFGOPT_PROCESS_MAX, 7);
    }

    if ($self->begin('invalid boolean - option ' . cfgOptionName(CFGOPT_HARDLINK)))
    {
        $oConfig = {};
        $$oConfig{&CFGDEF_SECTION_GLOBAL . ':' . cfgCommandName(CFGCMD_BACKUP)}{cfgOptionName(CFGOPT_HARDLINK)} = 'Y';
        storageTest()->put($strConfigFile, iniRender($oConfig, true));

        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSet(CFGOPT_CONFIG, $strConfigFile);

        $self->configTestLoadExpect(
            cfgCommandName(CFGCMD_BACKUP), ERROR_OPTION_INVALID_VALUE, 'Y', cfgOptionName(CFGOPT_HARDLINK));
    }

    if ($self->begin('invalid value - option ' . cfgOptionName(CFGOPT_LOG_LEVEL_CONSOLE)))
    {
        $oConfig = {};
        $$oConfig{&CFGDEF_SECTION_GLOBAL}{cfgOptionName(CFGOPT_LOG_LEVEL_CONSOLE)} = BOGUS;
        storageTest()->put($strConfigFile, iniRender($oConfig, true));

        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSet(CFGOPT_CONFIG, $strConfigFile);

        $self->configTestLoadExpect(
            cfgCommandName(CFGCMD_BACKUP), ERROR_OPTION_INVALID_VALUE, BOGUS, cfgOptionName(CFGOPT_LOG_LEVEL_CONSOLE));
    }

    if ($self->begin('valid value - option ' . cfgOptionName(CFGOPT_LOG_LEVEL_CONSOLE)))
    {
        $oConfig = {};
        $$oConfig{&CFGDEF_SECTION_GLOBAL}{cfgOptionName(CFGOPT_LOG_LEVEL_CONSOLE)} = lc(INFO);
        storageTest()->put($strConfigFile, iniRender($oConfig, true));

        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSet(CFGOPT_CONFIG, $strConfigFile);

        $self->configTestLoadExpect(cfgCommandName(CFGCMD_RESTORE));
    }

    if ($self->begin('archive-push - option ' . cfgOptionName(CFGOPT_LOG_LEVEL_CONSOLE)))
    {
        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_CONFIG, $strConfigFile);

        $self->configTestLoadExpect(cfgCommandName(CFGCMD_ARCHIVE_PUSH));
    }

    if ($self->begin(cfgCommandName(CFGCMD_EXPIRE) . ' ' . cfgOptionName(CFGOPT_RETENTION_FULL)))
    {
        $oConfig = {};
        $$oConfig{$self->stanza() . ':' . cfgCommandName(CFGCMD_EXPIRE)}{cfgOptionName(CFGOPT_RETENTION_FULL)} = 2;
        storageTest()->put($strConfigFile, iniRender($oConfig, true));

        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_CONFIG, $strConfigFile);

        $self->configTestLoadExpect(cfgCommandName(CFGCMD_EXPIRE));
        $self->optionTestExpect(CFGOPT_RETENTION_FULL, 2);
    }

    if ($self->begin(cfgCommandName(CFGCMD_BACKUP) . ' option ' . cfgOptionName(CFGOPT_COMPRESS)))
    {
        $oConfig = {};
        $$oConfig{&CFGDEF_SECTION_GLOBAL . ':' . cfgCommandName(CFGCMD_BACKUP)}{cfgOptionName(CFGOPT_COMPRESS)} = 'n';
        storageTest()->put($strConfigFile, iniRender($oConfig, true));

        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSet(CFGOPT_CONFIG, $strConfigFile);

        $self->configTestLoadExpect(cfgCommandName(CFGCMD_BACKUP));
        $self->optionTestExpect(CFGOPT_COMPRESS, false);
    }

    if ($self->begin(cfgCommandName(CFGCMD_RESTORE) . ' global option ' . cfgOptionName(CFGOPT_RECOVERY_OPTION) . ' error'))
    {
        $oConfig = {};
        $$oConfig{&CFGDEF_SECTION_GLOBAL . ':' . cfgCommandName(CFGCMD_RESTORE)}{cfgOptionName(CFGOPT_RECOVERY_OPTION)} = 'bogus=';
        storageTest()->put($strConfigFile, iniRender($oConfig, true));

        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSet(CFGOPT_CONFIG, $strConfigFile);

        $self->configTestLoadExpect(
            cfgCommandName(CFGCMD_RESTORE), ERROR_OPTION_INVALID_VALUE, 'bogus=', cfgOptionName(CFGOPT_RECOVERY_OPTION));
    }

    if ($self->begin(cfgCommandName(CFGCMD_RESTORE) . ' global option ' . cfgOptionName(CFGOPT_RECOVERY_OPTION) . ' error'))
    {
        $oConfig = {};
        $$oConfig{&CFGDEF_SECTION_GLOBAL . ':' . cfgCommandName(CFGCMD_RESTORE)}{cfgOptionName(CFGOPT_RECOVERY_OPTION)} = '=bogus';
        storageTest()->put($strConfigFile, iniRender($oConfig, true));

        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSet(CFGOPT_CONFIG, $strConfigFile);

        $self->configTestLoadExpect(
            cfgCommandName(CFGCMD_RESTORE), ERROR_OPTION_INVALID_VALUE, '=bogus', cfgOptionName(CFGOPT_RECOVERY_OPTION));
    }

    if ($self->begin(cfgCommandName(CFGCMD_RESTORE) . ' global option ' . cfgOptionName(CFGOPT_RECOVERY_OPTION)))
    {
        $oConfig = {};
        $$oConfig{&CFGDEF_SECTION_GLOBAL . ':' . cfgCommandName(CFGCMD_RESTORE)}{cfgOptionName(CFGOPT_RECOVERY_OPTION)} =
            'archive-command=/path/to/pgbackrest';
        storageTest()->put($strConfigFile, iniRender($oConfig, true));

        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSet(CFGOPT_CONFIG, $strConfigFile);

        $self->configTestLoadExpect(cfgCommandName(CFGCMD_RESTORE));
        $self->optionTestExpect(CFGOPT_RECOVERY_OPTION, '/path/to/pgbackrest', 'archive-command');
    }

    if ($self->begin(cfgCommandName(CFGCMD_RESTORE) . ' stanza option ' . cfgOptionName(CFGOPT_RECOVERY_OPTION)))
    {
        $oConfig = {};
        $$oConfig{$self->stanza()}{cfgOptionName(CFGOPT_RECOVERY_OPTION)} = ['standby-mode=on', 'a=b'];
        storageTest()->put($strConfigFile, iniRender($oConfig, true));

        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSet(CFGOPT_CONFIG, $strConfigFile);

        $self->configTestLoadExpect(cfgCommandName(CFGCMD_RESTORE));
        $self->optionTestExpect(CFGOPT_RECOVERY_OPTION, 'b', 'a');
        $self->optionTestExpect(CFGOPT_RECOVERY_OPTION, 'on', 'standby-mode');
    }

    if ($self->begin(cfgCommandName(CFGCMD_BACKUP) . ' option ' . cfgOptionName(CFGOPT_DB_PATH)))
    {
        $oConfig = {};
        $$oConfig{$self->stanza()}{cfgOptionName(CFGOPT_DB_PATH)} = '/path/to/db';
        storageTest()->put($strConfigFile, iniRender($oConfig, true));

        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_CONFIG, $strConfigFile);

        $self->configTestLoadExpect(cfgCommandName(CFGCMD_BACKUP));
        $self->optionTestExpect(CFGOPT_DB_PATH, '/path/to/db');
    }

    if ($self->begin(cfgCommandName(CFGCMD_BACKUP) . ' option ' . cfgOptionName(CFGOPT_ARCHIVE_CHECK)))
    {
        $oConfig = {};
        $$oConfig{$self->stanza()}{cfgOptionName(CFGOPT_DB_PATH)} = '/path/to/db';
        storageTest()->put($strConfigFile, iniRender($oConfig, true));

        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_CONFIG, $strConfigFile);
        $self->optionTestSetBool(CFGOPT_ARCHIVE_CHECK, false);

        $self->configTestLoadExpect(cfgCommandName(CFGCMD_BACKUP));
        $self->optionTestExpect(CFGOPT_ONLINE, true);
        $self->optionTestExpect(CFGOPT_ARCHIVE_CHECK, false);
    }

    if ($self->begin(cfgCommandName(CFGCMD_ARCHIVE_PUSH) . ' option ' . cfgOptionName(CFGOPT_DB_PATH)))
    {
        $oConfig = {};
        $$oConfig{$self->stanza()}{cfgOptionName(CFGOPT_DB_PATH)} = '/path/to/db';
        storageTest()->put($strConfigFile, iniRender($oConfig, true));

        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_CONFIG, $strConfigFile);

        $self->configTestLoadExpect(cfgCommandName(CFGCMD_ARCHIVE_PUSH));
        $self->optionTestExpect(CFGOPT_DB_PATH, '/path/to/db');
    }

    if ($self->begin(cfgCommandName(CFGCMD_BACKUP) . ' option ' . cfgOptionName(CFGOPT_REPO_PATH)))
    {
        $oConfig = {};
        $$oConfig{&CFGDEF_SECTION_GLOBAL}{cfgOptionName(CFGOPT_REPO_PATH)} = '/repo';
        storageTest()->put($strConfigFile, iniRender($oConfig, true));

        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSet(CFGOPT_CONFIG, $strConfigFile);

        $self->configTestLoadExpect(cfgCommandName(CFGCMD_BACKUP));
        $self->optionTestExpect(CFGOPT_REPO_PATH, '/repo');
    }

    if ($self->begin(cfgCommandName(CFGCMD_BACKUP) . ' option ' . cfgOptionName(CFGOPT_REPO_PATH) . ' multiple times'))
    {
        $oConfig = {};
        $$oConfig{&CFGDEF_SECTION_GLOBAL}{cfgOptionName(CFGOPT_REPO_PATH)} = ['/repo', '/repo2'];
        storageTest()->put($strConfigFile, iniRender($oConfig, true));

        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSet(CFGOPT_CONFIG, $strConfigFile);

        $self->configTestLoadExpect(cfgCommandName(CFGCMD_BACKUP), ERROR_OPTION_MULTIPLE_VALUE, cfgOptionName(CFGOPT_REPO_PATH));
    }
}

####################################################################################################################################
# Getters
####################################################################################################################################
# Change this from the default so the same stanza is not used in all tests.
sub stanza {return 'main'};

1;
