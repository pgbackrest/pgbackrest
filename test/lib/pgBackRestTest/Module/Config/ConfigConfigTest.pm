####################################################################################################################################
# ConfigConfigTest.pm - Tests for mixed command line and config file options in Config.pm
####################################################################################################################################
package pgBackRestTest::Module::Config::ConfigConfigTest;
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
use pgBackRest::LibC qw(:config :configRule);

use pgBackRestBuild::Config::Data;

use pgBackRestTest::Common::RunTest;

####################################################################################################################################
# run
####################################################################################################################################
sub run
{
    my $self = shift;

    my $oConfig = {};
    my $strConfigFile = $self->testPath() . '/pgbackrest.conf';

    if ($self->begin('set and negate option ' . CFGBLDOPT_CONFIG))
    {
        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSet(CFGOPT_CONFIG, '/dude/dude.conf');
        $self->optionTestSetBool(CFGOPT_CONFIG, false);

        $self->configTestLoadExpect(CFGBLDCMD_BACKUP, ERROR_OPTION_NEGATE, CFGBLDOPT_CONFIG);
    }

    if ($self->begin('option ' . CFGBLDOPT_CONFIG))
    {
        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSetBool(CFGOPT_CONFIG, false);

        $self->configTestLoadExpect(CFGBLDCMD_BACKUP);
        $self->optionTestExpect(CFGOPT_CONFIG);
    }

    if ($self->begin('default option ' . CFGBLDOPT_CONFIG))
    {
        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');

        $self->configTestLoadExpect(CFGBLDCMD_BACKUP);
        $self->optionTestExpect(CFGOPT_CONFIG, cfgOptionRuleDefault(CFGCMD_BACKUP, CFGOPT_CONFIG));
    }

    if ($self->begin('config file is a path'))
    {
        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSet(CFGOPT_CONFIG, $self->testPath());

        $self->configTestLoadExpect(CFGBLDCMD_BACKUP, ERROR_FILE_INVALID, $self->testPath());
    }

    if ($self->begin('load from config stanza command section - option ' . CFGBLDOPT_PROCESS_MAX))
    {
        $oConfig = {};
        $$oConfig{$self->stanza() . ':' . cfgCommandName(CFGCMD_BACKUP)}{&CFGBLDOPT_PROCESS_MAX} = 2;
        storageTest()->put($strConfigFile, iniRender($oConfig, true));

        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSet(CFGOPT_CONFIG, $strConfigFile);

        $self->configTestLoadExpect(CFGBLDCMD_BACKUP);
        $self->optionTestExpect(CFGOPT_PROCESS_MAX, 2);
    }

    if ($self->begin('load from config stanza section - option ' . CFGBLDOPT_PROCESS_MAX))
    {
        $oConfig = {};
        $$oConfig{$self->stanza()}{&CFGBLDOPT_PROCESS_MAX} = 3;
        storageTest()->put($strConfigFile, iniRender($oConfig, true));

        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSet(CFGOPT_CONFIG, $strConfigFile);

        $self->configTestLoadExpect(CFGBLDCMD_BACKUP);
        $self->optionTestExpect(CFGOPT_PROCESS_MAX, 3);
    }

    if ($self->begin('load from config global command section - option thread-max'))
    {
        $oConfig = {};
        $$oConfig{&CONFIG_SECTION_GLOBAL . ':' . cfgCommandName(CFGCMD_BACKUP)}{'thread-max'} = 2;
        storageTest()->put($strConfigFile, iniRender($oConfig, true));

        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSet(CFGOPT_CONFIG, $strConfigFile);

        $self->configTestLoadExpect(CFGBLDCMD_BACKUP);
        $self->optionTestExpect(CFGOPT_PROCESS_MAX, 2);
    }

    if ($self->begin('load from config global section - option ' . CFGBLDOPT_PROCESS_MAX))
    {
        $oConfig = {};
        $$oConfig{&CONFIG_SECTION_GLOBAL}{&CFGBLDOPT_PROCESS_MAX} = 5;
        storageTest()->put($strConfigFile, iniRender($oConfig, true));

        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSet(CFGOPT_CONFIG, $strConfigFile);

        $self->configTestLoadExpect(CFGBLDCMD_BACKUP);
        $self->optionTestExpect(CFGOPT_PROCESS_MAX, 5);
    }

    if ($self->begin('default - option ' . CFGBLDOPT_PROCESS_MAX))
    {
        $oConfig = {};
        storageTest()->put($strConfigFile, iniRender($oConfig, true));

        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSet(CFGOPT_CONFIG, $strConfigFile);

        $self->configTestLoadExpect(CFGBLDCMD_BACKUP);
        $self->optionTestExpect(CFGOPT_PROCESS_MAX, 1);
    }

    if ($self->begin('command-line override - option ' . CFGBLDOPT_PROCESS_MAX))
    {
        $oConfig = {};
        $$oConfig{&CONFIG_SECTION_GLOBAL}{&CFGBLDOPT_PROCESS_MAX} = 9;
        storageTest()->put($strConfigFile, iniRender($oConfig, true));

        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSet(CFGOPT_PROCESS_MAX, 7);
        $self->optionTestSet(CFGOPT_CONFIG, $strConfigFile);

        $self->configTestLoadExpect(CFGBLDCMD_BACKUP);
        $self->optionTestExpect(CFGOPT_PROCESS_MAX, 7);
    }

    if ($self->begin('invalid boolean - option ' . CFGBLDOPT_HARDLINK))
    {
        $oConfig = {};
        $$oConfig{&CONFIG_SECTION_GLOBAL . ':' . cfgCommandName(CFGCMD_BACKUP)}{&CFGBLDOPT_HARDLINK} = 'Y';
        storageTest()->put($strConfigFile, iniRender($oConfig, true));

        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSet(CFGOPT_CONFIG, $strConfigFile);

        $self->configTestLoadExpect(CFGBLDCMD_BACKUP, ERROR_OPTION_INVALID_VALUE, 'Y', CFGBLDOPT_HARDLINK);
    }

    if ($self->begin('invalid value - option ' . CFGBLDOPT_LOG_LEVEL_CONSOLE))
    {
        $oConfig = {};
        $$oConfig{&CONFIG_SECTION_GLOBAL}{&CFGBLDOPT_LOG_LEVEL_CONSOLE} = BOGUS;
        storageTest()->put($strConfigFile, iniRender($oConfig, true));

        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSet(CFGOPT_CONFIG, $strConfigFile);

        $self->configTestLoadExpect(CFGBLDCMD_BACKUP, ERROR_OPTION_INVALID_VALUE, BOGUS, CFGBLDOPT_LOG_LEVEL_CONSOLE);
    }

    if ($self->begin('valid value - option ' . CFGBLDOPT_LOG_LEVEL_CONSOLE))
    {
        $oConfig = {};
        $$oConfig{&CONFIG_SECTION_GLOBAL}{&CFGBLDOPT_LOG_LEVEL_CONSOLE} = lc(INFO);
        storageTest()->put($strConfigFile, iniRender($oConfig, true));

        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSet(CFGOPT_CONFIG, $strConfigFile);

        $self->configTestLoadExpect(CFGBLDCMD_RESTORE);
    }

    if ($self->begin('archive-push - option ' . CFGBLDOPT_LOG_LEVEL_CONSOLE))
    {
        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_CONFIG, $strConfigFile);

        $self->configTestLoadExpect(CFGBLDCMD_ARCHIVE_PUSH);
    }

    if ($self->begin(cfgCommandName(CFGCMD_EXPIRE) . ' ' . CFGBLDOPT_RETENTION_FULL))
    {
        $oConfig = {};
        $$oConfig{$self->stanza() . ':' . cfgCommandName(CFGCMD_EXPIRE)}{&CFGBLDOPT_RETENTION_FULL} = 2;
        storageTest()->put($strConfigFile, iniRender($oConfig, true));

        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_CONFIG, $strConfigFile);

        $self->configTestLoadExpect(CFGBLDCMD_EXPIRE);
        $self->optionTestExpect(CFGOPT_RETENTION_FULL, 2);
    }

    if ($self->begin(cfgCommandName(CFGCMD_BACKUP) . ' option ' . CFGBLDOPT_COMPRESS))
    {
        $oConfig = {};
        $$oConfig{&CONFIG_SECTION_GLOBAL . ':' . cfgCommandName(CFGCMD_BACKUP)}{&CFGBLDOPT_COMPRESS} = 'n';
        storageTest()->put($strConfigFile, iniRender($oConfig, true));

        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSet(CFGOPT_CONFIG, $strConfigFile);

        $self->configTestLoadExpect(CFGBLDCMD_BACKUP);
        $self->optionTestExpect(CFGOPT_COMPRESS, false);
    }

    if ($self->begin(cfgCommandName(CFGCMD_RESTORE) . ' global option ' . CFGBLDOPT_RECOVERY_OPTION . ' error'))
    {
        $oConfig = {};
        $$oConfig{&CONFIG_SECTION_GLOBAL . ':' . cfgCommandName(CFGCMD_RESTORE)}{&CFGBLDOPT_RECOVERY_OPTION} = 'bogus=';
        storageTest()->put($strConfigFile, iniRender($oConfig, true));

        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSet(CFGOPT_CONFIG, $strConfigFile);

        $self->configTestLoadExpect(CFGBLDCMD_RESTORE, ERROR_OPTION_INVALID_VALUE, 'bogus=', CFGBLDOPT_RECOVERY_OPTION);
    }

    if ($self->begin(cfgCommandName(CFGCMD_RESTORE) . ' global option ' . CFGBLDOPT_RECOVERY_OPTION . ' error'))
    {
        $oConfig = {};
        $$oConfig{&CONFIG_SECTION_GLOBAL . ':' . cfgCommandName(CFGCMD_RESTORE)}{&CFGBLDOPT_RECOVERY_OPTION} = '=bogus';
        storageTest()->put($strConfigFile, iniRender($oConfig, true));

        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSet(CFGOPT_CONFIG, $strConfigFile);

        $self->configTestLoadExpect(CFGBLDCMD_RESTORE, ERROR_OPTION_INVALID_VALUE, '=bogus', CFGBLDOPT_RECOVERY_OPTION);
    }

    if ($self->begin(cfgCommandName(CFGCMD_RESTORE) . ' global option ' . CFGBLDOPT_RECOVERY_OPTION))
    {
        $oConfig = {};
        $$oConfig{&CONFIG_SECTION_GLOBAL . ':' . cfgCommandName(CFGCMD_RESTORE)}{&CFGBLDOPT_RECOVERY_OPTION} =
            'archive-command=/path/to/pgbackrest';
        storageTest()->put($strConfigFile, iniRender($oConfig, true));

        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSet(CFGOPT_CONFIG, $strConfigFile);

        $self->configTestLoadExpect(CFGBLDCMD_RESTORE);
        $self->optionTestExpect(CFGOPT_RECOVERY_OPTION, '/path/to/pgbackrest', 'archive-command');
    }

    if ($self->begin(cfgCommandName(CFGCMD_RESTORE) . ' stanza option ' . CFGBLDOPT_RECOVERY_OPTION))
    {
        $oConfig = {};
        $$oConfig{$self->stanza()}{&CFGBLDOPT_RECOVERY_OPTION} = ['standby-mode=on', 'a=b'];
        storageTest()->put($strConfigFile, iniRender($oConfig, true));

        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSet(CFGOPT_CONFIG, $strConfigFile);

        $self->configTestLoadExpect(CFGBLDCMD_RESTORE);
        $self->optionTestExpect(CFGOPT_RECOVERY_OPTION, 'b', 'a');
        $self->optionTestExpect(CFGOPT_RECOVERY_OPTION, 'on', 'standby-mode');
    }

    if ($self->begin(cfgCommandName(CFGCMD_BACKUP) . ' option ' . CFGBLDOPT_DB_PATH))
    {
        $oConfig = {};
        $$oConfig{$self->stanza()}{&CFGBLDOPT_DB_PATH} = '/path/to/db';
        storageTest()->put($strConfigFile, iniRender($oConfig, true));

        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_CONFIG, $strConfigFile);

        $self->configTestLoadExpect(CFGBLDCMD_BACKUP);
        $self->optionTestExpect(CFGOPT_DB_PATH, '/path/to/db');
    }

    if ($self->begin(cfgCommandName(CFGCMD_BACKUP) . ' option ' . CFGBLDOPT_ARCHIVE_CHECK))
    {
        $oConfig = {};
        $$oConfig{$self->stanza()}{&CFGBLDOPT_DB_PATH} = '/path/to/db';
        storageTest()->put($strConfigFile, iniRender($oConfig, true));

        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_CONFIG, $strConfigFile);
        $self->optionTestSetBool(CFGOPT_ARCHIVE_CHECK, false);

        $self->configTestLoadExpect(CFGBLDCMD_BACKUP);
        $self->optionTestExpect(CFGOPT_ONLINE, true);
        $self->optionTestExpect(CFGOPT_ARCHIVE_CHECK, false);
    }

    if ($self->begin(cfgCommandName(CFGCMD_ARCHIVE_PUSH) . ' option ' . CFGBLDOPT_DB_PATH))
    {
        $oConfig = {};
        $$oConfig{$self->stanza()}{&CFGBLDOPT_DB_PATH} = '/path/to/db';
        storageTest()->put($strConfigFile, iniRender($oConfig, true));

        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_CONFIG, $strConfigFile);

        $self->configTestLoadExpect(CFGBLDCMD_ARCHIVE_PUSH);
        $self->optionTestExpect(CFGOPT_DB_PATH, '/path/to/db');
    }

    if ($self->begin(cfgCommandName(CFGCMD_BACKUP) . ' option ' . CFGBLDOPT_REPO_PATH))
    {
        $oConfig = {};
        $$oConfig{&CONFIG_SECTION_GLOBAL}{&CFGBLDOPT_REPO_PATH} = '/repo';
        storageTest()->put($strConfigFile, iniRender($oConfig, true));

        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSet(CFGOPT_CONFIG, $strConfigFile);

        $self->configTestLoadExpect(CFGBLDCMD_BACKUP);
        $self->optionTestExpect(CFGOPT_REPO_PATH, '/repo');
    }

    if ($self->begin(cfgCommandName(CFGCMD_BACKUP) . ' option ' . CFGBLDOPT_REPO_PATH . ' multiple times'))
    {
        $oConfig = {};
        $$oConfig{&CONFIG_SECTION_GLOBAL}{&CFGBLDOPT_REPO_PATH} = ['/repo', '/repo2'];
        storageTest()->put($strConfigFile, iniRender($oConfig, true));

        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_DB_PATH, '/db');
        $self->optionTestSet(CFGOPT_CONFIG, $strConfigFile);

        $self->configTestLoadExpect(CFGBLDCMD_BACKUP, ERROR_OPTION_MULTIPLE_VALUE, CFGBLDOPT_REPO_PATH);
    }
}

####################################################################################################################################
# Getters
####################################################################################################################################
# Change this from the default so the same stanza is not used in all tests.
sub stanza {return 'main'};

1;
