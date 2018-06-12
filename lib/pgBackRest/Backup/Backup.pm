####################################################################################################################################
# BACKUP MODULE
####################################################################################################################################
package pgBackRest::Backup::Backup;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Exporter qw(import);
use File::Basename;

use pgBackRest::Archive::Common;
use pgBackRest::Archive::Get::Get;
use pgBackRest::Backup::Common;
use pgBackRest::Backup::File;
use pgBackRest::Backup::Info;
use pgBackRest::Common::Cipher;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Ini;
use pgBackRest::Common::Log;
use pgBackRest::Common::Wait;
use pgBackRest::Common::String;
use pgBackRest::Config::Config;
use pgBackRest::Db;
use pgBackRest::DbVersion;
use pgBackRest::Manifest;
use pgBackRest::Protocol::Local::Process;
use pgBackRest::Protocol::Helper;
use pgBackRest::Protocol::Storage::Helper;
use pgBackRest::Common::Io::Handle;
use pgBackRest::Storage::Base;
use pgBackRest::Storage::Filter::Gzip;
use pgBackRest::Storage::Filter::Sha;
use pgBackRest::Storage::Helper;
use pgBackRest::Version;

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
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->new');

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# resumeClean - cleans the directory from a previous failed backup so it can be reused
####################################################################################################################################
sub resumeClean
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oStorageRepo,
        $strBackupLabel,
        $oManifest,
        $oAbortedManifest
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->resumeClean', \@_,
            {name => 'oStorageRepo'},
            {name => 'strBackupLabel'},
            {name => 'oManifest'},
            {name => 'oAbortedManifest'}
        );

    &log(DETAIL, 'clean resumed backup path: ' . $oStorageRepo->pathGet(STORAGE_REPO_BACKUP . "/${strBackupLabel}"));

    # Build manifest for aborted backup path
    my $hFile = $oStorageRepo->manifest(STORAGE_REPO_BACKUP . "/${strBackupLabel}");

    # Get compress flag
    my $bCompressed = $oAbortedManifest->boolGet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_COMPRESS);

    # Find paths and files to delete
    my @stryFile;

    foreach my $strName (sort(keys(%{$hFile})))
    {
        # Ignore files that will never be in the manifest but should be preserved
        if ($strName eq FILE_MANIFEST_COPY ||
            $strName eq '.')
        {
            next;
        }

        # Get the file type (all links will be deleted since they are easy to recreate)
        my $cType = $hFile->{$strName}{type};

        # If a directory check if it exists in the new manifest
        if ($cType eq 'd')
        {
            if ($oManifest->test(MANIFEST_SECTION_TARGET_PATH, $strName))
            {
                next;
            }
        }
        # Else if a file
        elsif ($cType eq 'f')
        {
            # If the original backup was compressed the remove the extension before checking the manifest
            my $strFile = $strName;

            if ($bCompressed)
            {
                $strFile = substr($strFile, 0, length($strFile) - 3);
            }

            # To be preserved the file must exist in the new manifest and not be a reference to a previous backup
            if ($oManifest->test(MANIFEST_SECTION_TARGET_FILE, $strFile) &&
                !$oManifest->test(MANIFEST_SECTION_TARGET_FILE, $strFile, MANIFEST_SUBKEY_REFERENCE))
            {
                # To be preserved the checksum must be defined
                my $strChecksum = $oAbortedManifest->get(MANIFEST_SECTION_TARGET_FILE, $strFile, MANIFEST_SUBKEY_CHECKSUM, false);

                if (defined($strChecksum))
                {
                    $oManifest->set(MANIFEST_SECTION_TARGET_FILE, $strFile, MANIFEST_SUBKEY_CHECKSUM, $strChecksum);

                    # Also copy page checksum results if they exist
                    my $bChecksumPage =
                        $oAbortedManifest->get(MANIFEST_SECTION_TARGET_FILE, $strFile, MANIFEST_SUBKEY_CHECKSUM_PAGE, false);

                    if (defined($bChecksumPage))
                    {
                        $oManifest->boolSet(MANIFEST_SECTION_TARGET_FILE, $strFile, MANIFEST_SUBKEY_CHECKSUM_PAGE, $bChecksumPage);

                        if (!$bChecksumPage &&
                            $oAbortedManifest->test(MANIFEST_SECTION_TARGET_FILE, $strFile, MANIFEST_SUBKEY_CHECKSUM_PAGE_ERROR))
                        {
                            $oManifest->set(
                                MANIFEST_SECTION_TARGET_FILE, $strFile, MANIFEST_SUBKEY_CHECKSUM_PAGE_ERROR,
                                $oAbortedManifest->get(
                                    MANIFEST_SECTION_TARGET_FILE, $strFile, MANIFEST_SUBKEY_CHECKSUM_PAGE_ERROR));
                        }
                    }

                    next;
                }
            }
        }

        # If a directory then remove it
        if ($cType eq 'd')
        {
            logDebugMisc($strOperation, "remove path ${strName}");
            $oStorageRepo->remove(STORAGE_REPO_BACKUP . "/${strBackupLabel}/${strName}", {bRecurse => true});
        }
        # Else add the file/link to be deleted later
        else
        {
            logDebugMisc($strOperation, "remove file ${strName}");
            push(@stryFile, STORAGE_REPO_BACKUP . "/${strBackupLabel}/${strName}");
        }
    }

    # Delete files in batch for more efficiency
    if (@stryFile > 0)
    {
        $oStorageRepo->remove(\@stryFile);
    }

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# processManifest
#
# Process the file level backup.  Uses the information in the manifest to determine which files need to be copied.  Directories
# and tablespace links are only created when needed, except in the case of a full backup or if hardlinks are requested.
####################################################################################################################################
sub processManifest
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strDbMasterPath,
        $strDbCopyPath,
        $strType,
        $strDbVersion,
        $bCompress,
        $bHardLink,
        $oBackupManifest,
        $strBackupLabel,
        $strLsnStart,
    ) =
        logDebugParam
    (
        __PACKAGE__ . '->processManifest', \@_,
        {name => 'strDbMasterPath'},
        {name => 'strDbCopyPath'},
        {name => 'strType'},
        {name => 'strDbVersion'},
        {name => 'bCompress'},
        {name => 'bHardLink'},
        {name => 'oBackupManifest'},
        {name => 'strBackupLabel'},
        {name => 'strLsnStart', required => false},
    );

    # Start backup test point
    &log(TEST, TEST_BACKUP_START);

    # Get the master protocol for keep-alive
    my $oProtocolMaster =
        !isDbLocal({iRemoteIdx => $self->{iMasterRemoteIdx}}) ?
            protocolGet(CFGOPTVAL_REMOTE_TYPE_DB, $self->{iMasterRemoteIdx}) : undef;
    defined($oProtocolMaster) && $oProtocolMaster->noOp();

    # Initialize the backup process
    my $oBackupProcess = new pgBackRest::Protocol::Local::Process(CFGOPTVAL_LOCAL_TYPE_DB);

    if ($self->{iCopyRemoteIdx} != $self->{iMasterRemoteIdx})
    {
        $oBackupProcess->hostAdd($self->{iMasterRemoteIdx}, 1);
    }

    $oBackupProcess->hostAdd($self->{iCopyRemoteIdx}, cfgOption(CFGOPT_PROCESS_MAX));

    # Variables used for parallel copy
    my $lFileTotal = 0;
    my $lSizeTotal = 0;

    # If this is a full backup or hard-linked then create all paths and tablespace links
    if ($bHardLink || $strType eq CFGOPTVAL_BACKUP_TYPE_FULL)
    {
        # Create paths
        foreach my $strPath ($oBackupManifest->keys(MANIFEST_SECTION_TARGET_PATH))
        {
            storageRepo()->pathCreate(STORAGE_REPO_BACKUP . "/${strBackupLabel}/${strPath}", {bIgnoreExists => true});
        }

        if (storageRepo()->driver()->capability(STORAGE_CAPABILITY_LINK))
        {
            for my $strTarget ($oBackupManifest->keys(MANIFEST_SECTION_BACKUP_TARGET))
            {
                if ($oBackupManifest->isTargetTablespace($strTarget))
                {
                    storageRepo()->linkCreate(
                        STORAGE_REPO_BACKUP . "/${strBackupLabel}/${strTarget}",
                        STORAGE_REPO_BACKUP . "/${strBackupLabel}/" . MANIFEST_TARGET_PGDATA . "/${strTarget}",
                        {bRelative => true});
                }
            }
        }
    }

    # Build the lsn start parameter to pass to the extra function
    my $hStartLsnParam =
    {
        iWalId => defined($strLsnStart) ? hex((split('/', $strLsnStart))[0]) : 0xFFFF,
        iWalOffset => defined($strLsnStart) ? hex((split('/', $strLsnStart))[1]) : 0xFFFF,
    };

    # Iterate all files in the manifest
    foreach my $strRepoFile (
        sort {sprintf("%016d-${b}", $oBackupManifest->numericGet(MANIFEST_SECTION_TARGET_FILE, $b, MANIFEST_SUBKEY_SIZE)) cmp
              sprintf("%016d-${a}", $oBackupManifest->numericGet(MANIFEST_SECTION_TARGET_FILE, $a, MANIFEST_SUBKEY_SIZE))}
        ($oBackupManifest->keys(MANIFEST_SECTION_TARGET_FILE, INI_SORT_NONE)))
    {
        # If the file has a reference it does not need to be copied since it can be retrieved from the referenced backup.
        # However, if hard-linking is turned on the link will need to be created
        my $strReference = $oBackupManifest->get(MANIFEST_SECTION_TARGET_FILE, $strRepoFile, MANIFEST_SUBKEY_REFERENCE, false);

        if (defined($strReference))
        {
            # If hardlinking is turned on then create a hardlink for files that have not changed since the last backup
            if ($bHardLink)
            {
                &log(DETAIL, "hardlink ${strRepoFile} to ${strReference}");

                storageRepo()->linkCreate(
                    STORAGE_REPO_BACKUP . "/${strReference}/${strRepoFile}" . ($bCompress ? qw{.} . COMPRESS_EXT : ''),
                    STORAGE_REPO_BACKUP . "/${strBackupLabel}/${strRepoFile}" . ($bCompress ? qw{.} . COMPRESS_EXT : ''),
                    {bHard => true});
            }
            # Else log the reference
            else
            {
                logDebugMisc($strOperation, "reference ${strRepoFile} to ${strReference}");
            }

            # This file will not need to be copied
            next;
        }

        # By default put everything into a single queue
        my $strQueueKey = MANIFEST_TARGET_PGDATA;

        # If the file belongs in a tablespace then put in a tablespace-specific queue
        if (index($strRepoFile, DB_PATH_PGTBLSPC . '/') == 0)
        {
            $strQueueKey = DB_PATH_PGTBLSPC . '/' . (split('\/', $strRepoFile))[1];
        }

        # Create the file hash
        my $bIgnoreMissing = true;
        my $strDbFile = $oBackupManifest->dbPathGet($strDbCopyPath, $strRepoFile);
        my $iHostConfigIdx = $self->{iCopyRemoteIdx};

        # Certain files must be copied from the master
        if ($oBackupManifest->boolGet(MANIFEST_SECTION_TARGET_FILE, $strRepoFile, MANIFEST_SUBKEY_MASTER))
        {
            $strDbFile = $oBackupManifest->dbPathGet($strDbMasterPath, $strRepoFile);
            $iHostConfigIdx = $self->{iMasterRemoteIdx};
        }

        # Make sure that pg_control is not removed during the backup
        if ($strRepoFile eq MANIFEST_TARGET_PGDATA . '/' . DB_FILE_PGCONTROL)
        {
            $bIgnoreMissing = false;
        }

        # Increment file total and size
        my $lSize = $oBackupManifest->numericGet(MANIFEST_SECTION_TARGET_FILE, $strRepoFile, MANIFEST_SUBKEY_SIZE);

        $lFileTotal++;
        $lSizeTotal += $lSize;

        # Queue for parallel backup
        $oBackupProcess->queueJob(
            $iHostConfigIdx, $strQueueKey, $strRepoFile, OP_BACKUP_FILE,
            [$strDbFile, $strRepoFile, $lSize,
                $oBackupManifest->get(MANIFEST_SECTION_TARGET_FILE, $strRepoFile, MANIFEST_SUBKEY_CHECKSUM, false),
                cfgOption(CFGOPT_CHECKSUM_PAGE) ? isChecksumPage($strRepoFile) : false, $strBackupLabel, $bCompress,
                cfgOption(CFGOPT_COMPRESS_LEVEL), $oBackupManifest->numericGet(MANIFEST_SECTION_TARGET_FILE, $strRepoFile,
                MANIFEST_SUBKEY_TIMESTAMP, false), $bIgnoreMissing,
                cfgOption(CFGOPT_CHECKSUM_PAGE) && isChecksumPage($strRepoFile) ? $hStartLsnParam : undef],
            {rParamSecure => $oBackupManifest->cipherPassSub() ? [$oBackupManifest->cipherPassSub()] : undef});

        # Size and checksum will be removed and then verified later as a sanity check
        $oBackupManifest->remove(MANIFEST_SECTION_TARGET_FILE, $strRepoFile, MANIFEST_SUBKEY_SIZE);
        $oBackupManifest->remove(MANIFEST_SECTION_TARGET_FILE, $strRepoFile, MANIFEST_SUBKEY_CHECKSUM);
    }

    # pg_control should always be in the backup (unless this is an offline backup)
    if (!$oBackupManifest->test(MANIFEST_SECTION_TARGET_FILE, MANIFEST_FILE_PGCONTROL) && cfgOption(CFGOPT_ONLINE))
    {
        confess &log(ERROR, DB_FILE_PGCONTROL . " must be present in all online backups\n" .
                     'HINT: is something wrong with the clock or filesystem timestamps?', ERROR_FILE_MISSING);
    }

    # If there are no files to backup then we'll exit with an error unless in test mode.  The other way this could happen is if
    # the database is down and backup is called with --no-online twice in a row.
    if ($lFileTotal == 0 && !cfgOption(CFGOPT_TEST))
    {
        confess &log(ERROR, "no files have changed since the last backup - this seems unlikely", ERROR_FILE_MISSING);
    }

    # Running total of bytes copied
    my $lSizeCurrent = 0;

    # Determine how often the manifest will be saved
    my $lManifestSaveCurrent = 0;
    my $lManifestSaveSize = int($lSizeTotal / 100);

    if (cfgOptionSource(CFGOPT_MANIFEST_SAVE_THRESHOLD) ne CFGDEF_SOURCE_DEFAULT ||
        $lManifestSaveSize < cfgOption(CFGOPT_MANIFEST_SAVE_THRESHOLD))
    {
        $lManifestSaveSize = cfgOption(CFGOPT_MANIFEST_SAVE_THRESHOLD);
    }

    # Run the backup jobs and process results
    while (my $hyJob = $oBackupProcess->process())
    {
        foreach my $hJob (@{$hyJob})
        {
            ($lSizeCurrent, $lManifestSaveCurrent) = backupManifestUpdate(
                $oBackupManifest, cfgOption(cfgOptionIdFromIndex(CFGOPT_PG_HOST, $hJob->{iHostConfigIdx}), false),
                $hJob->{iProcessId}, @{$hJob->{rParam}}[0..4], @{$hJob->{rResult}}, $lSizeTotal, $lSizeCurrent, $lManifestSaveSize,
                $lManifestSaveCurrent);
        }

        # A keep-alive is required here because if there are a large number of resumed files that need to be checksummed
        # then the remote might timeout while waiting for a command.
        protocolKeepAlive();
    }

    # Validate the manifest
    $oBackupManifest->validate();

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'lSizeTotal', value => $lSizeTotal}
    );
}

