####################################################################################################################################
# InfoUnitTest.pm - Unit tests for Info module
####################################################################################################################################
package pgBackRestTest::Module::Info::InfoUnitTest;
use parent 'pgBackRestTest::Env::ConfigEnvTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use File::Basename qw(dirname);
use Storable qw(dclone);

use pgBackRest::Backup::Info;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Lock;
use pgBackRest::Common::Log;
use pgBackRest::Config::Config;
use pgBackRest::DbVersion;
use pgBackRest::Info;
use pgBackRest::InfoCommon;
use pgBackRest::Manifest;
use pgBackRest::Protocol::Helper;
use pgBackRest::Protocol::Storage::Helper;
use pgBackRest::Storage::Local;

use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::RunTest;
use pgBackRestTest::Env::ExpireEnvTest;
use pgBackRestTest::Common::FileTest;
use pgBackRestTest::Env::Host::HostBackupTest;
use pgBackRestTest::Env::HostEnvTest;

####################################################################################################################################
# initModule
####################################################################################################################################
use constant STANZA_ENCRYPT                             => 'encrypt';

####################################################################################################################################
# initModule
####################################################################################################################################
sub initModule
{
    my $self = shift;

    $self->{strRepoPath} = $self->testPath() . '/repo';
    $self->{strArchivePath} = "$self->{strRepoPath}/archive/" . $self->stanza();
    $self->{strBackupPath} = "$self->{strRepoPath}/backup/" . $self->stanza();
    $self->{strDbPath} = $self->testPath() . '/db';
    $self->{strArchivePathEncrypt} = "$self->{strRepoPath}/archive/" . STANZA_ENCRYPT;
    $self->{strBackupPathEncrypt} = "$self->{strRepoPath}/backup/" . STANZA_ENCRYPT;
}

####################################################################################################################################
# initTest
####################################################################################################################################
sub initTest
{
    my $self = shift;

    # Create parent path for pg_control
    storageTest()->pathCreate(($self->{strDbPath} . '/' . DB_PATH_GLOBAL), {bCreateParent => true});

    # Create archive info path
    storageTest()->pathCreate($self->{strArchivePath}, {bIgnoreExists => true, bCreateParent => true});

    # Create backup info path
    storageTest()->pathCreate($self->{strBackupPath}, {bIgnoreExists => true, bCreateParent => true});
}

####################################################################################################################################
# initStanzaCreate - initialize options and create the stanza object
####################################################################################################################################
sub initStanzaCreate
{
    my $self = shift;
    my $strStanza = shift;
    my $bEncrypted = shift;
    my $strDbVersion = shift;

    # Set options for stanzaCreate
    my $rhConfig = $self->configTestClear();

    if (!defined($strStanza))
    {
        $strStanza = $self->stanza();
    }

    if (defined($bEncrypted) && $bEncrypted == true)
    {
        $self->optionTestSet(CFGOPT_REPO_CIPHER_TYPE, CFGOPTVAL_REPO_CIPHER_TYPE_AES_256_CBC);
        $self->optionTestSet(CFGOPT_REPO_CIPHER_PASS, 'x');
    }

    if (!defined($strDbVersion))
    {
        $strDbVersion = PG_VERSION_94;
    }

    $self->optionTestSet(CFGOPT_STANZA, $strStanza);
    $self->optionTestSet(CFGOPT_DB_PATH, $self->{strDbPath});
    $self->optionTestSet(CFGOPT_REPO_PATH, $self->{strRepoPath});
    $self->optionTestSet(CFGOPT_LOG_PATH, $self->testPath());
    $self->optionTestSetBool(CFGOPT_ONLINE, false);
    $self->optionTestSet(CFGOPT_DB_TIMEOUT, 5);
    $self->optionTestSet(CFGOPT_PROTOCOL_TIMEOUT, 6);

    $self->configTestLoad(CFGCMD_STANZA_CREATE);
    $self->configTestSet($rhConfig);

    my $oTestObject = "oExpireTest";

    # Create the test object
    if (defined($bEncrypted) && $bEncrypted == true)
    {
        $oTestObject = "oExpireTestEncrypt";
    }

    $self->{$oTestObject} = new pgBackRestTest::Env::ExpireEnvTest(undef, $self->backrestExe(), storageRepo(), undef, $self);
    $self->{$oTestObject}->stanzaCreate($strStanza, $strDbVersion);
}

