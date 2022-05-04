####################################################################################################################################
# ExpireCommonTest.pm - Common code for expire tests
####################################################################################################################################
package pgBackRestTest::Env::ExpireEnvTest;
use parent 'pgBackRestTest::Env::HostEnvTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Fcntl qw(O_RDONLY);
use File::Basename qw(basename);

use pgBackRestDoc::Common::Exception;
use pgBackRestDoc::Common::Ini;
use pgBackRestDoc::Common::Log;
use pgBackRestDoc::ProjectInfo;

use pgBackRestTest::Common::DbVersion;
use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::FileTest;
use pgBackRestTest::Common::RunTest;
use pgBackRestTest::Common::StorageRepo;
use pgBackRestTest::Env::ArchiveInfo;
use pgBackRestTest::Env::BackupInfo;
use pgBackRestTest::Env::HostEnvTest;
use pgBackRestTest::Env::Host::HostBackupTest;
use pgBackRestTest::Env::Host::HostBaseTest;
use pgBackRestTest::Env::Manifest;

####################################################################################################################################
# new
####################################################################################################################################
sub new
{
    my $class = shift;          # Class name

    # Create the class hash
    my $self = {};
    bless $self, $class;

    # Assign function parameters, defaults, and log debug info
    (
        my $strOperation,
        $self->{oHostBackup},
        $self->{strBackRestExe},
        $self->{oStorageRepo},
        $self->{strPgPath},
        $self->{oLogTest},
        $self->{oRunTest},
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'oHostBackup', required => false, trace => true},
            {name => 'strBackRestExe', trace => true},
            {name => 'oStorageRepo', trace => true},
            {name => 'strPgPath', trace => true},
            {name => 'oLogTest', required => false, trace => true},
            {name => 'oRunTest', required => false, trace => true},
        );

    $self->{strVm} = $self->{oRunTest}->vm();

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# get into from pg_control
####################################################################################################################################
my $oPgControlVersionHash =
{
    # iControlVersion => {iCatalogVersion => strDbVersion}
    903 =>
    {
        201008051 => PG_VERSION_90,
        201105231 => PG_VERSION_91,
    },
    922 => {201204301 => PG_VERSION_92},
    937 => {201306121 => PG_VERSION_93},
    942 =>
    {
        201409291 => PG_VERSION_94,
        201510051 => PG_VERSION_95,
    },
    960 =>
    {
        201608131 => PG_VERSION_96,
    },
    1002 =>
    {
        201707211 => PG_VERSION_10,
    },
    1100 =>
    {
        201809051 => PG_VERSION_11,
    },
    1201 =>
    {
        201909212 => PG_VERSION_12,
    },
    1300 =>
    {
        202007201 => PG_VERSION_13,
    },
};

sub info
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strDbPath
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->info', \@_,
            {name => 'strDbPath', default => $self->{strPgPath}}
        );

    # Open the control file and read system id and versions
    #-----------------------------------------------------------------------------------------------------------------------
    my $strControlFile = "${strDbPath}/" . DB_FILE_PGCONTROL;
    my $hFile;
    my $tBlock;

    sysopen($hFile, $strControlFile, O_RDONLY)
        or confess &log(ERROR, "unable to open ${strControlFile}", ERROR_FILE_OPEN);

    # Read system identifier
    sysread($hFile, $tBlock, 8) == 8
        or confess &log(ERROR, "unable to read database system identifier");

    $self->{info}{$strDbPath}{ullDbSysId} = unpack('Q', $tBlock);

    # Read control version
    sysread($hFile, $tBlock, 4) == 4
        or confess &log(ERROR, "unable to read control version");

    $self->{info}{$strDbPath}{iDbControlVersion} = unpack('L', $tBlock);

    # Read catalog version
    sysread($hFile, $tBlock, 4) == 4
        or confess &log(ERROR, "unable to read catalog version");

    $self->{info}{$strDbPath}{iDbCatalogVersion} = unpack('L', $tBlock);

    # Close the control file
    close($hFile);

    # Get PostgreSQL version
    $self->{info}{$strDbPath}{strDbVersion} =
        $oPgControlVersionHash->{$self->{info}{$strDbPath}{iDbControlVersion}}
            {$self->{info}{$strDbPath}{iDbCatalogVersion}};

    if (!defined($self->{info}{$strDbPath}{strDbVersion}))
    {
        confess &log(
            ERROR,
            'unexpected control version = ' . $self->{info}{$strDbPath}{iDbControlVersion} .
            ' and catalog version = ' . $self->{info}{$strDbPath}{iDbCatalogVersion} . "\n" .
            'HINT: is this version of PostgreSQL supported?');
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strDbVersion', value => $self->{info}{$strDbPath}{strDbVersion}},
        {name => 'iDbControlVersion', value => $self->{info}{$strDbPath}{iDbControlVersion}},
        {name => 'iDbCatalogVersion', value => $self->{info}{$strDbPath}{iDbCatalogVersion}},
        {name => 'ullDbSysId', value => $self->{info}{$strDbPath}{ullDbSysId}}
    );
}