####################################################################################################################################
# process
#
# Process the database backup.
####################################################################################################################################
sub process
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->process');

    # Record timestamp start
    my $lTimestampStart = time();

    # Initialize the local file object
    my $oStorageRepo = storageRepo();

    # Store local type, compress, and hardlink options since they can be modified by the process
    my $strType = cfgOption(CFGOPT_TYPE);
    my $bCompress = cfgOption(CFGOPT_COMPRESS);
    my $bHardLink = cfgOption(CFGOPT_REPO_HARDLINK);

    # Load the backup.info
    my $oBackupInfo = new pgBackRest::Backup::Info($oStorageRepo->pathGet(STORAGE_REPO_BACKUP));

    # Get passphrase to open manifest (undefined if repo not encrypted) and intialize passphrase variable for backup files
    my $strCipherPassManifest = $oBackupInfo->cipherPassSub();
    my $strCipherPassBackupSet;

    # Initialize database objects
    my $oDbMaster = undef;
    my $oDbStandby = undef;

    # Get the database objects
    ($oDbMaster, $self->{iMasterRemoteIdx}, $oDbStandby, $self->{iCopyRemoteIdx}) = dbObjectGet();

    # If remote copy was not explicitly set then set it equal to master
    if (!defined($self->{iCopyRemoteIdx}))
    {
        $self->{iCopyRemoteIdx} = $self->{iMasterRemoteIdx};
    }

    # If backup from standby option is set but a standby was not configured in the config file or on the command line, then turn off
    # CFGOPT_BACKUP_STANDBY & warn that backups will be performed from the master.
    if (!defined($oDbStandby) && cfgOption(CFGOPT_BACKUP_STANDBY))
    {
        cfgOptionSet(CFGOPT_BACKUP_STANDBY, false);
        &log(WARN, 'option backup-standby is enabled but standby is not properly configured - ' .
            'backups will be performed from the master');
    }

    # Initialize the master file object
    my $oStorageDbMaster = storageDb({iRemoteIdx => $self->{iMasterRemoteIdx}});

    # Determine the database paths
    my $strDbMasterPath = cfgOption(cfgOptionIdFromIndex(CFGOPT_PG_PATH, $self->{iMasterRemoteIdx}));
    my $strDbCopyPath = cfgOption(cfgOptionIdFromIndex(CFGOPT_PG_PATH, $self->{iCopyRemoteIdx}));

    # Database info
    my ($strDbVersion, $iControlVersion, $iCatalogVersion, $ullDbSysId) = $oDbMaster->info();

    my $iDbHistoryId = $oBackupInfo->check($strDbVersion, $iControlVersion, $iCatalogVersion, $ullDbSysId);

    # Find the previous backup based on the type
    my $oLastManifest;
    my $strBackupLastPath;

    if ($strType ne CFGOPTVAL_BACKUP_TYPE_FULL)
    {
        $strBackupLastPath = $oBackupInfo->last(
            $strType eq CFGOPTVAL_BACKUP_TYPE_DIFF ? CFGOPTVAL_BACKUP_TYPE_FULL : CFGOPTVAL_BACKUP_TYPE_INCR);

        # If there is a prior backup and it is for the current database, then use it as base
        if (defined($strBackupLastPath) && $oBackupInfo->confirmDb($strBackupLastPath, $strDbVersion, $ullDbSysId))
        {
            $oLastManifest = new pgBackRest::Manifest(
                $oStorageRepo->pathGet(STORAGE_REPO_BACKUP . "/${strBackupLastPath}/" . FILE_MANIFEST),
                {strCipherPass => $strCipherPassManifest});

            # If the repo is encrypted then use the passphrase in this manifest for the backup set
            $strCipherPassBackupSet = $oLastManifest->cipherPassSub();

            &log(INFO, 'last backup label = ' . $oLastManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_LABEL) .
                       ', version = ' . $oLastManifest->get(INI_SECTION_BACKREST, INI_KEY_VERSION));

            # If this is incr or diff warn if certain options have changed
            my $strKey;

            # Warn if compress option changed
            if (!$oLastManifest->boolTest(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_COMPRESS, undef, $bCompress))
            {
                &log(WARN, "${strType} backup cannot alter compress option to '" . boolFormat($bCompress) .
                           "', reset to value in ${strBackupLastPath}");
                $bCompress = $oLastManifest->boolGet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_COMPRESS);
            }

            # Warn if hardlink option changed
            if (!$oLastManifest->boolTest(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_HARDLINK, undef, $bHardLink))
            {
                &log(WARN, "${strType} backup cannot alter hardlink option to '" . boolFormat($bHardLink) .
                           "', reset to value in ${strBackupLastPath}");
                $bHardLink = $oLastManifest->boolGet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_HARDLINK);
            }
        }
        else
        {
            &log(WARN, "no prior backup exists, ${strType} backup has been changed to full");
            $strType = CFGOPTVAL_BACKUP_TYPE_FULL;
            $strBackupLastPath = undef;
        }
    }

    # Search cluster directory for an aborted backup
    my $strBackupLabel;
    my $oAbortedManifest;
    my $strBackupPath;

    foreach my $strAbortedBackup ($oStorageRepo->list(
        STORAGE_REPO_BACKUP, {strExpression => backupRegExpGet(true, true, true), strSortOrder => 'reverse'}))
    {
        # Aborted backups have a copy of the manifest but no main
        if ($oStorageRepo->exists(STORAGE_REPO_BACKUP . "/${strAbortedBackup}/" . FILE_MANIFEST_COPY) &&
            !$oStorageRepo->exists(STORAGE_REPO_BACKUP . "/${strAbortedBackup}/" . FILE_MANIFEST))
        {
            my $bUsable;
            my $strReason = "resume is disabled";
            $strBackupPath = $oStorageRepo->pathGet(STORAGE_REPO_BACKUP . "/${strAbortedBackup}");

            # Attempt to read the manifest file in the aborted backup to see if it can be used.  If any error at all occurs then the
            # backup will be considered unusable and a resume will not be attempted.
            if (cfgOption(CFGOPT_RESUME))
            {
                $strReason = "unable to read ${strBackupPath}/" . FILE_MANIFEST;

                eval
                {
                    # Load the aborted manifest
                    $oAbortedManifest = new pgBackRest::Manifest("${strBackupPath}/" . FILE_MANIFEST,
                        {strCipherPass => $strCipherPassManifest});

                    # Key and values that do not match
                    my $strKey;
                    my $strValueNew;
                    my $strValueAborted;

                    # Check version
                    if ($oAbortedManifest->get(INI_SECTION_BACKREST, INI_KEY_VERSION) ne BACKREST_VERSION)
                    {
                        $strKey =  INI_KEY_VERSION;
                        $strValueNew = BACKREST_VERSION;
                        $strValueAborted = $oAbortedManifest->get(INI_SECTION_BACKREST, INI_KEY_VERSION);
                    }
                    # Check format
                    elsif ($oAbortedManifest->get(INI_SECTION_BACKREST, INI_KEY_FORMAT) ne BACKREST_FORMAT)
                    {
                        $strKey =  INI_KEY_FORMAT;
                        $strValueNew = BACKREST_FORMAT;
                        $strValueAborted = $oAbortedManifest->get(INI_SECTION_BACKREST, INI_KEY_FORMAT);
                    }
                    # Check backup type
                    elsif ($oAbortedManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TYPE) ne $strType)
                    {
                        $strKey =  MANIFEST_KEY_TYPE;
                        $strValueNew = $strType;
                        $strValueAborted = $oAbortedManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TYPE);
                    }
                    # Check prior label
                    elsif ($oAbortedManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_PRIOR, undef, false, '<undef>') ne
                           (defined($strBackupLastPath) ? $strBackupLastPath : '<undef>'))
                    {
                        $strKey =  MANIFEST_KEY_PRIOR;
                        $strValueNew = defined($strBackupLastPath) ? $strBackupLastPath : '<undef>';
                        $strValueAborted =
                            $oAbortedManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_PRIOR, undef, false, '<undef>');
                    }
                    # Check compression
                    elsif ($oAbortedManifest->boolGet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_COMPRESS) !=
                           cfgOption(CFGOPT_COMPRESS))
                    {
                        $strKey = MANIFEST_KEY_COMPRESS;
                        $strValueNew = cfgOption(CFGOPT_COMPRESS);
                        $strValueAborted = $oAbortedManifest->boolGet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_COMPRESS);
                    }
                    # Check hardlink
                    elsif ($oAbortedManifest->boolGet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_HARDLINK) !=
                           cfgOption(CFGOPT_REPO_HARDLINK))
                    {
                        $strKey = MANIFEST_KEY_HARDLINK;
                        $strValueNew = cfgOption(CFGOPT_REPO_HARDLINK);
                        $strValueAborted = $oAbortedManifest->boolGet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_HARDLINK);
                    }

                    # If key is defined then something didn't match
                    if (defined($strKey))
                    {
                        $strReason = "new ${strKey} '${strValueNew}' does not match aborted ${strKey} '${strValueAborted}'";
                    }
                    # Else the backup can be resumed
                    else
                    {
                        $bUsable = true;
                    }

                    return true;
                }
                or do
                {
                    $bUsable = false;
                }
            }

            # If the backup is usable then set the backup label
            if ($bUsable)
            {
                $strBackupLabel = $strAbortedBackup;

                # If the repo is encrypted, set the backup set passphrase from this manifest
                if (defined($strCipherPassManifest))
                {
                    $strCipherPassBackupSet = $oAbortedManifest->cipherPassSub();
                }
            }
            else
            {
                &log(WARN, "aborted backup ${strAbortedBackup} cannot be resumed: ${strReason}");
                &log(TEST, TEST_BACKUP_NORESUME);

                $oStorageRepo->remove(STORAGE_REPO_BACKUP . "/${strAbortedBackup}", {bRecurse => true});
                undef($oAbortedManifest);
            }

            last;
        }
    }

    # Generate a passphrase for the backup set if the repo is encrypted
    if (defined($strCipherPassManifest) && !defined($strCipherPassBackupSet) && $strType eq CFGOPTVAL_BACKUP_TYPE_FULL)
    {
        $strCipherPassBackupSet = cipherPassGen();
    }

    # If backup label is not defined then create the label and path.
    if (!defined($strBackupLabel))
    {
        $strBackupLabel = backupLabel($oStorageRepo, $strType, $strBackupLastPath, $lTimestampStart);
        $strBackupPath = $oStorageRepo->pathGet(STORAGE_REPO_BACKUP . "/${strBackupLabel}");
    }

    # Declare the backup manifest. Since the manifest could be an aborted backup, don't load it from the file here.
    # Instead just instantiate it. Pass the passphrases to open the manifest and one to encrypt the backup files if the repo is
    # encrypted (undefined if not).
    my $oBackupManifest = new pgBackRest::Manifest("$strBackupPath/" . FILE_MANIFEST,
        {bLoad => false, strDbVersion => $strDbVersion,
        strCipherPass => defined($strCipherPassManifest) ? $strCipherPassManifest : undef,
        strCipherPassSub => defined($strCipherPassManifest) ? $strCipherPassBackupSet : undef});

    # Backup settings
    $oBackupManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TYPE, undef, $strType);
    $oBackupManifest->numericSet(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TIMESTAMP_START, undef, $lTimestampStart);
    $oBackupManifest->boolSet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_BACKUP_STANDBY, undef, cfgOption(CFGOPT_BACKUP_STANDBY));
    $oBackupManifest->boolSet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_COMPRESS, undef, $bCompress);
    $oBackupManifest->boolSet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_HARDLINK, undef, $bHardLink);
    $oBackupManifest->boolSet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_ONLINE, undef, cfgOption(CFGOPT_ONLINE));
    $oBackupManifest->boolSet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_ARCHIVE_COPY, undef,
                              !cfgOption(CFGOPT_ONLINE) ||
                              (cfgOption(CFGOPT_ARCHIVE_CHECK) && cfgOption(CFGOPT_ARCHIVE_COPY)));
    $oBackupManifest->boolSet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_ARCHIVE_CHECK, undef,
                              !cfgOption(CFGOPT_ONLINE) || cfgOption(CFGOPT_ARCHIVE_CHECK));

    # Database settings
    $oBackupManifest->numericSet(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_DB_ID, undef, $iDbHistoryId);
    $oBackupManifest->numericSet(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_CONTROL, undef, $iControlVersion);
    $oBackupManifest->numericSet(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_CATALOG, undef, $iCatalogVersion);
    $oBackupManifest->numericSet(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_SYSTEM_ID, undef, $ullDbSysId);

    # Backup from standby can only be used on PostgreSQL >= 9.1
    if (cfgOption(CFGOPT_ONLINE) && cfgOption(CFGOPT_BACKUP_STANDBY) && $strDbVersion < PG_VERSION_BACKUP_STANDBY)
    {
        confess &log(ERROR,
            'option \'' . cfgOptionName(CFGOPT_BACKUP_STANDBY) . '\' not valid for PostgreSQL < ' . PG_VERSION_BACKUP_STANDBY,
            ERROR_CONFIG);
    }

    # Start backup (unless --no-online is set)
    my $strArchiveStart = undef;
    my $strLsnStart = undef;
    my $hTablespaceMap = undef;
	my $hDatabaseMap = undef;

    # If this is an offline backup
    if (!cfgOption(CFGOPT_ONLINE))
    {
        # If checksum-page is not explictly enabled then disable it.  Even if the version is high enough to have checksums we can't
        # know if they are enabled without asking the database.  When pg_control can be reliably parsed then this decision could be
        # based on that.
        if (!cfgOptionTest(CFGOPT_CHECKSUM_PAGE))
        {
            cfgOptionSet(CFGOPT_CHECKSUM_PAGE, false);
        }

        # Check if Postgres is running and if so only continue when forced
        if ($oStorageDbMaster->exists($strDbMasterPath . '/' . DB_FILE_POSTMASTERPID))
        {
            if (cfgOption(CFGOPT_FORCE))
            {
                &log(WARN, '--no-online passed and ' . DB_FILE_POSTMASTERPID . ' exists but --force was passed so backup will ' .
                           'continue though it looks like the postmaster is running and the backup will probably not be ' .
                           'consistent');
            }
            else
            {
                confess &log(ERROR, '--no-online passed but ' . DB_FILE_POSTMASTERPID . ' exists - looks like the postmaster is ' .
                            'running. Shutdown the postmaster and try again, or use --force.', ERROR_POSTMASTER_RUNNING);
            }
        }
    }
    # Else start the backup normally
    else
    {
        # Start the backup
        ($strArchiveStart, $strLsnStart) =
            $oDbMaster->backupStart(
                BACKREST_NAME . ' backup started at ' . timestampFormat(undef, $lTimestampStart), cfgOption(CFGOPT_START_FAST));

        # Record the archive start location
        $oBackupManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_ARCHIVE_START, undef, $strArchiveStart);
        $oBackupManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_LSN_START, undef, $strLsnStart);
        &log(INFO, "backup start archive = ${strArchiveStart}, lsn = ${strLsnStart}");

        # Get tablespace map
        $hTablespaceMap = $oDbMaster->tablespaceMapGet();

        # Get database map
        $hDatabaseMap = $oDbMaster->databaseMapGet();

        # Wait for replay on the standby to catch up
        if (cfgOption(CFGOPT_BACKUP_STANDBY))
        {
            my ($strStandbyDbVersion, $iStandbyControlVersion, $iStandbyCatalogVersion, $ullStandbyDbSysId) = $oDbStandby->info();
            $oBackupInfo->check($strStandbyDbVersion, $iStandbyControlVersion, $iStandbyCatalogVersion, $ullStandbyDbSysId);

            $oDbStandby->configValidate();

            &log(INFO, "wait for replay on the standby to reach ${strLsnStart}");

            my ($strReplayedLSN, $strCheckpointLSN) = $oDbStandby->replayWait($strLsnStart);

            &log(
                INFO,
                "replay on the standby reached ${strReplayedLSN}" .
                    (defined($strCheckpointLSN) ? ", checkpoint ${strCheckpointLSN}" : ''));

            # The standby db object won't be used anymore so undef it to catch any subsequent references
            undef($oDbStandby);
            protocolDestroy(CFGOPTVAL_REMOTE_TYPE_DB, $self->{iCopyRemoteIdx}, true);
        }
    }

    # Don't allow the checksum-page option to change in a diff or incr backup.  This could be confusing as only certain files would
    # be checksummed and the list could be incomplete during reporting.
    if ($strType ne CFGOPTVAL_BACKUP_TYPE_FULL && defined($strBackupLastPath))
    {
        # If not defined this backup was done in a version prior to page checksums being introduced.  Just set checksum-page to
        # false and move on without a warning.  Page checksums will start on the next full backup.
        if (!$oLastManifest->test(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_CHECKSUM_PAGE))
        {
            cfgOptionSet(CFGOPT_CHECKSUM_PAGE, false);
        }
        else
        {
            my $bChecksumPageLast =
                $oLastManifest->boolGet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_CHECKSUM_PAGE);

            if ($bChecksumPageLast != cfgOption(CFGOPT_CHECKSUM_PAGE))
            {
                &log(WARN,
                    "${strType} backup cannot alter '" . cfgOptionName(CFGOPT_CHECKSUM_PAGE) . "' option to '" .
                        boolFormat(cfgOption(CFGOPT_CHECKSUM_PAGE)) . "', reset to '" . boolFormat($bChecksumPageLast) .
                        "' from ${strBackupLastPath}");
                cfgOptionSet(CFGOPT_CHECKSUM_PAGE, $bChecksumPageLast);
            }
        }
    }

    # Record checksum-page option in the manifest
    $oBackupManifest->boolSet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_CHECKSUM_PAGE, undef, cfgOption(CFGOPT_CHECKSUM_PAGE));

    # Build the manifest
    $oBackupManifest->build($oStorageDbMaster, $strDbMasterPath, $oLastManifest, cfgOption(CFGOPT_ONLINE),
                            $hTablespaceMap, $hDatabaseMap);
    &log(TEST, TEST_MANIFEST_BUILD);

    # If resuming from an aborted backup
    if (defined($oAbortedManifest))
    {
        &log(WARN, "aborted backup ${strBackupLabel} of same type exists, will be cleaned to remove invalid files and resumed");
        &log(TEST, TEST_BACKUP_RESUME);

        # Clean the backup path before resuming
        $self->resumeClean($oStorageRepo, $strBackupLabel, $oBackupManifest, $oAbortedManifest);
    }
    # Else create the backup path
    else
    {
        logDebugMisc($strOperation, "create backup path ${strBackupPath}");
        $oStorageRepo->pathCreate(STORAGE_REPO_BACKUP . "/${strBackupLabel}");
    }

    # Save the backup manifest
    $oBackupManifest->saveCopy();

    # Perform the backup
    my $lBackupSizeTotal =
        $self->processManifest(
            $strDbMasterPath, $strDbCopyPath, $strType, $strDbVersion, $bCompress, $bHardLink, $oBackupManifest, $strBackupLabel,
            $strLsnStart);
    &log(INFO, "${strType} backup size = " . fileSizeFormat($lBackupSizeTotal));

    # Master file object no longer needed
    undef($oStorageDbMaster);

    # Stop backup (unless --no-online is set)
    my $strArchiveStop = undef;
    my $strLsnStop = undef;

    if (cfgOption(CFGOPT_ONLINE))
    {
        ($strArchiveStop, $strLsnStop, my $strTimestampDbStop, my $oFileHash) = $oDbMaster->backupStop();

        $oBackupManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_ARCHIVE_STOP, undef, $strArchiveStop);
        $oBackupManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_LSN_STOP, undef, $strLsnStop);
        &log(INFO, "backup stop archive = ${strArchiveStop}, lsn = ${strLsnStop}");

        # Write out files returned from stop backup
        foreach my $strFile (sort(keys(%{$oFileHash})))
        {
            # Only save the file if it has content
            if (defined($oFileHash->{$strFile}))
            {
                my $rhyFilter = [{strClass => STORAGE_FILTER_SHA}];

                # Add compression filter
                if ($bCompress)
                {
                    push(@{$rhyFilter}, {strClass => STORAGE_FILTER_GZIP});
                }

                # If the backups are encrypted, then the passphrase for the backup set from the manifest file is required to access
                # the file in the repo
                my $oDestinationFileIo = $oStorageRepo->openWrite(
                    STORAGE_REPO_BACKUP . "/${strBackupLabel}/${strFile}" . ($bCompress ? qw{.} . COMPRESS_EXT : ''),
                    {rhyFilter => $rhyFilter,
                    strCipherPass => defined($strCipherPassBackupSet) ? $strCipherPassBackupSet : undef});

                # Write content out to a file
                $oStorageRepo->put($oDestinationFileIo, $oFileHash->{$strFile});

                # Add file to manifest
                $oBackupManifest->fileAdd(
                    $strFile, time(), length($oFileHash->{$strFile}), $oDestinationFileIo->result(STORAGE_FILTER_SHA), true);

                &log(DETAIL, "wrote '${strFile}' file returned from pg_stop_backup()");
            }
        }
    }

    # Remotes no longer needed (destroy them here so they don't timeout)
    &log(TEST, TEST_BACKUP_STOP);

    undef($oDbMaster);
    protocolDestroy(undef, undef, true);

    # If archive logs are required to complete the backup, then check them.  This is the default, but can be overridden if the
    # archive logs are going to a different server.  Be careful of this option because there is no way to verify that the backup
    # will be consistent - at least not here.
    if (cfgOption(CFGOPT_ONLINE) && cfgOption(CFGOPT_ARCHIVE_CHECK))
    {
        # Save the backup manifest before getting archive logs in case of failure
        $oBackupManifest->saveCopy();

        # Create the modification time for the archive logs
        my $lModificationTime = time();

        # After the backup has been stopped, need to make a copy of the archive logs to make the db consistent
        logDebugMisc($strOperation, "retrieve archive logs ${strArchiveStart}:${strArchiveStop}");

        my $oArchiveInfo = new pgBackRest::Archive::Info(storageRepo()->pathGet(STORAGE_REPO_ARCHIVE), true);
        my $strArchiveId = $oArchiveInfo->archiveId();
        my @stryArchive = lsnFileRange($strLsnStart, $strLsnStop, $strDbVersion);

        foreach my $strArchive (@stryArchive)
        {
            my $strArchiveFile = walSegmentFind(
                $oStorageRepo, $strArchiveId, substr($strArchiveStop, 0, 8) . $strArchive, cfgOption(CFGOPT_ARCHIVE_TIMEOUT));

            $strArchive = substr($strArchiveFile, 0, 24);

            if (cfgOption(CFGOPT_ARCHIVE_COPY))
            {
                logDebugMisc($strOperation, "archive: ${strArchive} (${strArchiveFile})");

                # Copy the log file from the archive repo to the backup
                my $bArchiveCompressed = $strArchiveFile =~ ('^.*\.' . COMPRESS_EXT . '\$');

                $oStorageRepo->copy(
                    $oStorageRepo->openRead(STORAGE_REPO_ARCHIVE . "/${strArchiveId}/${strArchiveFile}",
                        {strCipherPass => $oArchiveInfo->cipherPassSub()}),
                    $oStorageRepo->openWrite(STORAGE_REPO_BACKUP . "/${strBackupLabel}/" . MANIFEST_TARGET_PGDATA . qw{/} .
                        $oBackupManifest->walPath() . "/${strArchive}" . ($bCompress ? qw{.} . COMPRESS_EXT : ''),
                        {bPathCreate => true, strCipherPass => $strCipherPassBackupSet})
                    );

                # Add the archive file to the manifest so it can be part of the restore and checked in validation
                my $strPathLog = MANIFEST_TARGET_PGDATA . qw{/} . $oBackupManifest->walPath();
                my $strFileLog = "${strPathLog}/${strArchive}";

                # Add file to manifest
                $oBackupManifest->fileAdd(
                    $strFileLog, $lModificationTime, PG_WAL_SEGMENT_SIZE, substr($strArchiveFile, 25, 40), true);
            }
        }
    }

    # Record timestamp stop in the config
    my $lTimestampStop = time();
    $oBackupManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TIMESTAMP_STOP, undef, $lTimestampStop + 0);
    $oBackupManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_LABEL, undef, $strBackupLabel);

    # Sync backup path if supported
    if ($oStorageRepo->driver()->capability(STORAGE_CAPABILITY_PATH_SYNC))
    {
        # Sync all paths in the backup
        $oStorageRepo->pathSync(STORAGE_REPO_BACKUP . "/${strBackupLabel}");

        foreach my $strPath ($oBackupManifest->keys(MANIFEST_SECTION_TARGET_PATH))
        {
            my $strPathSync = $oStorageRepo->pathGet(STORAGE_REPO_BACKUP . "/${strBackupLabel}/$strPath");

            # Not all paths are created for diff/incr backups, so only sync if this is a full backup or the path exists
            if ($strType eq CFGOPTVAL_BACKUP_TYPE_FULL || $oStorageRepo->pathExists($strPathSync))
            {
                $oStorageRepo->pathSync($strPathSync);
            }
        }
    }

    # Final save of the backup manifest
    $oBackupManifest->save();

    &log(INFO, "new backup label = ${strBackupLabel}");

    # Copy a compressed version of the manifest to history. If the repo is encrypted then the passphrase to open the manifest is
    # required.
    my $strHistoryPath = $oStorageRepo->pathGet(
        STORAGE_REPO_BACKUP . qw{/} . PATH_BACKUP_HISTORY . qw{/} . substr($strBackupLabel, 0, 4));

    $oStorageRepo->copy(
        $oStorageRepo->openRead(STORAGE_REPO_BACKUP . "/${strBackupLabel}/" . FILE_MANIFEST,
            {'strCipherPass' => $strCipherPassManifest}),
        $oStorageRepo->openWrite(
            "${strHistoryPath}/${strBackupLabel}.manifest." . COMPRESS_EXT,
            {rhyFilter => [{strClass => STORAGE_FILTER_GZIP}],
                bPathCreate => true, bAtomic => true,
                strCipherPass => defined($strCipherPassManifest) ? $strCipherPassManifest : undef}));

    # Sync history path if supported
    if ($oStorageRepo->driver()->capability(STORAGE_CAPABILITY_PATH_SYNC))
    {
        $oStorageRepo->pathSync(STORAGE_REPO_BACKUP . qw{/} . PATH_BACKUP_HISTORY);
        $oStorageRepo->pathSync($strHistoryPath);
    }

    # Create a link to the most recent backup
    $oStorageRepo->remove(STORAGE_REPO_BACKUP . qw(/) . LINK_LATEST);

    if (storageRepo()->driver()->capability(STORAGE_CAPABILITY_LINK))
    {
        $oStorageRepo->linkCreate(
            STORAGE_REPO_BACKUP . "/${strBackupLabel}", STORAGE_REPO_BACKUP . qw{/} . LINK_LATEST, {bRelative => true});
    }

    # Save backup info
    $oBackupInfo->add($oBackupManifest);

    # Sync backup root path if supported
    if ($oStorageRepo->driver()->capability(STORAGE_CAPABILITY_PATH_SYNC))
    {
        $oStorageRepo->pathSync(STORAGE_REPO_BACKUP);
    }

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

1;