####################################################################################################################################
# initStanzaUpgrade - initialize options and create the stanza object
####################################################################################################################################
sub initStanzaUpgrade
{
    my $self = shift;

    # Set options for stanzaCreate
    my $rhConfig = $self->configTestClear();

    $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
    $self->optionTestSet(CFGOPT_DB_PATH, $self->{strDbPath});
    $self->optionTestSet(CFGOPT_REPO_PATH, $self->{strRepoPath});
    $self->optionTestSet(CFGOPT_LOG_PATH, $self->testPath());
    $self->optionTestSetBool(CFGOPT_ONLINE, false);
    $self->optionTestSet(CFGOPT_DB_TIMEOUT, 5);
    $self->optionTestSet(CFGOPT_PROTOCOL_TIMEOUT, 6);

    $self->configTestLoad(CFGCMD_STANZA_UPGRADE);
    $self->configTestSet($rhConfig);

    # Create the test object
    $self->{oExpireTest} = new pgBackRestTest::Env::ExpireEnvTest(undef, $self->backrestExe(), storageRepo(), undef, $self);

    $self->{oExpireTest}->stanzaUpgrade($self->stanza(), PG_VERSION_95);
}

####################################################################################################################################
# run
####################################################################################################################################
sub run
{
    my $self = shift;

    # Used to create backups and WAL to test
    use constant SECONDS_PER_DAY => 86400;
    my $lBaseTime = 1486137448 - (60 * SECONDS_PER_DAY);

    # Create the initial backup "day" number - backup day is incremented for each backup
    my $iLastBackup = 5;

    ################################################################################################################################
    if ($self->begin("Info"))
    {
        $self->configTestLoad(CFGCMD_INFO);

        my $oInfo = new pgBackRest::Info();

        # Output No stanzas exist in default text option
        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(sub {$oInfo->process()}, 0, 'No stanzas exist and default text option');

        # Invalid option
        #---------------------------------------------------------------------------------------------------------------------------
        cfgOptionSet(CFGOPT_OUTPUT, BOGUS);

        $self->testException(sub {$oInfo->process()}, ERROR_ASSERT, "invalid info output option '" . BOGUS . "'");

        # Output json option with no stanza defined
        #---------------------------------------------------------------------------------------------------------------------------
        $self->optionTestSet(CFGOPT_OUTPUT, CFGOPTVAL_INFO_OUTPUT_JSON);
        $self->configTestLoad(CFGCMD_INFO);

        $self->testResult(sub {$oInfo->process()}, 0, 'json option');

        # Add linefeed to JSON
        #---------------------------------------------------------------------------------------------------------------------------
        my $strJson = '[{"archive" : 1}]';
        $self->testResult(sub {$oInfo->outputJSON($strJson)}, 1, 'add linefeed to json');

        # Missing stanza path
        #---------------------------------------------------------------------------------------------------------------------------
        my $hyStanza = $oInfo->stanzaList(BOGUS);

        $self->testResult(sub {$oInfo->formatText($hyStanza)},
            "stanza: bogus\n    status: error (missing stanza path)\n",
            'missing stanza path');

        # formatBackupText - coverage for certain conditions
        #---------------------------------------------------------------------------------------------------------------------------
        my $oBackupHash =
        {
            'archive' =>
            {
                'start' => 1,
                'stop' => undef
            },
            'timestamp' =>
            {
                'start' => 1481039848,
                'stop' => 1481039848,
            },
            'label' => 'BACKUPLABEL',
            'reference' => ['BACKUPREFERENCE'],
            'type' => 'BACKUPTYPE',
        };

        $self->testResult(sub {$oInfo->formatTextBackup($oBackupHash)},
            "        BACKUPTYPE backup: BACKUPLABEL\n" .
            "            timestamp start/stop: 2016-12-06 15:57:28 / 2016-12-06 15:57:28\n" .
            "            wal start/stop: n/a\n" .
            "            database size: , backup size: \n" .
            "            repository size: , repository backup size: \n" .
            "            backup reference list: BACKUPREFERENCE",
            'formatTextBackup');

        # Test !isRepoLocal branch
        #---------------------------------------------------------------------------------------------------------------------------
        cfgOptionSet(CFGOPT_BACKUP_HOST, false);
        cfgOptionSet(CFGOPT_BACKUP_CONFIG, BOGUS);
        $self->testException(sub {$oInfo->stanzaList(BOGUS)}, ERROR_ASSERT, "option backup-cmd is required");

        # dbArchiveSection() -- no archive
        #---------------------------------------------------------------------------------------------------------------------------
        my $hDbInfo =
        {
            &INFO_HISTORY_ID => 1,
            &INFO_DB_VERSION => PG_VERSION_94,
            &INFO_SYSTEM_ID => WAL_VERSION_94_SYS_ID,
        };

        $self->testResult(sub {$oInfo->dbArchiveSection($hDbInfo, PG_VERSION_94 . '-1', $self->{strArchivePath}, PG_VERSION_94,
            WAL_VERSION_94_SYS_ID)}, "{database => {id => 1}, id => 9.4-1, max => [undef], min => [undef]}",
            'no archive, db-ver match, db-sys match');

        $self->testResult(sub {$oInfo->dbArchiveSection($hDbInfo, PG_VERSION_94 . '-1', $self->{strArchivePath}, PG_VERSION_94,
            WAL_VERSION_95_SYS_ID)}, undef, 'no archive, db-ver match, db-sys mismatch');

        $hDbInfo->{&INFO_DB_VERSION} = PG_VERSION_95;
        $self->testResult(sub {$oInfo->dbArchiveSection($hDbInfo, PG_VERSION_94 . '-1', $self->{strArchivePath}, PG_VERSION_94,
            WAL_VERSION_94_SYS_ID)}, undef, 'no archive, db-ver mismatch, db-sys match');

        $hDbInfo->{&INFO_SYSTEM_ID} = WAL_VERSION_95_SYS_ID;
        $self->testResult(sub {$oInfo->dbArchiveSection($hDbInfo, PG_VERSION_94 . '-1', $self->{strArchivePath}, PG_VERSION_94,
            WAL_VERSION_94_SYS_ID)}, undef, 'no archive, db-ver mismatch, db-sys mismatch');

        # Create more than one stanza but no data
        #---------------------------------------------------------------------------------------------------------------------------
        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_REPO_PATH, $self->{strRepoPath});
        $self->configTestLoad(CFGCMD_INFO);

        # Create archive and backup paths but no info files
        storageTest()->pathCreate("$self->{strRepoPath}/archive/" . BOGUS, {bIgnoreExists => true, bCreateParent => true});
        storageTest()->pathCreate("$self->{strRepoPath}/backup/" . BOGUS, {bIgnoreExists => true, bCreateParent => true});

        # Get a list of all stanzas in the repo
        $hyStanza = $oInfo->stanzaList();

        $self->testResult(sub {$oInfo->formatText($hyStanza)},
            "stanza: bogus\n    status: error (missing stanza data)\n\n" .
            "stanza: db\n    status: error (missing stanza data)\n",
            'fomatText() multiple stanzas missing data');

        # Define the stanza option
        #---------------------------------------------------------------------------------------------------------------------------
        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestClear(CFGOPT_OUTPUT);
        $self->configTestLoad(CFGCMD_INFO);

        $self->testResult(sub {$oInfo->process()}, 0, 'stanza set');

        # Create the stanza - no WAL or backups
        #---------------------------------------------------------------------------------------------------------------------------
        $self->initStanzaCreate();
        $self->configTestLoad(CFGCMD_INFO);

        $self->testResult(sub {$oInfo->formatText($oInfo->stanzaList())},
            "stanza: bogus\n    status: error (missing stanza data)\n\n" .
            "stanza: db\n    status: error (no valid backups)\n\n" .
            "    db (current)\n        wal archive min/max (9.4-1): none present\n",
            "formatText() multiple stanzas, one missing data");

        $hyStanza = $oInfo->stanzaList($self->stanza());
        $self->testResult(sub {$oInfo->formatText($hyStanza)},
            "stanza: db\n    status: error (no valid backups)\n\n" .
            "    db (current)\n        wal archive min/max (9.4-1): none present\n",
            "formatText() one stanza");

        # Create a backup and list backup for just one stanza
        #---------------------------------------------------------------------------------------------------------------------------
        $self->{oExpireTest}->backupCreate($self->stanza(), CFGOPTVAL_BACKUP_TYPE_FULL, $lBaseTime += SECONDS_PER_DAY, -1, -1);

        $hyStanza = $oInfo->stanzaList($self->stanza());
        $self->testResult(sub {$oInfo->formatText($hyStanza)},
            "stanza: db\n    status: ok\n\n    db (current)\n        wal archive min/max (9.4-1): none present\n\n" .
            "        full backup: 20161206-155728F\n" .
            "            timestamp start/stop: 2016-12-06 15:57:28 / 2016-12-06 15:57:28\n" .
            "            wal start/stop: n/a\n            database size: 0B, backup size: 0B\n" .
            "            repository size: 0B, repository backup size: 0B\n",
            "formatText() one stanza");

        # Coverage for major WAL paths with no WAL
        #---------------------------------------------------------------------------------------------------------------------------
        storageTest()->pathCreate($self->{strArchivePath} . "/9.4-1/0000000100000000", {bIgnoreExists => true});
        storageTest()->pathCreate($self->{strArchivePath} . "/9.4-1/0000000200000000", {bIgnoreExists => true});
        $hyStanza = $oInfo->stanzaList($self->stanza());
        $self->testResult(sub {$oInfo->formatText($hyStanza)},
            "stanza: db\n    status: ok\n\n    db (current)\n        wal archive min/max (9.4-1): none present\n\n" .
            "        full backup: 20161206-155728F\n" .
            "            timestamp start/stop: 2016-12-06 15:57:28 / 2016-12-06 15:57:28\n" .
            "            wal start/stop: n/a\n            database size: 0B, backup size: 0B\n" .
            "            repository size: 0B, repository backup size: 0B\n",
            "formatText() major WAL paths with no WAL");

        # Upgrade postgres version and backup with WAL
        #---------------------------------------------------------------------------------------------------------------------------
        undef($self->{oExpireTest});
        $self->initStanzaUpgrade();

        $self->configTestLoad(CFGCMD_INFO);

        $self->{oExpireTest}->backupCreate($self->stanza(), CFGOPTVAL_BACKUP_TYPE_FULL, $lBaseTime += SECONDS_PER_DAY, 1, 1);
        $self->{oExpireTest}->backupCreate($self->stanza(), CFGOPTVAL_BACKUP_TYPE_DIFF, $lBaseTime += SECONDS_PER_DAY, 1, 1);

        # Remove the 9.4-1 path for condition test coverage
        storageTest()->remove($self->{strArchivePath} . "/9.4-1/", {bRecurse => true});

        $hyStanza = $oInfo->stanzaList($self->stanza());
        $self->testResult(sub {$oInfo->formatText($hyStanza)},
            "stanza: db\n    status: ok\n" .
            "\n    db (current)\n" .
            "        wal archive min/max (9.5-2): 000000010000000000000000 / 000000010000000000000003\n\n" .
            "        full backup: 20161207-155728F\n" .
            "            timestamp start/stop: 2016-12-07 15:57:28 / 2016-12-07 15:57:28\n" .
            "            wal start/stop: 000000010000000000000000 / 000000010000000000000000\n" .
            "            database size: 0B, backup size: 0B\n" .
            "            repository size: 0B, repository backup size: 0B\n\n" .
            "        diff backup: 20161207-155728F_20161208-155728D\n" .
            "            timestamp start/stop: 2016-12-08 15:57:28 / 2016-12-08 15:57:28\n" .
            "            wal start/stop: 000000010000000000000002 / 000000010000000000000002\n" .
            "            database size: 0B, backup size: 0B\n" .
            "            repository size: 0B, repository backup size: 0B\n" .
            "\n    db (prior)\n" .
            "        full backup: 20161206-155728F\n" .
            "            timestamp start/stop: 2016-12-06 15:57:28 / 2016-12-06 15:57:28\n" .
            "            wal start/stop: n/a\n" .
            "            database size: 0B, backup size: 0B\n" .
            "            repository size: 0B, repository backup size: 0B\n",
            "formatText() multiple DB versions");

        # dbArchiveSection() -- with archive
        #---------------------------------------------------------------------------------------------------------------------------
        $hDbInfo->{&INFO_HISTORY_ID} = 2;
        $hDbInfo->{&INFO_DB_VERSION} = PG_VERSION_95;
        $hDbInfo->{&INFO_SYSTEM_ID} = WAL_VERSION_95_SYS_ID;

        my $strArchiveId = PG_VERSION_95 . '-2';
        $self->testResult(sub {$oInfo->dbArchiveSection($hDbInfo, $strArchiveId, "$self->{strArchivePath}/$strArchiveId",
            PG_VERSION_95, WAL_VERSION_95_SYS_ID)},
            "{database => {id => 2}, id => 9.5-2, max => 000000010000000000000003, min => 000000010000000000000000}",
            'archive, db-ver match, db-sys-id match');

        $self->testResult(sub {$oInfo->dbArchiveSection($hDbInfo, $strArchiveId, "$self->{strArchivePath}/$strArchiveId",
            PG_VERSION_95, WAL_VERSION_94_SYS_ID)},
            "{database => {id => 2}, id => 9.5-2, max => 000000010000000000000003, min => 000000010000000000000000}",
            'archive, db-ver match, db-sys-id mismatch');

        $self->testResult(sub {$oInfo->dbArchiveSection($hDbInfo, $strArchiveId, "$self->{strArchivePath}/$strArchiveId",
            PG_VERSION_94, WAL_VERSION_95_SYS_ID)},
            "{database => {id => 2}, id => 9.5-2, max => 000000010000000000000003, min => 000000010000000000000000}",
            'archive, db-ver mismatch, db-sys-id match');

        $self->testResult(sub {$oInfo->dbArchiveSection($hDbInfo, $strArchiveId, "$self->{strArchivePath}/$strArchiveId",
            PG_VERSION_94, WAL_VERSION_94_SYS_ID)},
            "{database => {id => 2}, id => 9.5-2, max => 000000010000000000000003, min => 000000010000000000000000}",
            'archive, db-ver mismatch, db-sys-id mismatch');

        # Set last backup run in this test set
        $iLastBackup = 8;
    }

    ################################################################################################################################
    if ($self->begin("Info - encryption"))
    {
        # create an unencrypted and an encrypted repo and run a backup in each
        #--------------------------------------------------------------------------------------------------------------------------
        $self->initStanzaCreate();
        $self->{oExpireTest}->backupCreate($self->stanza(), CFGOPTVAL_BACKUP_TYPE_FULL, $lBaseTime += SECONDS_PER_DAY, -1, -1);
        $iLastBackup++;
        my $strUnencryptBackup = ($iLastBackup < 10) ? "0$iLastBackup" : $iLastBackup;

        $self->initStanzaCreate(STANZA_ENCRYPT, true);
        $self->{oExpireTestEncrypt}->backupCreate(STANZA_ENCRYPT, CFGOPTVAL_BACKUP_TYPE_FULL, $lBaseTime += SECONDS_PER_DAY, -1, -1);
        $iLastBackup++;
        my $strEncryptBackup = ($iLastBackup < 10) ? "0$iLastBackup" : $iLastBackup;;

        # Clear the main storage repo settings
        storageRepoCacheClear(STORAGE_REPO);

        # Clear the stanza setting and set the main storage repo basePath to the testing repo path
        $self->optionTestClear(CFGOPT_STANZA);
        $self->optionTestSet(CFGOPT_REPO_PATH, $self->{strRepoPath});

        # Recreate the main storage repo with default unencrypted repo setting and test basePath
        storageRepo();

        # Test the info command without option --stanza being configured
        #--------------------------------------------------------------------------------------------------------------------------
        $self->configTestLoad(CFGCMD_INFO);
        my $oInfo = new pgBackRest::Info();

        $self->testException(sub {$oInfo->stanzaList()}, ERROR_CIPHER,
            "unable to parse '" . $self->{strBackupPathEncrypt} . "/backup.info'" .
            "\nHINT: Is or was the repo encrypted?" .
            "\nHINT: use option --stanza if encryption settings are different for the stanza than the global settings");

        # Test the info command with option --stanza configured
        #--------------------------------------------------------------------------------------------------------------------------
        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->configTestLoad(CFGCMD_INFO);

        $self->testResult(sub {$oInfo->formatText($oInfo->stanzaList($self->stanza()))},
            "stanza: db\n    status: ok\n\n    db (current)\n        wal archive min/max (9.4-1): none present\n\n" .
            "        full backup: 201612" . $strUnencryptBackup . "-155728F\n" .
            "            timestamp start/stop: 2016-12-" . $strUnencryptBackup . " 15:57:28 / 2016-12-" . $strUnencryptBackup .
            " 15:57:28\n" .
            "            wal start/stop: n/a\n            database size: 0B, backup size: 0B\n" .
            "            repository size: 0B, repository backup size: 0B\n",
            "formatText() unencrypted stanza");

        $self->optionTestSet(CFGOPT_STANZA, STANZA_ENCRYPT);
        $self->configTestLoad(CFGCMD_INFO);

        $self->testResult(sub {$oInfo->formatText($oInfo->stanzaList(STANZA_ENCRYPT))},
            "stanza: " . STANZA_ENCRYPT ."\n    status: ok\n\n    db (current)" .
            "\n        wal archive min/max (9.4-1): none present\n\n" .
            "        full backup: 201612" . $strEncryptBackup . "-155728F\n" .
            "            timestamp start/stop: 2016-12-" . $strEncryptBackup . " 15:57:28 / 2016-12-" . $strEncryptBackup .
            " 15:57:28\n" .
            "            wal start/stop: n/a\n            database size: 0B, backup size: 0B\n" .
            "            repository size: 0B, repository backup size: 0B\n",
            "formatText() encrypted stanza");

        # Change the permissions of the backup file so it cannot be read and confirm correct error reported
        #---------------------------------------------------------------------------------------------------------------------------
        forceStorageMode(
            storageRepo({strStanza => STANZA_ENCRYPT}), $self->{strBackupPathEncrypt} . "/backup.inf*", '220');
        $self->testException(sub {$oInfo->backupList(STANZA_ENCRYPT)}, ERROR_FILE_OPEN,
            "unable to open '" . $self->{strBackupPathEncrypt} . "/backup.info': Permission denied");
    }
}

1;