####################################################################################################################################
# stanzaSet - set the local stanza object
####################################################################################################################################
sub stanzaSet
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strStanza,
        $strDbVersion,
        $bStanzaUpgrade,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->stanzaSet', \@_,
            {name => 'strStanza'},
            {name => 'strDbVersion'},
            {name => 'bStanzaUpgrade'},
        );

    # Assign variables
    my $oStanza = {};
    my $oArchiveInfo = {};
    my $oBackupInfo = {};
    my $iArchiveDbId = 1;
    my $iBackupDbId = 1;

    # If we're not upgrading, then create the info files
    if (!$bStanzaUpgrade)
    {
        $oArchiveInfo =
            new pgBackRestTest::Env::ArchiveInfo($self->{oHostBackup}->repoArchivePath(), false,
            {bIgnoreMissing => true, strCipherPassSub => $self->{oHostBackup}->repoEncrypt() ? ENCRYPTION_KEY_ARCHIVE : undef});
        $oBackupInfo =
            new pgBackRestTest::Env::BackupInfo($self->{oHostBackup}->repoBackupPath(), false,
            {bIgnoreMissing => true, strCipherPassSub => $self->{oHostBackup}->repoEncrypt() ? ENCRYPTION_KEY_MANIFEST : undef});
    }
    # Else get the info data from disk
    else
    {
        $oArchiveInfo =
            new pgBackRestTest::Env::ArchiveInfo($self->{oHostBackup}->repoArchivePath(),
            {strCipherPassSub => $self->{oHostBackup}->repoEncrypt() ? ENCRYPTION_KEY_ARCHIVE : undef});
        $oBackupInfo =
            new pgBackRestTest::Env::BackupInfo($self->{oHostBackup}->repoBackupPath(),
            {strCipherPassSub => $self->{oHostBackup}->repoEncrypt() ? ENCRYPTION_KEY_MANIFEST : undef});
    }

    # Get the database info for the stanza
    (my $strVersion, $$oStanza{iControlVersion}, $$oStanza{iCatalogVersion}, $$oStanza{ullDbSysId}) = $self->info();
    $$oStanza{strDbVersion} = $strDbVersion;

    if ($bStanzaUpgrade)
    {
        $iArchiveDbId = $oArchiveInfo->dbHistoryIdGet() + 1;
        $iBackupDbId = $oBackupInfo->dbHistoryIdGet() + 1;
    }

    $oArchiveInfo->dbSectionSet($$oStanza{strDbVersion}, $$oStanza{ullDbSysId}, $iArchiveDbId);
    $oArchiveInfo->save();

    $oBackupInfo->dbSectionSet($$oStanza{strDbVersion}, $$oStanza{iControlVersion}, $$oStanza{iCatalogVersion},
        $$oStanza{ullDbSysId}, $iBackupDbId);
    $oBackupInfo->save();

    # Get the archive and directory paths for the stanza
    $$oStanza{strArchiveClusterPath} = $self->{oHostBackup}->repoArchivePath($oArchiveInfo->archiveId());
    $$oStanza{strBackupClusterPath} = $self->{oHostBackup}->repoBackupPath();

    $self->{oStanzaHash}{$strStanza} = $oStanza;

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# stanzaCreate
####################################################################################################################################
sub stanzaCreate
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strStanza,
        $strDbVersion,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->stanzaCreate', \@_,
            {name => 'strStanza'},
            {name => 'strDbVersion'},
        );

    my $strDbVersionTemp = $strDbVersion;
    $strDbVersionTemp =~ s/\.//;

    # Create the test path for pg_control
    storageTest()->pathCreate(($self->{strPgPath} . '/' . DB_PATH_GLOBAL), {bIgnoreExists => true});

    # Generate pg_control for stanza-create
    $self->controlGenerate($self->{strPgPath}, $strDbVersion);
    executeTest('chmod 600 ' . $self->{strPgPath} . '/' . DB_FILE_PGCONTROL);

    # Create the stanza and set the local stanza object
    $self->stanzaSet($strStanza, $strDbVersion, false);

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# stanzaUpgrade
####################################################################################################################################
sub stanzaUpgrade
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strStanza,
        $strDbVersion,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->stanzaUpgrade', \@_,
            {name => 'strStanza'},
            {name => 'strDbVersion'},
        );

    my $strDbVersionTemp = $strDbVersion;
    $strDbVersionTemp =~ s/\.//;

    # Remove pg_control
    storageTest()->remove($self->{strPgPath} . '/' . DB_FILE_PGCONTROL);

    # Copy pg_control for stanza-upgrade
    $self->controlGenerate($self->{strPgPath}, $strDbVersion);
    executeTest('chmod 600 ' . $self->{strPgPath} . '/' . DB_FILE_PGCONTROL);

    $self->stanzaSet($strStanza, $strDbVersion, true);

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}
####################################################################################################################################
# backupCreate
####################################################################################################################################
sub backupCreate
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strStanza,
        $strType,
        $lTimestamp,
        $iArchiveBackupTotal,
        $iArchiveBetweenTotal
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->backupCreate', \@_,
            {name => 'strStanza'},
            {name => 'strType'},
            {name => 'lTimestamp'},
            {name => 'iArchiveBackupTotal', default => 3},
            {name => 'iArchiveBetweenTotal', default => 3}
        );

    my $oStanza = $self->{oStanzaHash}{$strStanza};

    my ($strArchiveStart, $strArchiveStop);

    if ($iArchiveBackupTotal != -1)
    {
        ($strArchiveStart, $strArchiveStop) = $self->archiveCreate($strStanza, $iArchiveBackupTotal);
    }

    # Create the manifest
    my $oLastManifest = $strType ne CFGOPTVAL_BACKUP_TYPE_FULL ? $$oStanza{oManifest} : undef;

    my $strBackupLabel =
        backupLabelFormat($strType,
                          defined($oLastManifest) ? $oLastManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_LABEL) : undef,
                          $lTimestamp);

    my $strBackupClusterSetPath = "$$oStanza{strBackupClusterPath}/${strBackupLabel}";

    &log(INFO, "create backup ${strBackupLabel}");

    # Get passphrase (returns undefined if repo not encrypted) to access the manifest
    my $strCipherPassManifest =
        (new pgBackRestTest::Env::BackupInfo($self->{oHostBackup}->repoBackupPath()))->cipherPassSub();
    my $strCipherPassBackupSet;

    # If repo is encrypted then get passphrase for accessing the backup files from the last manifest if it exists provide one
    if (defined($strCipherPassManifest))
    {
        $strCipherPassBackupSet = (defined($oLastManifest)) ? $oLastManifest->cipherPassSub() :
            ENCRYPTION_KEY_BACKUPSET;
    }

    my $strManifestFile = "$$oStanza{strBackupClusterPath}/${strBackupLabel}/" . FILE_MANIFEST;

    my $oManifest = new pgBackRestTest::Env::Manifest($strManifestFile, {bLoad => false, strDbVersion => PG_VERSION_93,
        iDbCatalogVersion => $self->dbCatalogVersion(PG_VERSION_93),
        strCipherPass => $strCipherPassManifest, strCipherPassSub => $strCipherPassBackupSet});

    # Store information about the backup into the backup section
    $oManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_LABEL, undef, $strBackupLabel);
    $oManifest->boolSet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_ARCHIVE_CHECK, undef, true);
    $oManifest->boolSet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_ARCHIVE_COPY, undef, false);
    $oManifest->boolSet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_BACKUP_STANDBY, undef, false);
    $oManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_ARCHIVE_START, undef, $strArchiveStart);
    $oManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_ARCHIVE_STOP, undef, $strArchiveStop);
    $oManifest->boolSet(MANIFEST_SECTION_BACKUP_OPTION, 'backup-bundle', undef, true);
    $oManifest->boolSet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_CHECKSUM_PAGE, undef, true);
    $oManifest->boolSet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_COMPRESS, undef, true);
    $oManifest->numericSet(INI_SECTION_BACKREST, INI_KEY_FORMAT, undef, REPOSITORY_FORMAT);
    $oManifest->boolSet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_HARDLINK, undef, false);
    $oManifest->boolSet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_ONLINE, undef, true);
    $oManifest->numericSet(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TIMESTAMP_START, undef, $lTimestamp);
    $oManifest->numericSet(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TIMESTAMP_STOP, undef, $lTimestamp);
    $oManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TYPE, undef, $strType);
    $oManifest->set(INI_SECTION_BACKREST, INI_KEY_VERSION, undef, PROJECT_VERSION);

    if ($strType ne CFGOPTVAL_BACKUP_TYPE_FULL)
    {
        if (!defined($oLastManifest))
        {
            confess &log(ERROR, "oLastManifest must be defined when strType = ${strType}");
        }

        # Set backup-prior
        if ($strType eq CFGOPTVAL_BACKUP_TYPE_INCR)
        {
            # If this is an incremental backup, then it is always based on the prior backup so use the label from the last backup
            $oManifest->set(
                MANIFEST_SECTION_BACKUP, MANIFEST_KEY_PRIOR, undef,
                $oLastManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_LABEL));
        }
        else
        {
            # If it is a differential then backup-prior must be set to the newest full backup so get the full backup label from
            # the prior label
            $oManifest->set(
                MANIFEST_SECTION_BACKUP, MANIFEST_KEY_PRIOR, undef,
                substr($oLastManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_LABEL), 0, 16));
        }
    }

    $oManifest->save();
    $$oStanza{oManifest} = $oManifest;

    # Add the backup to info
    my $oBackupInfo = new pgBackRestTest::Env::BackupInfo($$oStanza{strBackupClusterPath}, false);

    $oBackupInfo->check($$oStanza{strDbVersion}, $$oStanza{iControlVersion}, $$oStanza{iCatalogVersion}, $$oStanza{ullDbSysId});
    $oBackupInfo->add($oManifest);

    # Create the backup description string
    if (defined($$oStanza{strBackupDescription}))
    {
        $$oStanza{strBackupDescription} .= "\n";
    }

    $$oStanza{strBackupDescription} .=
        "* ${strType} backup: label = ${strBackupLabel}" .
        (defined($oLastManifest) ? ', prior = ' . $oLastManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_LABEL) : '') .
        (defined($strArchiveStart) ? ", start = ${strArchiveStart}, stop = ${strArchiveStop}" : ', not online');

    if ($iArchiveBetweenTotal != -1)
    {
        $self->archiveCreate($strStanza, $iArchiveBetweenTotal);
    }

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# archiveNext
####################################################################################################################################
sub archiveNext
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strArchive,
        $bSkipFF
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->archiveNext', \@_,
            {name => 'strArchive', trace => true},
            {name => 'bSkipFF', trace => true}
        );

    # Break archive log into components
    my $lTimeline = hex(substr($strArchive, 0, 8));
    my $lMajor = hex(substr($strArchive, 8, 8));
    my $lMinor = hex(substr($strArchive, 16, 8));

    # Increment the minor component (and major when needed)
    $lMinor += 1;

    if ($bSkipFF && $lMinor == 255 || !$bSkipFF && $lMinor == 256)
    {
        $lMajor += 1;
        $lMinor = 0;
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strArchiveNext', value => uc(sprintf("%08x%08x%08x", $lTimeline, $lMajor, $lMinor)), trace => true}
    );
}

