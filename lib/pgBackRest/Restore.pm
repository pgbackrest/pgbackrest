####################################################################################################################################
# RESTORE MODULE
####################################################################################################################################
package pgBackRest::Restore;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Cwd qw(abs_path);
use File::Basename qw(basename dirname);
use File::stat qw(lstat);

use pgBackRest::Backup::Info;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Ini;
use pgBackRest::Common::Log;
use pgBackRest::Config::Config;
use pgBackRest::DbVersion;
use pgBackRest::Manifest;
use pgBackRest::RestoreFile;
use pgBackRest::Protocol::Helper;
use pgBackRest::Protocol::Local::Process;
use pgBackRest::Protocol::Storage::Helper;
use pgBackRest::Storage::Helper;
use pgBackRest::Version;

####################################################################################################################################
# CONSTRUCTOR
####################################################################################################################################
sub new
{
    my $class = shift;              # Class name

    # Create the class hash
    my $self = {};
    bless $self, $class;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->new');

    # Initialize protocol
    $self->{oProtocol} = !isRepoLocal() ? protocolGet(CFGOPTVAL_REMOTE_TYPE_BACKUP) : undef;

    # Initialize variables
    $self->{strDbClusterPath} = cfgOption(CFGOPT_PG_PATH);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# recovery
#
# Creates the recovery.conf file.
####################################################################################################################################
sub recovery
{
    my $self = shift;           # Class hash
    my $strDbVersion = shift;   # Version to restore

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam (__PACKAGE__ . '->recovery');

    # Create recovery.conf path/file
    my $strRecoveryConf = $self->{strDbClusterPath} . '/' . DB_FILE_RECOVERYCONF;

    # See if recovery.conf already exists
    my $bRecoveryConfExists = storageDb()->exists($strRecoveryConf);

    # If CFGOPTVAL_RESTORE_TYPE_PRESERVE then warn if recovery.conf does not exist and return
    if (cfgOptionTest(CFGOPT_TYPE, CFGOPTVAL_RESTORE_TYPE_PRESERVE))
    {
        if (!$bRecoveryConfExists)
        {
            &log(WARN, "recovery type is " . cfgOption(CFGOPT_TYPE) . " but recovery file does not exist at ${strRecoveryConf}");
        }
    }
    else
    {
        # In all other cases the old recovery.conf should be removed if it exists
        if ($bRecoveryConfExists)
        {
            storageDb()->remove($strRecoveryConf);
        }

        # If CFGOPTVAL_RESTORE_TYPE_NONE then return
        if (!cfgOptionTest(CFGOPT_TYPE, CFGOPTVAL_RESTORE_TYPE_NONE))
        {
            # Write recovery options read from the configuration file
            my $strRecovery = '';
            my $bRestoreCommandOverride = false;

            if (cfgOptionTest(CFGOPT_RECOVERY_OPTION))
            {
                my $oRecoveryRef = cfgOption(CFGOPT_RECOVERY_OPTION);

                foreach my $strKey (sort(keys(%$oRecoveryRef)))
                {
                    my $strPgKey = $strKey;
                    $strPgKey =~ s/\-/\_/g;

                    if ($strPgKey eq 'restore_command')
                    {
                        $bRestoreCommandOverride = true;
                    }

                    $strRecovery .= "${strPgKey} = '$$oRecoveryRef{$strKey}'\n";
                }
            }

            # Write the restore command
            if (!$bRestoreCommandOverride)
            {
                $strRecovery .=  "restore_command = '" . cfgCommandWrite(CFGCMD_ARCHIVE_GET) . " %f \"%p\"'\n";
            }

            # If type is CFGOPTVAL_RESTORE_TYPE_IMMEDIATE
            if (cfgOptionTest(CFGOPT_TYPE, CFGOPTVAL_RESTORE_TYPE_IMMEDIATE))
            {
                $strRecovery .= "recovery_target = '" . CFGOPTVAL_RESTORE_TYPE_IMMEDIATE . "'\n";
            }
            # If type is not CFGOPTVAL_RESTORE_TYPE_DEFAULT write target options
            elsif (!cfgOptionTest(CFGOPT_TYPE, CFGOPTVAL_RESTORE_TYPE_DEFAULT))
            {
                # Write the recovery target
                $strRecovery .= "recovery_target_" . cfgOption(CFGOPT_TYPE) . " = '" . cfgOption(CFGOPT_TARGET) . "'\n";

                # Write recovery_target_inclusive
                if (cfgOption(CFGOPT_TARGET_EXCLUSIVE, false))
                {
                    $strRecovery .= "recovery_target_inclusive = 'false'\n";
                }
            }

            # Write pause_at_recovery_target/recovery_target_action
            if (cfgOptionTest(CFGOPT_TARGET_ACTION))
            {
                my $strTargetAction = cfgOption(CFGOPT_TARGET_ACTION);

                if ($strTargetAction ne cfgOptionDefault(CFGOPT_TARGET_ACTION))
                {
                    if ($strDbVersion >= PG_VERSION_95)
                    {
                        $strRecovery .= "recovery_target_action = '${strTargetAction}'\n";
                    }
                    elsif  ($strDbVersion >= PG_VERSION_91)
                    {
                        if ($strTargetAction eq CFGOPTVAL_RESTORE_TARGET_ACTION_SHUTDOWN)
                        {
                            confess &log(ERROR,
                                cfgOptionName(CFGOPT_TARGET_ACTION) .  '=' . CFGOPTVAL_RESTORE_TARGET_ACTION_SHUTDOWN .
                                    ' is only available in PostgreSQL >= ' . PG_VERSION_95)
                        }

                        $strRecovery .= "pause_at_recovery_target = 'false'\n";
                    }
                    else
                    {
                        confess &log(ERROR,
                            cfgOptionName(CFGOPT_TARGET_ACTION) .  ' option is only available in PostgreSQL >= ' . PG_VERSION_91)
                    }
                }
            }

            # Write recovery_target_timeline
            if (cfgOptionTest(CFGOPT_TARGET_TIMELINE))
            {
                $strRecovery .= "recovery_target_timeline = '" . cfgOption(CFGOPT_TARGET_TIMELINE) . "'\n";
            }

            # Write recovery.conf
            my $hFile;

            open($hFile, '>', $strRecoveryConf)
                or confess &log(ERROR, "unable to open ${strRecoveryConf}: $!");

            syswrite($hFile, $strRecovery)
                or confess "unable to write section ${strRecoveryConf}: $!";

            close($hFile)
                or confess "unable to close ${strRecoveryConf}: $!";

            &log(INFO, "write $strRecoveryConf");
        }
    }

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# process
#
# Takes a backup and restores it back to the original or a remapped location.
####################################################################################################################################
sub process
{
    my $self = shift;       # Class hash

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam (__PACKAGE__ . '->process');

    # Db storage
    my $oStorageDb = storageDb();

    # Load the manifest into a hash (already partially processed by C)
    my $oManifest = new pgBackRest::Manifest(
        storageDb()->pathGet($self->{strDbClusterPath} . '/' . FILE_MANIFEST), {oStorage => storageDb()});

    # Substitute current user for false in user/group (code further expects false to be replaced already)
    my %oOwnerHash = ();

    my %oFileTypeHash =
    (
        &MANIFEST_SECTION_TARGET_PATH => true,
        &MANIFEST_SECTION_TARGET_LINK => true,
        &MANIFEST_SECTION_TARGET_FILE => true
    );

    my %oOwnerTypeHash;

    $oOwnerTypeHash{&MANIFEST_SUBKEY_USER} = getpwuid($<);

    if (!defined($oOwnerTypeHash{&MANIFEST_SUBKEY_USER}))
    {
        confess &log(ERROR_USER_MISSING, 'current user uid does not map to a name');
    }

    $oOwnerTypeHash{&MANIFEST_SUBKEY_GROUP} = getgrgid($();

    if (!defined($oOwnerTypeHash{&MANIFEST_SUBKEY_GROUP}))
    {
        confess &log(ERROR_GROUP_MISSING, 'current user gid does not map to a name');
    }

    foreach my $strOwnerType (sort (keys %oOwnerTypeHash))
    {
        foreach my $strSection (sort (keys %oFileTypeHash))
        {
            foreach my $strName ($oManifest->keys($strSection))
            {
                my $strOwner = $oManifest->get($strSection, $strName, $strOwnerType);

                if ($oManifest->boolTest($strSection, $strName, $strOwnerType, false))
                {
                    $strOwner = $oOwnerTypeHash{$strOwnerType};
                    $oManifest->set($strSection, $strName, $strOwnerType, $strOwner);
                }
           }
        }
    }

    # Make sure links are still valid after remapping
    $oManifest->linkCheck();

    # Delete pg_control file.  This will be copied from the backup at the very end to prevent a partially restored database
    # from being started by PostgreSQL.
    $oStorageDb->remove(
        $oManifest->dbPathGet(
            $oManifest->get(MANIFEST_SECTION_BACKUP_TARGET, MANIFEST_TARGET_PGDATA, MANIFEST_SUBKEY_PATH),
            MANIFEST_FILE_PGCONTROL));

    # Get variables required for restore
    my $strCurrentUser = getpwuid($<);
    my $strCurrentGroup = getgrgid($();

    # Build an expression to match files that should be zeroed for filtered restores
    my $strDbFilter;

    if (cfgOptionTest(CFGOPT_DB_INCLUDE))
    {
        # Build a list of databases from the manifest since the db name/id mappings will not be available for an offline restore.
        my %oDbList;

        foreach my $strFile ($oManifest->keys(MANIFEST_SECTION_TARGET_FILE))
        {
            my $strTblspcRegEx;
            if ($oManifest->dbVersion() < PG_VERSION_90)
            {
                $strTblspcRegEx = '^' . MANIFEST_TARGET_PGTBLSPC . '\/[0-9]+\/[0-9]+\/PG\_VERSION';
            }
            else
            {
                $strTblspcRegEx = '^' . MANIFEST_TARGET_PGTBLSPC .
                    '\/[0-9]+\/'.$oManifest->tablespacePathGet().'\/[0-9]+\/PG\_VERSION';
            }

            # Check for DBs not in a tablespace and those that were created in a tablespace
            if ($strFile =~ ('^' . MANIFEST_TARGET_PGDATA . '\/base\/[0-9]+\/PG\_VERSION') ||
                $strFile =~ ($strTblspcRegEx))
            {
                my $lDbId = basename(dirname($strFile));

                $oDbList{$lDbId} = true;
            }
        }

        # If no databases where found then this backup does not contain a valid cluster
        if (keys(%oDbList) == 0)
        {
            confess &log(ASSERT, 'no databases for include/exclude -- does not look like a valid cluster');
        }

        # Log databases found
        # &log(DETAIL, 'databases for include/exclude (' . join(', ', sort(keys(%oDbList))) . ')');

        # Remove included databases from the list
        my $oDbInclude = cfgOption(CFGOPT_DB_INCLUDE);

        for my $strDbKey (sort(keys(%{$oDbInclude})))
        {
            # To be included the db must exist - first treat the key as an id and check for a match
            if (!defined($oDbList{$strDbKey}))
            {
                # If the key does not match as an id then check for a name mapping
                my $lDbId = $oManifest->get(MANIFEST_SECTION_DB, $strDbKey, MANIFEST_KEY_DB_ID, false);

                if (!defined($lDbId) || !defined($oDbList{$lDbId}))
                {
                    confess &log(ERROR, "database to include '${strDbKey}' does not exist", ERROR_DB_MISSING);
                }

                # Set the key to the id if the name mapping was successful
                $strDbKey = $lDbId;
            }

            # Error if the db is a built-in db
            if ($strDbKey < DB_USER_OBJECT_MINIMUM_ID)
            {
                confess &log(ERROR, "system databases (template0, postgres, etc.) are included by default", ERROR_DB_INVALID);
            }

            # Otherwise remove from list of DBs to zero
            delete($oDbList{$strDbKey});
        }

        # Construct regexp to identify files that should be zeroed
        for my $strDbKey (sort(keys(%oDbList)))
        {
            # Only user created databases can be zeroed, never built-in databases
            if ($strDbKey >= DB_USER_OBJECT_MINIMUM_ID)
            {
                # Filter files in base directory
                $strDbFilter .= (defined($strDbFilter) ? '|' : '') .
                    '(^' . MANIFEST_TARGET_PGDATA . '\/base\/' . $strDbKey . '\/)';

                # Filter files in tablespace directories
                for my $strTarget ($oManifest->keys(MANIFEST_SECTION_BACKUP_TARGET))
                {
                    if ($oManifest->isTargetTablespace($strTarget))
                    {
                        if ($oManifest->dbVersion() < PG_VERSION_90)
                        {
                            $strDbFilter .= '|(^' . $strTarget . '\/' . $strDbKey . '\/)';
                        }
                        else
                        {
                            $strDbFilter .=
                                '|(^' . $strTarget . '\/' . $oManifest->tablespacePathGet() . '\/' . $strDbKey . '\/)';
                        }
                    }
                }
            }
        }

        # Output the generated filter for debugging
        # &log(DETAIL, "database filter: " . (defined($strDbFilter) ? "${strDbFilter}" : ''));
    }

    # Initialize the restore process
    my $oRestoreProcess = new pgBackRest::Protocol::Local::Process(CFGOPTVAL_LOCAL_TYPE_BACKUP);
    $oRestoreProcess->hostAdd(1, cfgOption(CFGOPT_PROCESS_MAX));

    # Variables used for parallel copy
    my $lSizeTotal = 0;
    my $lSizeCurrent = 0;

    foreach my $strRepoFile (
        sort {sprintf("%016d-%s", $oManifest->numericGet(MANIFEST_SECTION_TARGET_FILE, $b, MANIFEST_SUBKEY_SIZE), $b) cmp
              sprintf("%016d-%s", $oManifest->numericGet(MANIFEST_SECTION_TARGET_FILE, $a, MANIFEST_SUBKEY_SIZE), $a)}
        ($oManifest->keys(MANIFEST_SECTION_TARGET_FILE, INI_SORT_NONE)))
    {
        # Skip the tablespace_map file in versions >= 9.5 so Postgres does not rewrite links in DB_PATH_PGTBLSPC.
        # The tablespace links have already been created by Restore::build().
        if ($strRepoFile eq MANIFEST_FILE_TABLESPACEMAP &&
            $oManifest->get(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_DB_VERSION) >= PG_VERSION_95)
        {
            next;
        }

        # By default put everything into a single queue
        my $strQueueKey = MANIFEST_TARGET_PGDATA;

        # If the file belongs in a tablespace then put in a tablespace-specific queue
        if (index($strRepoFile, DB_PATH_PGTBLSPC . '/') == 0)
        {
            $strQueueKey = DB_PATH_PGTBLSPC . '/' . (split('\/', $strRepoFile))[1];
        }

        # Get restore information
        my $strDbFile = $oManifest->dbPathGet($self->{strDbClusterPath}, $strRepoFile);
        my $lSize = $oManifest->numericGet(MANIFEST_SECTION_TARGET_FILE, $strRepoFile, MANIFEST_SUBKEY_SIZE);

        # Copy pg_control to a temporary file that will be renamed later
        if ($strRepoFile eq MANIFEST_TARGET_PGDATA . '/' . DB_FILE_PGCONTROL)
        {
            $strDbFile .= '.' . STORAGE_TEMP_EXT;
        }

        # Increment file size
        $lSizeTotal += $lSize;

        # Queue for parallel restore
        $oRestoreProcess->queueJob(
            1, $strQueueKey, $strRepoFile, OP_RESTORE_FILE,
            [$strDbFile, $lSize,
                $oManifest->numericGet(MANIFEST_SECTION_TARGET_FILE, $strRepoFile, MANIFEST_SUBKEY_TIMESTAMP),
                $oManifest->get(MANIFEST_SECTION_TARGET_FILE, $strRepoFile, MANIFEST_SUBKEY_CHECKSUM, $lSize > 0),
                defined($strDbFilter) && $strRepoFile =~ $strDbFilter && $strRepoFile !~ /\/PG\_VERSION$/ ? true : false,
                cfgOption(CFGOPT_FORCE), $strRepoFile,
                $oManifest->boolTest(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_HARDLINK, undef, true) ? undef :
                    $oManifest->get(MANIFEST_SECTION_TARGET_FILE, $strRepoFile, MANIFEST_SUBKEY_REFERENCE, false),
                $oManifest->get(MANIFEST_SECTION_TARGET_FILE, $strRepoFile, MANIFEST_SUBKEY_MODE),
                $oManifest->get(MANIFEST_SECTION_TARGET_FILE, $strRepoFile, MANIFEST_SUBKEY_USER),
                $oManifest->get(MANIFEST_SECTION_TARGET_FILE, $strRepoFile, MANIFEST_SUBKEY_GROUP),
                $oManifest->numericGet(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TIMESTAMP_COPY_START),  cfgOption(CFGOPT_DELTA),
                $oManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_LABEL),
                $oManifest->boolGet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_COMPRESS)],
            {rParamSecure => $oManifest->cipherPassSub() ? [$oManifest->cipherPassSub()] : undef});
    }

    # Run the restore jobs and process results
    while (my $hyJob = $oRestoreProcess->process())
    {
        foreach my $hJob (@{$hyJob})
        {
            ($lSizeCurrent) = restoreLog(
                $hJob->{iProcessId}, @{$hJob->{rParam}}[0..5], @{$hJob->{rResult}}, $lSizeTotal, $lSizeCurrent);
        }

        # A keep-alive is required here because if there are a large number of resumed files that need to be checksummed
        # then the remote might timeout while waiting for a command.
        protocolKeepAlive();
    }

    # Create recovery.conf file
    $self->recovery($oManifest->get(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_DB_VERSION));

    # Sync db cluster paths
    # foreach my $strPath ($oManifest->keys(MANIFEST_SECTION_TARGET_PATH))
    # {
    #     # This is already synced as a subpath of pg_data
    #     next if $strPath eq MANIFEST_TARGET_PGTBLSPC;
    #
    #     $oStorageDb->pathSync($oManifest->dbPathGet($self->{strDbClusterPath}, $strPath));
    # }

    # Sync targets that don't have paths above
    # foreach my $strTarget ($oManifest->keys(MANIFEST_SECTION_BACKUP_TARGET))
    # {
    #     # Already synced
    #     next if $strTarget eq MANIFEST_TARGET_PGDATA;
    #
    #     $oStorageDb->pathSync(
    #         $oStorageDb->pathAbsolute(
    #             $self->{strDbClusterPath} . ($strTarget =~ ('^' . MANIFEST_TARGET_PGTBLSPC) ? '/' . MANIFEST_TARGET_PGTBLSPC : ''),
    #             $oManifest->get(MANIFEST_SECTION_BACKUP_TARGET, $strTarget, MANIFEST_VALUE_PATH)));
    # }

    # Move pg_control last
    &log(INFO,
        'restore ' . $oManifest->dbPathGet(undef, MANIFEST_FILE_PGCONTROL) .
        ' (performed last to ensure aborted restores cannot be started)');

    $oStorageDb->move(
        $self->{strDbClusterPath} . '/' . DB_FILE_PGCONTROL . '.' . STORAGE_TEMP_EXT,
        $self->{strDbClusterPath} . '/' . DB_FILE_PGCONTROL);

    # Sync to be sure the rename of pg_control is persisted
    $oStorageDb->pathSync($self->{strDbClusterPath});

    # Finally remove the manifest to indicate the restore is complete
    $oStorageDb->remove($self->{strDbClusterPath} . '/' . FILE_MANIFEST);

    # Sync removal of the manifest
    $oStorageDb->pathSync($self->{strDbClusterPath});

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

1;
