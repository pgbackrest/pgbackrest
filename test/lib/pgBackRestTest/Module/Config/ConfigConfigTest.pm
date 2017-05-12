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
use pgBackRest::FileCommon;

use pgBackRestTest::Common::RunTest;

####################################################################################################################################
# run
####################################################################################################################################
sub run
{
    my $self = shift;

    my $oOption = {};
    my $oConfig = {};
    my $strConfigFile = $self->testPath() . '/pgbackrest.conf';

    if ($self->begin('set and negate option ' . OPTION_CONFIG))
    {
        $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
        $self->optionSetTest($oOption, OPTION_DB_PATH, '/db');
        $self->optionSetTest($oOption, OPTION_CONFIG, '/dude/dude.conf');
        $self->optionBoolSetTest($oOption, OPTION_CONFIG, false);

        $self->configLoadExpect($oOption, CMD_BACKUP, ERROR_OPTION_NEGATE, OPTION_CONFIG);
    }

    if ($self->begin('option ' . OPTION_CONFIG))
    {
        $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
        $self->optionSetTest($oOption, OPTION_DB_PATH, '/db');
        $self->optionBoolSetTest($oOption, OPTION_CONFIG, false);

        $self->configLoadExpect($oOption, CMD_BACKUP);
        $self->optionTestExpect(OPTION_CONFIG);
    }

    if ($self->begin('default option ' . OPTION_CONFIG))
    {
        $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
        $self->optionSetTest($oOption, OPTION_DB_PATH, '/db');

        $self->configLoadExpect($oOption, CMD_BACKUP);
        $self->optionTestExpect(OPTION_CONFIG, OPTION_DEFAULT_CONFIG);
    }

    if ($self->begin('config file is a path'))
    {
        $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
        $self->optionSetTest($oOption, OPTION_DB_PATH, '/db');
        $self->optionSetTest($oOption, OPTION_CONFIG, $self->testPath());

        $self->configLoadExpect($oOption, CMD_BACKUP, ERROR_FILE_INVALID, $self->testPath());
    }

    if ($self->begin('load from config stanza command section - option ' . OPTION_PROCESS_MAX))
    {
        $oConfig = {};
        $$oConfig{$self->stanza() . ':' . &CMD_BACKUP}{&OPTION_PROCESS_MAX} = 2;
        fileStringWrite($strConfigFile, iniRender($oConfig, true));

        $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
        $self->optionSetTest($oOption, OPTION_DB_PATH, '/db');
        $self->optionSetTest($oOption, OPTION_CONFIG, $strConfigFile);

        $self->configLoadExpect($oOption, CMD_BACKUP);
        $self->optionTestExpect(OPTION_PROCESS_MAX, 2);
    }

    if ($self->begin('load from config stanza section - option ' . OPTION_PROCESS_MAX))
    {
        $oConfig = {};
        $$oConfig{$self->stanza()}{&OPTION_PROCESS_MAX} = 3;
        fileStringWrite($strConfigFile, iniRender($oConfig, true));

        $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
        $self->optionSetTest($oOption, OPTION_DB_PATH, '/db');
        $self->optionSetTest($oOption, OPTION_CONFIG, $strConfigFile);

        $self->configLoadExpect($oOption, CMD_BACKUP);
        $self->optionTestExpect(OPTION_PROCESS_MAX, 3);
    }

    if ($self->begin('load from config global command section - option thread-max'))
    {
        $oConfig = {};
        $$oConfig{&CONFIG_SECTION_GLOBAL . ':' . &CMD_BACKUP}{'thread-max'} = 2;
        fileStringWrite($strConfigFile, iniRender($oConfig, true));

        $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
        $self->optionSetTest($oOption, OPTION_DB_PATH, '/db');
        $self->optionSetTest($oOption, OPTION_CONFIG, $strConfigFile);

        $self->configLoadExpect($oOption, CMD_BACKUP);
        $self->optionTestExpect(OPTION_PROCESS_MAX, 2);
    }

    if ($self->begin('load from config global section - option ' . OPTION_PROCESS_MAX))
    {
        $oConfig = {};
        $$oConfig{&CONFIG_SECTION_GLOBAL}{&OPTION_PROCESS_MAX} = 5;
        fileStringWrite($strConfigFile, iniRender($oConfig, true));

        $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
        $self->optionSetTest($oOption, OPTION_DB_PATH, '/db');
        $self->optionSetTest($oOption, OPTION_CONFIG, $strConfigFile);

        $self->configLoadExpect($oOption, CMD_BACKUP);
        $self->optionTestExpect(OPTION_PROCESS_MAX, 5);
    }

    if ($self->begin('default - option ' . OPTION_PROCESS_MAX))
    {
        $oConfig = {};
        fileStringWrite($strConfigFile, iniRender($oConfig, true));

        $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
        $self->optionSetTest($oOption, OPTION_DB_PATH, '/db');
        $self->optionSetTest($oOption, OPTION_CONFIG, $strConfigFile);

        $self->configLoadExpect($oOption, CMD_BACKUP);
        $self->optionTestExpect(OPTION_PROCESS_MAX, 1);
    }

    if ($self->begin('command-line override - option ' . OPTION_PROCESS_MAX))
    {
        $oConfig = {};
        $$oConfig{&CONFIG_SECTION_GLOBAL}{&OPTION_PROCESS_MAX} = 9;
        fileStringWrite($strConfigFile, iniRender($oConfig, true));

        $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
        $self->optionSetTest($oOption, OPTION_DB_PATH, '/db');
        $self->optionSetTest($oOption, OPTION_PROCESS_MAX, 7);
        $self->optionSetTest($oOption, OPTION_CONFIG, $strConfigFile);

        $self->configLoadExpect($oOption, CMD_BACKUP);
        $self->optionTestExpect(OPTION_PROCESS_MAX, 7);
    }

    if ($self->begin('invalid boolean - option ' . OPTION_HARDLINK))
    {
        $oConfig = {};
        $$oConfig{&CONFIG_SECTION_GLOBAL . ':' . &CMD_BACKUP}{&OPTION_HARDLINK} = 'Y';
        fileStringWrite($strConfigFile, iniRender($oConfig, true));

        $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
        $self->optionSetTest($oOption, OPTION_DB_PATH, '/db');
        $self->optionSetTest($oOption, OPTION_CONFIG, $strConfigFile);

        $self->configLoadExpect($oOption, CMD_BACKUP, ERROR_OPTION_INVALID_VALUE, 'Y', OPTION_HARDLINK);
    }

    if ($self->begin('invalid value - option ' . OPTION_LOG_LEVEL_CONSOLE))
    {
        $oConfig = {};
        $$oConfig{&CONFIG_SECTION_GLOBAL}{&OPTION_LOG_LEVEL_CONSOLE} = BOGUS;
        fileStringWrite($strConfigFile, iniRender($oConfig, true));

        $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
        $self->optionSetTest($oOption, OPTION_DB_PATH, '/db');
        $self->optionSetTest($oOption, OPTION_CONFIG, $strConfigFile);

        $self->configLoadExpect($oOption, CMD_BACKUP, ERROR_OPTION_INVALID_VALUE, BOGUS, OPTION_LOG_LEVEL_CONSOLE);
    }

    if ($self->begin('valid value - option ' . OPTION_LOG_LEVEL_CONSOLE))
    {
        $oConfig = {};
        $$oConfig{&CONFIG_SECTION_GLOBAL}{&OPTION_LOG_LEVEL_CONSOLE} = lc(INFO);
        fileStringWrite($strConfigFile, iniRender($oConfig, true));

        $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
        $self->optionSetTest($oOption, OPTION_DB_PATH, '/db');
        $self->optionSetTest($oOption, OPTION_CONFIG, $strConfigFile);

        $self->configLoadExpect($oOption, CMD_RESTORE);
    }

    if ($self->begin('archive-push - option ' . OPTION_LOG_LEVEL_CONSOLE))
    {
        $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
        $self->optionSetTest($oOption, OPTION_CONFIG, $strConfigFile);

        $self->configLoadExpect($oOption, CMD_ARCHIVE_PUSH);
    }

    if ($self->begin(CMD_EXPIRE . ' ' . OPTION_RETENTION_FULL))
    {
        $oConfig = {};
        $$oConfig{$self->stanza() . ':' . &CMD_EXPIRE}{&OPTION_RETENTION_FULL} = 2;
        fileStringWrite($strConfigFile, iniRender($oConfig, true));

        $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
        $self->optionSetTest($oOption, OPTION_CONFIG, $strConfigFile);

        $self->configLoadExpect($oOption, CMD_EXPIRE);
        $self->optionTestExpect(OPTION_RETENTION_FULL, 2);
    }

    if ($self->begin(CMD_BACKUP . ' option ' . OPTION_COMPRESS))
    {
        $oConfig = {};
        $$oConfig{&CONFIG_SECTION_GLOBAL . ':' . &CMD_BACKUP}{&OPTION_COMPRESS} = 'n';
        fileStringWrite($strConfigFile, iniRender($oConfig, true));

        $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
        $self->optionSetTest($oOption, OPTION_DB_PATH, '/db');
        $self->optionSetTest($oOption, OPTION_CONFIG, $strConfigFile);

        $self->configLoadExpect($oOption, CMD_BACKUP);
        $self->optionTestExpect(OPTION_COMPRESS, false);
    }

    if ($self->begin(CMD_RESTORE . ' global option ' . OPTION_RESTORE_RECOVERY_OPTION . ' error'))
    {
        $oConfig = {};
        $$oConfig{&CONFIG_SECTION_GLOBAL . ':' . &CMD_RESTORE}{&OPTION_RESTORE_RECOVERY_OPTION} = 'bogus=';
        fileStringWrite($strConfigFile, iniRender($oConfig, true));

        $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
        $self->optionSetTest($oOption, OPTION_DB_PATH, '/db');
        $self->optionSetTest($oOption, OPTION_CONFIG, $strConfigFile);

        $self->configLoadExpect($oOption, CMD_RESTORE, ERROR_OPTION_INVALID_VALUE, 'bogus=', OPTION_RESTORE_RECOVERY_OPTION);
    }

    if ($self->begin(CMD_RESTORE . ' global option ' . OPTION_RESTORE_RECOVERY_OPTION . ' error'))
    {
        $oConfig = {};
        $$oConfig{&CONFIG_SECTION_GLOBAL . ':' . &CMD_RESTORE}{&OPTION_RESTORE_RECOVERY_OPTION} = '=bogus';
        fileStringWrite($strConfigFile, iniRender($oConfig, true));

        $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
        $self->optionSetTest($oOption, OPTION_DB_PATH, '/db');
        $self->optionSetTest($oOption, OPTION_CONFIG, $strConfigFile);

        $self->configLoadExpect($oOption, CMD_RESTORE, ERROR_OPTION_INVALID_VALUE, '=bogus', OPTION_RESTORE_RECOVERY_OPTION);
    }

    if ($self->begin(CMD_RESTORE . ' global option ' . OPTION_RESTORE_RECOVERY_OPTION))
    {
        $oConfig = {};
        $$oConfig{&CONFIG_SECTION_GLOBAL . ':' . &CMD_RESTORE}{&OPTION_RESTORE_RECOVERY_OPTION} =
            'archive-command=/path/to/pgbackrest';
        fileStringWrite($strConfigFile, iniRender($oConfig, true));

        $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
        $self->optionSetTest($oOption, OPTION_DB_PATH, '/db');
        $self->optionSetTest($oOption, OPTION_CONFIG, $strConfigFile);

        $self->configLoadExpect($oOption, CMD_RESTORE);
        $self->optionTestExpect(OPTION_RESTORE_RECOVERY_OPTION, '/path/to/pgbackrest', 'archive-command');
    }

    if ($self->begin(CMD_RESTORE . ' stanza option ' . OPTION_RESTORE_RECOVERY_OPTION))
    {
        $oConfig = {};
        $$oConfig{$self->stanza()}{&OPTION_RESTORE_RECOVERY_OPTION} = ['standby-mode=on', 'a=b'];
        fileStringWrite($strConfigFile, iniRender($oConfig, true));

        $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
        $self->optionSetTest($oOption, OPTION_DB_PATH, '/db');
        $self->optionSetTest($oOption, OPTION_CONFIG, $strConfigFile);

        $self->configLoadExpect($oOption, CMD_RESTORE);
        $self->optionTestExpect(OPTION_RESTORE_RECOVERY_OPTION, 'b', 'a');
        $self->optionTestExpect(OPTION_RESTORE_RECOVERY_OPTION, 'on', 'standby-mode');
    }

    if ($self->begin(CMD_BACKUP . ' option ' . OPTION_DB_PATH))
    {
        $oConfig = {};
        $$oConfig{$self->stanza()}{&OPTION_DB_PATH} = '/path/to/db';
        fileStringWrite($strConfigFile, iniRender($oConfig, true));

        $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
        $self->optionSetTest($oOption, OPTION_CONFIG, $strConfigFile);

        $self->configLoadExpect($oOption, CMD_BACKUP);
        $self->optionTestExpect(OPTION_DB_PATH, '/path/to/db');
    }

    if ($self->begin(CMD_BACKUP . ' option ' . OPTION_BACKUP_ARCHIVE_CHECK))
    {
        $oConfig = {};
        $$oConfig{$self->stanza()}{&OPTION_DB_PATH} = '/path/to/db';
        fileStringWrite($strConfigFile, iniRender($oConfig, true));

        $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
        $self->optionSetTest($oOption, OPTION_CONFIG, $strConfigFile);
        $self->optionBoolSetTest($oOption, OPTION_BACKUP_ARCHIVE_CHECK, false);

        $self->configLoadExpect($oOption, CMD_BACKUP);
        $self->optionTestExpect(OPTION_ONLINE, true);
        $self->optionTestExpect(OPTION_BACKUP_ARCHIVE_CHECK, false);
    }

    if ($self->begin(CMD_ARCHIVE_PUSH . ' option ' . OPTION_DB_PATH))
    {
        $oConfig = {};
        $$oConfig{$self->stanza()}{&OPTION_DB_PATH} = '/path/to/db';
        fileStringWrite($strConfigFile, iniRender($oConfig, true));

        $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
        $self->optionSetTest($oOption, OPTION_CONFIG, $strConfigFile);

        $self->configLoadExpect($oOption, CMD_ARCHIVE_PUSH);
        $self->optionTestExpect(OPTION_DB_PATH, '/path/to/db');
    }

    if ($self->begin(CMD_BACKUP . ' option ' . OPTION_REPO_PATH))
    {
        $oConfig = {};
        $$oConfig{&CONFIG_SECTION_GLOBAL}{&OPTION_REPO_PATH} = '/repo';
        fileStringWrite($strConfigFile, iniRender($oConfig, true));

        $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
        $self->optionSetTest($oOption, OPTION_DB_PATH, '/db');
        $self->optionSetTest($oOption, OPTION_CONFIG, $strConfigFile);

        $self->configLoadExpect($oOption, CMD_BACKUP);
        $self->optionTestExpect(OPTION_REPO_PATH, '/repo');
    }

    if ($self->begin(CMD_BACKUP . ' option ' . OPTION_REPO_PATH . ' multiple times'))
    {
        $oConfig = {};
        $$oConfig{&CONFIG_SECTION_GLOBAL}{&OPTION_REPO_PATH} = ['/repo', '/repo2'];
        fileStringWrite($strConfigFile, iniRender($oConfig, true));

        $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
        $self->optionSetTest($oOption, OPTION_DB_PATH, '/db');
        $self->optionSetTest($oOption, OPTION_CONFIG, $strConfigFile);

        $self->configLoadExpect($oOption, CMD_BACKUP, ERROR_OPTION_MULTIPLE_VALUE, OPTION_REPO_PATH);
    }
}

####################################################################################################################################
# Getters
####################################################################################################################################
# Change this from the default so the same stanza is not used in all tests.
sub stanza {return 'main'};

1;