####################################################################################################################################
# archiveCreate
####################################################################################################################################
sub archiveCreate
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strStanza,
        $iArchiveTotal
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->archiveCreate', \@_,
            {name => 'strStanza'},
            {name => 'iArchiveTotal'}
        );

    my $oStanza = $self->{oStanzaHash}{$strStanza};
    my $iArchiveIdx = 0;
    my $bSkipFF = $$oStanza{strDbVersion} <= PG_VERSION_92;

    my $strArchive = defined($$oStanza{strArchiveLast}) ? $self->archiveNext($$oStanza{strArchiveLast}, $bSkipFF) :
                                                          '000000010000000000000000';

    # Get passphrase (returns undefined if repo not encrypted) to access the archive files
    my $strCipherPass =
        (new pgBackRestTest::Env::ArchiveInfo($self->{oHostBackup}->repoArchivePath()))->cipherPassSub();

    push(my @stryArchive, $strArchive);

    do
    {
        my $strPath = "$$oStanza{strArchiveClusterPath}/" . substr($strArchive, 0, 16);
        my $strFile = "${strPath}/${strArchive}-0000000000000000000000000000000000000000" . ($iArchiveIdx % 2 == 0 ? '.gz' : '');

        storageRepo()->put($strFile, 'ARCHIVE', {strCipherPass => $strCipherPass});

        $iArchiveIdx++;

        if ($iArchiveIdx < $iArchiveTotal)
        {
            $strArchive = $self->archiveNext($strArchive, $bSkipFF);
        }
    }
    while ($iArchiveIdx < $iArchiveTotal);

    push(@stryArchive, $strArchive);
    $$oStanza{strArchiveLast} = $strArchive;

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'stryArchive', value => \@stryArchive}
    );
}

