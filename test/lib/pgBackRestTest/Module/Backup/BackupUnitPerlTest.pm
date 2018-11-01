####################################################################################################################################
# Tests for Backup module
####################################################################################################################################
package pgBackRestTest::Module::Backup::BackupUnitPerlTest;
use parent 'pgBackRestTest::Env::HostEnvTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use File::Basename qw(dirname);
use Storable qw(dclone);

use pgBackRest::Backup::Backup;
use pgBackRest::Backup::Common;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::Common::Wait;
use pgBackRest::Config::Config;
use pgBackRest::DbVersion;
use pgBackRest::Manifest;
use pgBackRest::Protocol::Helper;
use pgBackRest::Protocol::Storage::Helper;
use pgBackRest::Storage::Helper;

use pgBackRestTest::Common::ContainerTest;
use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::FileTest;
use pgBackRestTest::Common::RunTest;
use pgBackRestTest::Env::Host::HostBackupTest;

####################################################################################################################################
# run
####################################################################################################################################
sub run
{
    my $self = shift;

    ################################################################################################################################
    if ($self->begin('backupRegExpGet()'))
    {
        # Expected results matrix
        my $hExpected = {};
        $hExpected->{&false}{&false}{&true}{&false} = '[0-9]{8}\-[0-9]{6}F\_[0-9]{8}\-[0-9]{6}I';
        $hExpected->{&false}{&false}{&true}{&true} = '^[0-9]{8}\-[0-9]{6}F\_[0-9]{8}\-[0-9]{6}I$';
        $hExpected->{&false}{&true}{&false}{&false} = '[0-9]{8}\-[0-9]{6}F\_[0-9]{8}\-[0-9]{6}D';
        $hExpected->{&false}{&true}{&false}{&true} = '^[0-9]{8}\-[0-9]{6}F\_[0-9]{8}\-[0-9]{6}D$';
        $hExpected->{&false}{&true}{&true}{&false} = '[0-9]{8}\-[0-9]{6}F\_[0-9]{8}\-[0-9]{6}(D|I)';
        $hExpected->{&false}{&true}{&true}{&true} = '^[0-9]{8}\-[0-9]{6}F\_[0-9]{8}\-[0-9]{6}(D|I)$';
        $hExpected->{&true}{&false}{&false}{&false} = '[0-9]{8}\-[0-9]{6}F';
        $hExpected->{&true}{&false}{&false}{&true} = '^[0-9]{8}\-[0-9]{6}F$';
        $hExpected->{&true}{&false}{&true}{&false} = '[0-9]{8}\-[0-9]{6}F(\_[0-9]{8}\-[0-9]{6}I){0,1}';
        $hExpected->{&true}{&false}{&true}{&true} = '^[0-9]{8}\-[0-9]{6}F(\_[0-9]{8}\-[0-9]{6}I){0,1}$';
        $hExpected->{&true}{&true}{&false}{&false} = '[0-9]{8}\-[0-9]{6}F(\_[0-9]{8}\-[0-9]{6}D){0,1}';
        $hExpected->{&true}{&true}{&false}{&true} = '^[0-9]{8}\-[0-9]{6}F(\_[0-9]{8}\-[0-9]{6}D){0,1}$';
        $hExpected->{&true}{&true}{&true}{&false} = '[0-9]{8}\-[0-9]{6}F(\_[0-9]{8}\-[0-9]{6}(D|I)){0,1}';
        $hExpected->{&true}{&true}{&true}{&true} = '^[0-9]{8}\-[0-9]{6}F(\_[0-9]{8}\-[0-9]{6}(D|I)){0,1}$';

        # Iterate though all possible combinations
        for (my $bFull = false; $bFull <= true; $bFull++)
        {
        for (my $bDiff = false; $bDiff <= true; $bDiff++)
        {
        for (my $bIncr = false; $bIncr <= true; $bIncr++)
        {
        for (my $bAnchor = false; $bAnchor <= true; $bAnchor++)
        {
            # Make sure that an assertion is thrown if no types are requested
            if (!($bFull || $bDiff || $bIncr))
            {
                $self->testException(
                    sub {backupRegExpGet($bFull, $bDiff, $bIncr, $bAnchor)},
                    ERROR_ASSERT, 'at least one backup type must be selected');
            }
            # Else make sure the returned value is correct
            else
            {
                $self->testResult(
                    sub {backupRegExpGet($bFull, $bDiff, $bIncr, $bAnchor)}, $hExpected->{$bFull}{$bDiff}{$bIncr}{$bAnchor},
                    "expression full $bFull, diff $bDiff, incr $bIncr, anchor $bAnchor = " .
                        $hExpected->{$bFull}{$bDiff}{$bIncr}{$bAnchor});
            }
        }
        }
        }
        }
    }

    ################################################################################################################################
    if ($self->begin('backupLabelFormat()'))
    {
        my $strBackupLabelFull = timestampFileFormat(undef, 1482000000) . 'F';
        $self->testResult(sub {backupLabelFormat(CFGOPTVAL_BACKUP_TYPE_FULL, undef, 1482000000)}, $strBackupLabelFull,
        'full backup label');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testException(
            sub {backupLabelFormat(CFGOPTVAL_BACKUP_TYPE_FULL, $strBackupLabelFull, 1482000000)},
            ERROR_ASSERT, "strBackupLabelLast must not be defined when strType = 'full'");

        #---------------------------------------------------------------------------------------------------------------------------
        my $strBackupLabelDiff = "${strBackupLabelFull}_" . timestampFileFormat(undef, 1482000400) . 'D';
        $self->testResult(
            sub {backupLabelFormat(CFGOPTVAL_BACKUP_TYPE_DIFF, $strBackupLabelFull, 1482000400)}, $strBackupLabelDiff,
            'diff backup label');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testException(
            sub {backupLabelFormat(CFGOPTVAL_BACKUP_TYPE_DIFF, undef, 1482000400)},
            ERROR_ASSERT, "strBackupLabelLast must be defined when strType = 'diff'");

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {backupLabelFormat(CFGOPTVAL_BACKUP_TYPE_INCR, $strBackupLabelDiff, 1482000800)},
            "${strBackupLabelFull}_" . timestampFileFormat(undef, 1482000800) . 'I',
            'incremental backup label');
    }

    ################################################################################################################################
    if ($self->begin('backupLabel()'))
    {
        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_REPO_PATH, $self->testPath() . '/repo');
        $self->configTestLoad(CFGCMD_ARCHIVE_PUSH);

        #---------------------------------------------------------------------------------------------------------------------------
        my $lTime = time();

        my $strFullLabel = backupLabelFormat(CFGOPTVAL_BACKUP_TYPE_FULL, undef, $lTime);
        storageRepo()->pathCreate(STORAGE_REPO_BACKUP . "/${strFullLabel}", {bCreateParent => true});

        my $strNewFullLabel = backupLabel(storageRepo(), CFGOPTVAL_BACKUP_TYPE_FULL, undef, $lTime);

        $self->testResult(sub {$strFullLabel ne $strNewFullLabel}, true, 'new full label <> existing full backup dir');

        #---------------------------------------------------------------------------------------------------------------------------
        executeTest('rmdir ' . storageRepo()->pathGet(STORAGE_REPO_BACKUP . "/${strFullLabel}"));

        storageRepo()->pathCreate(
            STORAGE_REPO_BACKUP . qw(/) . PATH_BACKUP_HISTORY . '/' . timestampFormat('%4d', $lTime), {bCreateParent => true});
        storageRepo()->put(
            STORAGE_REPO_BACKUP . qw{/} . PATH_BACKUP_HISTORY . '/' . timestampFormat('%4d', $lTime) .
                "/${strFullLabel}.manifest." . COMPRESS_EXT);

        $strNewFullLabel = backupLabel(storageRepo(), CFGOPTVAL_BACKUP_TYPE_FULL, undef, $lTime);

        $self->testResult(sub {$strFullLabel ne $strNewFullLabel}, true, 'new full label <> existing full history file');

        #---------------------------------------------------------------------------------------------------------------------------
        $lTime = time() + 1000;
        $strFullLabel = backupLabelFormat(CFGOPTVAL_BACKUP_TYPE_FULL, undef, $lTime);

        $strNewFullLabel = backupLabel(storageRepo(), CFGOPTVAL_BACKUP_TYPE_FULL, undef, $lTime);

        $self->testResult(sub {$strFullLabel eq $strNewFullLabel}, true, 'new full label in future');

        #---------------------------------------------------------------------------------------------------------------------------
        $lTime = time();

        $strFullLabel = backupLabelFormat(CFGOPTVAL_BACKUP_TYPE_FULL, undef, $lTime);
        my $strDiffLabel = backupLabelFormat(CFGOPTVAL_BACKUP_TYPE_DIFF, $strFullLabel, $lTime);
        storageRepo()->pathCreate(STORAGE_REPO_BACKUP . "/${strDiffLabel}", {bCreateParent => true});

        my $strNewDiffLabel = backupLabel(storageRepo(), CFGOPTVAL_BACKUP_TYPE_DIFF, $strFullLabel, $lTime);

        $self->testResult(sub {$strDiffLabel ne $strNewDiffLabel}, true, 'new diff label <> existing diff backup dir');

        #---------------------------------------------------------------------------------------------------------------------------
        executeTest('rmdir ' . storageRepo()->pathGet(STORAGE_REPO_BACKUP . "/${strDiffLabel}"));

        storageRepo()->pathCreate(
            STORAGE_REPO_BACKUP . qw(/) .  PATH_BACKUP_HISTORY . '/' . timestampFormat('%4d', $lTime),
            {bIgnoreExists => true, bCreateParent => true});
        storageRepo()->put(
            STORAGE_REPO_BACKUP . qw{/} . PATH_BACKUP_HISTORY . '/' . timestampFormat('%4d', $lTime) .
                "/${strDiffLabel}.manifest." . COMPRESS_EXT);

        $strNewDiffLabel = backupLabel(storageRepo(), CFGOPTVAL_BACKUP_TYPE_DIFF, $strFullLabel, $lTime);

        $self->testResult(sub {$strDiffLabel ne $strNewDiffLabel}, true, 'new full label <> existing diff history file');

        #---------------------------------------------------------------------------------------------------------------------------
        $lTime = time() + 1000;
        $strDiffLabel = backupLabelFormat(CFGOPTVAL_BACKUP_TYPE_DIFF, $strFullLabel, $lTime);

        $strNewDiffLabel = backupLabel(storageRepo(), CFGOPTVAL_BACKUP_TYPE_DIFF, $strFullLabel, $lTime);

        $self->testResult(sub {$strDiffLabel eq $strNewDiffLabel}, true, 'new diff label in future');
    }

    ################################################################################################################################
    if ($self->begin('resumeClean()'))
    {
        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_REPO_PATH, $self->testPath() . '/repo');
        $self->optionTestSet(CFGOPT_PG_PATH, $self->testPath() . '/db');
        $self->configTestLoad(CFGCMD_BACKUP);

        my $lTime = time();

        my $strFullLabel = backupLabelFormat(CFGOPTVAL_BACKUP_TYPE_FULL, undef, $lTime);
        storageRepo()->pathCreate(STORAGE_REPO_BACKUP . "/${strFullLabel}", {bCreateParent => true});
        my $strBackupPath = storageRepo()->pathGet(STORAGE_REPO_BACKUP . "/${strFullLabel}");
        my $strBackupManifestFile = "$strBackupPath/" . FILE_MANIFEST;

        my $strPath = "path";
        my $strSubPath = "$strBackupPath/$strPath";
        my $strInManifestNoChecksum = 'in_manifest_no_checksum';
        my $strInManifestWithChecksum = 'in_manifest_with_checksum';
        my $strInManifestWithReference = 'in_manifest_with_reference';

        my $strExpectedManifest = $self->testPath() . '/expected.manifest';
        my $strAbortedManifest = $self->testPath() . '/aborted.manifest';
        my $oManifest = new pgBackRest::Manifest(
            $strBackupManifestFile,
            {bLoad => false, strDbVersion => PG_VERSION_94, iDbCatalogVersion => $self->dbCatalogVersion(PG_VERSION_94)});
        my $oAbortedManifest = new pgBackRest::Manifest(
            $strBackupManifestFile,
            {bLoad => false, strDbVersion => PG_VERSION_94, iDbCatalogVersion => $self->dbCatalogVersion(PG_VERSION_94)});
        my $oBackup = new pgBackRest::Backup::Backup();

        $oAbortedManifest->boolSet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_ONLINE, undef, false);

        # Compression prior enabled, gzip file exists and not in manifest, dir exists and is in manifest, delta not enabled
        #---------------------------------------------------------------------------------------------------------------------------
        $oAbortedManifest->boolSet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_COMPRESS, undef, true);
        storageRepo()->put(storageRepo()->openWrite($strBackupPath . '/' . BOGUS . '.gz',
            {strMode => '0750', strUser => TEST_USER, strGroup => TEST_GROUP, lTimestamp => $lTime}));
        storageRepo()->pathCreate($strSubPath, {bIgnoreExists => true});

        my $hDefault = {};
        $oManifest->set(MANIFEST_SECTION_TARGET_PATH, $strPath, undef, $hDefault);

        $self->testResult(sub {$oBackup->resumeClean(storageRepo(), $strFullLabel, $oManifest, $oAbortedManifest, false, false,
            undef, undef)}, false, 'resumeClean, prior compression enabled, delta not enabled');
        $self->testResult(sub {!storageRepo()->exists($strBackupPath . '/' . BOGUS . '.gz')}, true, '    gzip file removed');
        $self->testResult(sub {storageRepo()->pathExists($strSubPath)}, true, '    path not removed');

        # Disable compression
        $oAbortedManifest->boolSet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_COMPRESS, undef, false);
        $oManifest->remove(MANIFEST_SECTION_TARGET_PATH, $strPath);

        # Path and files to be removed (not in oManifest)
        #---------------------------------------------------------------------------------------------------------------------------
        storageRepo()->put(storageRepo()->openWrite($strBackupPath . "/" . FILE_MANIFEST_COPY,
            {strMode => '0750', strUser => TEST_USER, strGroup => TEST_GROUP, lTimestamp => $lTime}));
        storageRepo()->put(storageRepo()->openWrite($strSubPath . "/" . BOGUS,
            {strMode => '0750', strUser => TEST_USER, strGroup => TEST_GROUP, lTimestamp => $lTime}));
        storageRepo()->put(storageRepo()->openWrite($strBackupPath . "/" . BOGUS,
            {strMode => '0750', strUser => TEST_USER, strGroup => TEST_GROUP, lTimestamp => $lTime}));

        $self->testResult(sub {storageRepo()->pathExists($strSubPath) && storageRepo()->exists($strSubPath . "/" . BOGUS) &&
            storageRepo()->exists($strBackupPath . "/" . BOGUS)},
            true, 'dir and files to be removed exist');
        $self->testResult(sub {$oBackup->resumeClean(storageRepo(), $strFullLabel, $oManifest, $oAbortedManifest, false, true,
            undef, undef)}, true, 'resumeClean, delta enabled, path and files to remove, manifest copy to remain');
        $self->testResult(sub {!storageRepo()->pathExists($strSubPath) && !storageRepo()->exists($strSubPath . "/" . BOGUS)},
            true, '    dir removed');
        $self->testResult(sub {!storageRepo()->exists($strBackupPath . "/" . BOGUS) &&
            storageRepo()->exists($strBackupPath . "/" . FILE_MANIFEST_COPY)}, true,
            '    file removed, manifest copy remains');

        # Online changed, delta enabled
        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(sub {$oBackup->resumeClean(storageRepo(), $strFullLabel, $oManifest, $oAbortedManifest, true, false,
            undef, undef)}, true, 'resumeClean, online changed, delta enabled');

        # Online does not change, only backup.manifest.copy exists, delta not enabled
        #---------------------------------------------------------------------------------------------------------------------------
        storageRepo()->put(storageRepo()->openWrite($strBackupPath . '/' . BOGUS,
            {strMode => '0750', strUser => TEST_USER, strGroup => TEST_GROUP, lTimestamp => $lTime}));
        storageRepo()->put(storageRepo()->openWrite($strBackupPath . "/$strInManifestWithReference",
            {strMode => '0750', strUser => TEST_USER, strGroup => TEST_GROUP, lTimestamp => $lTime}));
        storageRepo()->put(storageRepo()->openWrite($strBackupPath . "/$strInManifestNoChecksum",
            {strMode => '0750', strUser => TEST_USER, strGroup => TEST_GROUP, lTimestamp => $lTime}));
        storageRepo()->put(storageRepo()->openWrite($strBackupPath . "/$strInManifestWithChecksum",
            {strMode => '0750', strUser => TEST_USER, strGroup => TEST_GROUP, lTimestamp => $lTime}), 'test');
        my ($strHash, $iSize) = storageRepo()->hashSize(storageRepo()->openRead($strBackupPath . "/$strInManifestWithChecksum"));

        $oManifest->numericSet(MANIFEST_SECTION_TARGET_FILE, $strInManifestNoChecksum,
            MANIFEST_SUBKEY_SIZE, 0);
        $oManifest->numericSet(MANIFEST_SECTION_TARGET_FILE, $strInManifestNoChecksum,
            MANIFEST_SUBKEY_TIMESTAMP, $lTime);
        $oManifest->numericSet(MANIFEST_SECTION_TARGET_FILE, $strInManifestWithChecksum,
            MANIFEST_SUBKEY_SIZE, $iSize);
        $oManifest->numericSet(MANIFEST_SECTION_TARGET_FILE, $strInManifestWithChecksum,
            MANIFEST_SUBKEY_TIMESTAMP, $lTime);
        $oManifest->numericSet(MANIFEST_SECTION_TARGET_FILE, $strInManifestWithReference,
            MANIFEST_SUBKEY_SIZE, 0);
        $oManifest->numericSet(MANIFEST_SECTION_TARGET_FILE, $strInManifestWithReference,
            MANIFEST_SUBKEY_TIMESTAMP, $lTime);
        $oManifest->set(MANIFEST_SECTION_TARGET_FILE, $strInManifestWithReference,
            MANIFEST_SUBKEY_REFERENCE, BOGUS);

        $oAbortedManifest->numericSet(MANIFEST_SECTION_TARGET_FILE, $strInManifestNoChecksum,
            MANIFEST_SUBKEY_SIZE, 0);
        $oAbortedManifest->numericSet(MANIFEST_SECTION_TARGET_FILE, $strInManifestNoChecksum,
            MANIFEST_SUBKEY_TIMESTAMP, $lTime);
        $oAbortedManifest->numericSet(MANIFEST_SECTION_TARGET_FILE, $strInManifestWithChecksum,
            MANIFEST_SUBKEY_SIZE, $iSize);
        $oAbortedManifest->numericSet(MANIFEST_SECTION_TARGET_FILE, $strInManifestWithChecksum,
            MANIFEST_SUBKEY_TIMESTAMP, $lTime);
        $oAbortedManifest->set(MANIFEST_SECTION_TARGET_FILE, $strInManifestWithChecksum,
            MANIFEST_SUBKEY_CHECKSUM, $strHash);
        $oManifest->numericSet(MANIFEST_SECTION_TARGET_FILE, $strInManifestWithReference,
            MANIFEST_SUBKEY_SIZE, 0);
        $oManifest->numericSet(MANIFEST_SECTION_TARGET_FILE, $strInManifestWithReference,
            MANIFEST_SUBKEY_TIMESTAMP, $lTime);
        $oManifest->set(MANIFEST_SECTION_TARGET_FILE, $strInManifestWithReference,
            MANIFEST_SUBKEY_REFERENCE, BOGUS);

        $self->testResult(sub {$oBackup->resumeClean(storageRepo(), $strFullLabel, $oManifest, $oAbortedManifest, false, false,
            undef, undef)}, false, 'resumeClean, online not changed, delta not enabled');
        $self->testResult(sub {!storageRepo()->exists($strBackupPath . "/" . BOGUS) &&
            !storageRepo()->exists($strBackupPath .  "/$strInManifestNoChecksum") &&
            !storageRepo()->exists($strBackupPath .  "/$strInManifestWithReference") &&
            storageRepo()->exists($strBackupPath .  "/$strInManifestWithChecksum") &&
            storageRepo()->exists($strBackupPath . "/" . FILE_MANIFEST_COPY)}, true,
            '    file not in manifest or in manifest but no-checksum removed, file in manifest and manifest.copy remains');
        $self->testResult(sub {$oManifest->test(MANIFEST_SECTION_TARGET_FILE, $strInManifestWithChecksum, MANIFEST_SUBKEY_CHECKSUM,
            $strHash)}, true, '    checksum copied to manifest');

        # Timestamp in the past for same-sized file with checksum.
        #---------------------------------------------------------------------------------------------------------------------------
        $oManifest->remove(MANIFEST_SECTION_TARGET_FILE, $strInManifestWithChecksum, MANIFEST_SUBKEY_CHECKSUM);
        $self->testResult(sub {$oManifest->test(MANIFEST_SECTION_TARGET_FILE, $strInManifestWithChecksum,
            MANIFEST_SUBKEY_CHECKSUM)}, false, 'manifest checksum does not exist');

        # Set the timestamp so that the new manifest appears to have a time in the past. This should enable delta.
        $oAbortedManifest->numericSet(MANIFEST_SECTION_TARGET_FILE, $strInManifestWithChecksum,
            MANIFEST_SUBKEY_TIMESTAMP, $lTime + 100);

        # Set checksum page for code coverage
        $oAbortedManifest->set(MANIFEST_SECTION_TARGET_FILE, $strInManifestWithChecksum, MANIFEST_SUBKEY_CHECKSUM_PAGE, false);

        $self->testResult(sub {$oBackup->resumeClean(storageRepo(), $strFullLabel, $oManifest, $oAbortedManifest, false, false,
            undef, undef)}, true, '    resumeClean, timestamp in past, delta enabled');
        $self->testResult(sub {$oManifest->test(MANIFEST_SECTION_TARGET_FILE, $strInManifestWithChecksum, MANIFEST_SUBKEY_CHECKSUM,
            $strHash) && $oManifest->boolTest(MANIFEST_SECTION_TARGET_FILE, $strInManifestWithChecksum,
            MANIFEST_SUBKEY_CHECKSUM_PAGE, false)}, true, '    checksum copied to manifest');

        # Timestamp different for same-sized file with checksum.
        #---------------------------------------------------------------------------------------------------------------------------
        $oManifest->remove(MANIFEST_SECTION_TARGET_FILE, $strInManifestWithChecksum, MANIFEST_SUBKEY_CHECKSUM);
        $oAbortedManifest->numericSet(MANIFEST_SECTION_TARGET_FILE, $strInManifestWithChecksum,
            MANIFEST_SUBKEY_TIMESTAMP, $lTime - 100);

        $self->testResult(sub {$oBackup->resumeClean(storageRepo(), $strFullLabel, $oManifest, $oAbortedManifest, false, false,
            undef, undef)}, false, 'resumeClean, timestamp different but size the same, delta not enabled');
        $self->testResult(sub {$oManifest->test(MANIFEST_SECTION_TARGET_FILE, $strInManifestWithChecksum,
            MANIFEST_SUBKEY_CHECKSUM) && !storageRepo()->exists($strBackupPath .  "/$strInManifestWithChecksum")},
            false, '    checksum not copied to manifest, file removed');

        # Size different, timestamp same for file with checksum.
        #---------------------------------------------------------------------------------------------------------------------------
        storageRepo()->put(storageRepo()->openWrite($strBackupPath . "/$strInManifestWithChecksum",
            {strMode => '0750', strUser => TEST_USER, strGroup => TEST_GROUP, lTimestamp => $lTime}), 'test');

        $oAbortedManifest->numericSet(MANIFEST_SECTION_TARGET_FILE, $strInManifestWithChecksum,
            MANIFEST_SUBKEY_SIZE, $iSize - 1);
        $oAbortedManifest->numericSet(MANIFEST_SECTION_TARGET_FILE, $strInManifestWithChecksum,
            MANIFEST_SUBKEY_TIMESTAMP, $lTime);

        $self->testResult(sub {$oBackup->resumeClean(storageRepo(), $strFullLabel, $oManifest, $oAbortedManifest, false, false,
            undef, undef)}, true, 'resumeClean, size different, timestamp same, delta enabled');
        $self->testResult(sub {$oManifest->test(MANIFEST_SECTION_TARGET_FILE, $strInManifestWithChecksum,
            MANIFEST_SUBKEY_CHECKSUM) && !storageRepo()->exists($strBackupPath .  "/$strInManifestWithChecksum")},
            false, '    checksum not copied to manifest, file removed');

        # Checksum page error and link to file.
        #---------------------------------------------------------------------------------------------------------------------------
        storageRepo()->put(storageRepo()->openWrite($strBackupPath . "/$strInManifestWithChecksum",
            {strMode => '0750', strUser => TEST_USER, strGroup => TEST_GROUP, lTimestamp => $lTime}), 'test');
        testLinkCreate($strBackupPath . "/testlink", $strBackupPath . "/$strInManifestWithChecksum");
        $self->testResult(sub {storageRepo()->exists($strBackupPath . "/testlink")}, true, 'link exists');

        $oAbortedManifest->numericSet(MANIFEST_SECTION_TARGET_FILE, $strInManifestWithChecksum,
            MANIFEST_SUBKEY_SIZE, $iSize);
        $oAbortedManifest->numericSet(MANIFEST_SECTION_TARGET_FILE, $strInManifestWithChecksum,
            MANIFEST_SUBKEY_TIMESTAMP, $lTime);

        # Set checksum page for code coverage
        $oAbortedManifest->boolSet(MANIFEST_SECTION_TARGET_FILE, $strInManifestWithChecksum, MANIFEST_SUBKEY_CHECKSUM_PAGE, false);
        $oAbortedManifest->set(MANIFEST_SECTION_TARGET_FILE, $strInManifestWithChecksum, MANIFEST_SUBKEY_CHECKSUM_PAGE_ERROR, 'E');

        $self->testResult(sub {$oBackup->resumeClean(storageRepo(), $strFullLabel, $oManifest, $oAbortedManifest, false, false,
            undef, undef)}, false, '    resumeClean, delta not enabled');

        $self->testResult(sub {$oManifest->test(MANIFEST_SECTION_TARGET_FILE, $strInManifestWithChecksum,
            MANIFEST_SUBKEY_CHECKSUM_PAGE_ERROR, 'E') && !storageRepo()->exists($strBackupPath .  "/testlink")},
            true, '    checksum page error copied to manifest, link removed');

        # Checksum page=true
        #---------------------------------------------------------------------------------------------------------------------------
        $oAbortedManifest->boolSet(MANIFEST_SECTION_TARGET_FILE, $strInManifestWithChecksum, MANIFEST_SUBKEY_CHECKSUM_PAGE, true);

        $self->testResult(sub {$oBackup->resumeClean(storageRepo(), $strFullLabel, $oManifest, $oAbortedManifest, false, false,
            undef, undef)}, false, '    resumeClean, checksum page = true');

        $self->testResult(sub {$oManifest->boolTest(MANIFEST_SECTION_TARGET_FILE, $strInManifestWithChecksum,
            MANIFEST_SUBKEY_CHECKSUM_PAGE, true)}, true, '    checksum page set true in manifest');
    }
}

1;