####################################################################################################################################
# supplementalLog
####################################################################################################################################
sub supplementalLog
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strStanza
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->supplementalLog', \@_,
            {name => 'strStanza'}
        );

    my $oStanza = $self->{oStanzaHash}{$strStanza};

    if (defined($self->{oLogTest}))
    {
        $self->{oLogTest}->supplementalAdd(
            $self->{oHostBackup}->repoPath() . "/backup/${strStanza}/backup.info", $$oStanza{strBackupDescription},
            ${storageRepo->get($self->{oHostBackup}->repoPath() . "/backup/${strStanza}/backup.info")});

        # Output backup list
        $self->{oLogTest}->logAdd(
            'ls ' . $self->{oHostBackup}->repoPath() . "/backup/${strStanza} | grep -v \"backup.*\"", undef,
            join("\n", grep(!/^backup\.info.*$/i, storageRepo()->list("backup/${strStanza}"))));

        # Output archive manifest
        my $rhManifest = storageRepo()->manifest($self->{oHostBackup}->repoArchivePath());
        my $strManifest;
        my $strPrefix = '';

        foreach my $strEntry (sort(keys(%{$rhManifest})))
        {
            # Skip files
            next if $strEntry eq ARCHIVE_INFO_FILE || $strEntry eq ARCHIVE_INFO_FILE . INI_COPY_EXT;

            if ($rhManifest->{$strEntry}->{type} eq 'd')
            {
                $strEntry = $self->{oHostBackup}->repoArchivePath($strEntry eq '.' ? undef : $strEntry);

                # &log(WARN, "DIR $strEntry");
                $strManifest .= (defined($strManifest) ? "\n" : '') . "${strEntry}:\n";
                $strPrefix = $strEntry;
            }
            else
            {
                # &log(WARN, "FILE $strEntry");
                $strManifest .= basename($strEntry) . "\n";
            }
        }

        $self->{oLogTest}->logAdd(
            'ls -R ' . $self->{oHostBackup}->repoPath() . "/archive/${strStanza} | grep -v \"archive.info\"", undef, $strManifest);
    }

    return logDebugReturn($strOperation);
}

####################################################################################################################################
# process
####################################################################################################################################
sub process
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strStanza,
        $iExpireFull,
        $iExpireDiff,
        $strExpireArchiveType,
        $iExpireArchive,
        $strDescription
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->process', \@_,
            {name => 'strStanza'},
            {name => 'iExpireFull', required => false},
            {name => 'iExpireDiff', required => false},
            {name => 'strExpireArchiveType'},
            {name => 'iExpireArchive', required => false},
            {name => 'strDescription'}
        );

    my $oStanza = $self->{oStanzaHash}{$strStanza};

    $self->supplementalLog($strStanza);

    undef($$oStanza{strBackupDescription});

    my $strCommand =
        $self->{strBackRestExe} . ' --config="' . $self->{oHostBackup}->backrestConfig() . '"' . ' --stanza=' . $strStanza .
        ' --log-level-console=' . lc(DETAIL);

    if (defined($iExpireFull))
    {
        $strCommand .= ' --repo1-retention-full=' . $iExpireFull;
    }

    if (defined($iExpireDiff))
    {
        $strCommand .= ' --repo1-retention-diff=' . $iExpireDiff;
    }

    if (defined($strExpireArchiveType))
    {
        if (defined($iExpireArchive))
        {
            $strCommand .= ' --repo1-retention-archive-type=' . $strExpireArchiveType .
                           ' --repo1-retention-archive=' . $iExpireArchive;
        }
        else
        {
            $strCommand .= ' --repo1-retention-archive-type=' . $strExpireArchiveType;
        }
    }

    $strCommand .= ' expire';

    $self->{oHostBackup}->executeSimple($strCommand, {strComment => $strDescription, oLogTest => $self->{oLogTest}});

    $self->supplementalLog($strStanza);

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

1;
