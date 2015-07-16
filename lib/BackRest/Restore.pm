####################################################################################################################################
# RESTORE MODULE
####################################################################################################################################
package BackRest::Restore;

use threads;
use threads::shared;
use Thread::Queue;
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use File::Basename qw(dirname);
use File::stat qw(lstat);

use lib dirname($0);
use BackRest::Config;
use BackRest::Db;
use BackRest::Exception;
use BackRest::File;
use BackRest::Manifest;
use BackRest::RestoreFile;
use BackRest::ThreadGroup;
use BackRest::Utility;

####################################################################################################################################
# Recovery.conf file
####################################################################################################################################
use constant FILE_RECOVERY_CONF  => 'recovery.conf';

####################################################################################################################################
# CONSTRUCTOR
####################################################################################################################################
sub new
{
    my $class = shift;              # Class name
    my $strDbClusterPath = shift;   # Database cluster path
    my $strBackupPath = shift;      # Backup to restore
    my $oRemapRef = shift;          # Tablespace remaps
    my $oFile = shift;              # Default file object
    my $iThreadTotal = shift;       # Total threads to run for restore
    my $bDelta = shift;             # perform delta restore
    my $bForce = shift;             # force a restore
    my $strType = shift;            # Recovery type
    my $strTarget = shift;          # Recovery target
    my $bTargetExclusive = shift;   # Target exlusive option
    my $bTargetResume = shift;      # Target resume option
    my $strTargetTimeline = shift;  # Target timeline option
    my $oRecoveryRef = shift;       # Other recovery options
    my $strStanza = shift;          # Restore stanza
    my $strBackRestBin = shift;     # Absolute backrest filename
    my $strConfigFile = shift;      # Absolute config filename (optional)

    # Create the class hash
    my $self = {};
    bless $self, $class;

    # Initialize variables
    $self->{strDbClusterPath} = $strDbClusterPath;
    $self->{strBackupPath} = $strBackupPath;
    $self->{oRemapRef} = $oRemapRef;
    $self->{oFile} = $oFile;
    $self->{iThreadTotal} = defined($iThreadTotal) ? $iThreadTotal : 1;
    $self->{bDelta} = $bDelta;
    $self->{bForce} = $bForce;
    $self->{strType} = $strType;
    $self->{strTarget} = $strTarget;
    $self->{bTargetExclusive} = $bTargetExclusive;
    $self->{bTargetResume} = $bTargetResume;
    $self->{strTargetTimeline} = $strTargetTimeline;
    $self->{oRecoveryRef} = $oRecoveryRef;
    $self->{strStanza} = $strStanza;
    $self->{strBackRestBin} = $strBackRestBin;
    $self->{strConfigFile} = $strConfigFile;

    return $self;
}

####################################################################################################################################
# MANIFEST_OWNERSHIP_CHECK
#
# Checks the users and groups that exist in the manifest and emits warnings for ownership that cannot be set properly, either
# because the current user does not have permissions or because the user/group does not exist.
####################################################################################################################################
sub manifest_ownership_check
{
    my $self = shift;               # Class hash
    my $oManifest = shift;          # Backup manifest

    # Create hashes to track valid/invalid users/groups
    my %oOwnerHash = ();

    # Create hash for each type and owner to be checked
    my $strDefaultUser = getpwuid($<);
    my $strDefaultGroup = getgrgid($();

    my %oFileTypeHash = (&MANIFEST_PATH => true, &MANIFEST_LINK => true, &MANIFEST_FILE => true);
    my %oOwnerTypeHash = (&MANIFEST_SUBKEY_USER => $strDefaultUser, &MANIFEST_SUBKEY_GROUP => $strDefaultGroup);

    # Loop through owner types (user, group)
    foreach my $strOwnerType (sort (keys %oOwnerTypeHash))
    {
        # Loop through all backup paths (base and tablespaces)
        foreach my $strPathKey ($oManifest->keys(MANIFEST_SECTION_BACKUP_PATH))
        {
            # Loop through types (path, link, file)
            foreach my $strFileType (sort (keys %oFileTypeHash))
            {
                my $strSection = "${strPathKey}:${strFileType}";

                # Get users and groups for paths
                if ($oManifest->test($strSection))
                {
                    foreach my $strName ($oManifest->keys($strSection))
                    {
                        my $strOwner = $oManifest->get($strSection, $strName, $strOwnerType);

                        # If root then test to see if the user/group is valid
                        if ($< == 0)
                        {
                            # If the owner has not been tested yet then test it
                            if (!defined($oOwnerHash{$strOwnerType}{$strOwner}))
                            {
                                my $strOwnerId;

                                if ($strOwnerType eq 'user')
                                {
                                    $strOwnerId = getpwnam($strOwner);
                                }
                                else
                                {
                                    $strOwnerId = getgrnam($strOwner);
                                }

                                $oOwnerHash{$strOwnerType}{$strOwner} = defined($strOwnerId) ? true : false;
                            }

                            if (!$oOwnerHash{$strOwnerType}{$strOwner})
                            {
                                $oManifest->set($strSection, $strName, $strOwnerType, $oOwnerTypeHash{$strOwnerType});
                            }
                        }
                        # Else set user/group to current user/group
                        else
                        {
                            if ($strOwner ne $oOwnerTypeHash{$strOwnerType})
                            {
                                $oOwnerHash{$strOwnerType}{$strOwner} = false;
                                $oManifest->set($strSection, $strName, $strOwnerType, $oOwnerTypeHash{$strOwnerType});
                            }
                        }
                   }
                }
            }
        }

        # Output warning for any invalid owners
        if (defined($oOwnerHash{$strOwnerType}))
        {
            foreach my $strOwner (sort (keys(%{$oOwnerHash{$strOwnerType}})))
            {
                if (!$oOwnerHash{$strOwnerType}{$strOwner})
                {
                    &log(WARN, "${strOwnerType} ${strOwner} " . ($< == 0 ? "does not exist" : "cannot be set") .
                               ", changed to $oOwnerTypeHash{$strOwnerType}");
                }
            }
        }
    }
}

####################################################################################################################################
# MANIFEST_LOAD
#
# Loads the backup manifest and performs requested tablespace remaps.
####################################################################################################################################
sub manifest_load
{
    my $self = shift;           # Class hash

    if ($self->{oFile}->exists(PATH_BACKUP_CLUSTER, $self->{strBackupPath}))
    {
        # Copy the backup manifest to the db cluster path
        $self->{oFile}->copy(PATH_BACKUP_CLUSTER, $self->{strBackupPath} . '/' . FILE_MANIFEST,
                             PATH_DB_ABSOLUTE, $self->{strDbClusterPath} . '/' . FILE_MANIFEST);

        # Load the manifest into a hash
        my $oManifest = new BackRest::Manifest($self->{oFile}->path_get(PATH_DB_ABSOLUTE,
                                                                        $self->{strDbClusterPath} . '/' . FILE_MANIFEST));

        # Remove the manifest now that it is in memory
        $self->{oFile}->remove(PATH_DB_ABSOLUTE, $self->{strDbClusterPath} . '/' . FILE_MANIFEST);

        # If backup is latest then set it equal to backup label, else verify that requested backup and label match
        my $strBackupLabel = $oManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_LABEL);

        if ($self->{strBackupPath} eq OPTION_DEFAULT_RESTORE_SET)
        {
            $self->{strBackupPath} = $strBackupLabel;
        }
        elsif ($self->{strBackupPath} ne $strBackupLabel)
        {
            confess &log(ASSERT, "request backup $self->{strBackupPath} and label ${strBackupLabel} do not match " .
                                 ' - this indicates some sort of corruption (at the very least paths have been renamed)');
        }

        if ($self->{strDbClusterPath} ne $oManifest->get(MANIFEST_SECTION_BACKUP_PATH, MANIFEST_KEY_BASE, MANIFEST_SUBKEY_PATH))
        {
            &log(INFO, 'remap base path to ' . $self->{strDbClusterPath});
            $oManifest->set(MANIFEST_SECTION_BACKUP_PATH, MANIFEST_KEY_BASE, MANIFEST_SUBKEY_PATH, $self->{strDbClusterPath});
        }

        # If no tablespaces are requested
        if (!optionGet(OPTION_TABLESPACE))
        {
            foreach my $strPathKey ($oManifest->keys(MANIFEST_SECTION_BACKUP_PATH))
            {
                if ($oManifest->test(MANIFEST_SECTION_BACKUP_PATH, $strPathKey, MANIFEST_SUBKEY_LINK))
                {
                    my $strTablespaceKey =
                        $oManifest->get(MANIFEST_SECTION_BACKUP_PATH, $strPathKey, MANIFEST_SUBKEY_LINK);
                    my $strTablespaceLink = "pg_tblspc/${strTablespaceKey}";
                    my $strTablespacePath =
                        $oManifest->get(MANIFEST_SECTION_BACKUP_PATH, MANIFEST_KEY_BASE, MANIFEST_SUBKEY_PATH) .
                        "/${strTablespaceLink}";

                    $oManifest->set(MANIFEST_SECTION_BACKUP_PATH, $strPathKey, MANIFEST_SUBKEY_PATH, $strTablespacePath);

                    $oManifest->remove('base:link', $strTablespaceLink);
                    $oManifest->set('base:path', $strTablespaceLink, MANIFEST_SUBKEY_GROUP,
                        $oManifest->get('base:path', '.', MANIFEST_SUBKEY_GROUP));
                    $oManifest->set('base:path', $strTablespaceLink, MANIFEST_SUBKEY_USER,
                        $oManifest->get('base:path', '.', MANIFEST_SUBKEY_USER));
                    $oManifest->set('base:path', $strTablespaceLink, MANIFEST_SUBKEY_MODE,
                        $oManifest->get('base:path', '.', MANIFEST_SUBKEY_MODE));

                    &log(INFO, "remapping tablespace ${strTablespaceKey} to ${strTablespacePath}");
                }
            }
        }
        # If tablespaces have been remapped, update the manifest
        elsif (defined($self->{oRemapRef}))
        {
            foreach my $strTablespaceKey (sort(keys(%{$self->{oRemapRef}})))
            {
                my $strRemapPath = ${$self->{oRemapRef}}{$strTablespaceKey};
                my $strPathKey = "tablespace/${strTablespaceKey}";

                # Make sure that the tablespace exists in the manifest
                if (!$oManifest->test(MANIFEST_SECTION_BACKUP_PATH, $strPathKey, MANIFEST_SUBKEY_LINK))
                {
                    confess &log(ERROR, "cannot remap invalid tablespace ${strTablespaceKey} to ${strRemapPath}");
                }

                # Remap the tablespace in the manifest
                &log(INFO, "remap tablespace ${strTablespaceKey} to ${strRemapPath}");

                my $strTablespaceLink = $oManifest->get(MANIFEST_SECTION_BACKUP_PATH, $strPathKey, MANIFEST_SUBKEY_LINK);

                $oManifest->set(MANIFEST_SECTION_BACKUP_PATH, $strPathKey, MANIFEST_SUBKEY_PATH, $strRemapPath);
                $oManifest->set('base:link', "pg_tblspc/${strTablespaceLink}", MANIFEST_SUBKEY_DESTINATION, $strRemapPath);
            }
        }

        $self->manifest_ownership_check($oManifest);

        return $oManifest;
    }

    confess &log(ERROR, 'backup ' . $self->{strBackupPath} . ' does not exist');
}

####################################################################################################################################
# CLEAN
#
# Checks that the restore paths are empty, or if --force was used then it cleans files/paths/links from the restore directories that
# are not present in the manifest.
####################################################################################################################################
sub clean
{
    my $self = shift;               # Class hash
    my $oManifest = shift;       # Backup manifest

    # Track if files/links/paths where removed
    my %oRemoveHash = (&MANIFEST_FILE => 0, &MANIFEST_PATH => 0, &MANIFEST_LINK => 0);

    # Check each restore directory in the manifest and make sure that it exists and is empty.
    # The --force option can be used to override the empty requirement.
    foreach my $strPathKey ($oManifest->keys(MANIFEST_SECTION_BACKUP_PATH))
    {
        my $strPath = $oManifest->get(MANIFEST_SECTION_BACKUP_PATH, $strPathKey, MANIFEST_SUBKEY_PATH);

        &log(INFO, "checking/cleaning db path ${strPath}");

        if (!$self->{oFile}->exists(PATH_DB_ABSOLUTE, $strPath))
        {
            next;
        }

        # Load path manifest so it can be compared to deleted files/paths/links that are not in the backup
        my %oPathManifest;
        $self->{oFile}->manifest(PATH_DB_ABSOLUTE, $strPath, \%oPathManifest);

        foreach my $strName (sort {$b cmp $a} (keys(%{$oPathManifest{name}})))
        {
            # Skip the root path
            if ($strName eq '.')
            {
                next;
            }

            # If force was not specified then error if any file is found
            if (!$self->{bForce} && !$self->{bDelta})
            {
                confess &log(ERROR, "cannot restore to path '${strPath}' that contains files - " .
                                    'try using --delta if this is what you intended', ERROR_RESTORE_PATH_NOT_EMPTY);
            }

            my $strFile = "${strPath}/${strName}";

            # Determine the file/path/link type
            my $strType = MANIFEST_FILE;

            if ($oPathManifest{name}{$strName}{type} eq 'd')
            {
                $strType = MANIFEST_PATH;
            }
            elsif ($oPathManifest{name}{$strName}{type} eq 'l')
            {
                $strType = MANIFEST_LINK;
            }

            # Build the section name
            my $strSection = "${strPathKey}:${strType}";

            # Check to see if the file/path/link exists in the manifest
            if ($oManifest->test($strSection, $strName))
            {
                my $strUser = $oManifest->get($strSection, $strName, MANIFEST_SUBKEY_USER);
                my $strGroup = $oManifest->get($strSection, $strName, MANIFEST_SUBKEY_GROUP);

                # If ownership does not match, fix it
                if ($strUser ne $oPathManifest{name}{$strName}{user} ||
                    $strGroup ne $oPathManifest{name}{$strName}{group})
                {
                    &log(INFO, "setting ${strFile} ownership to ${strUser}:${strGroup}");

                    $self->{oFile}->owner(PATH_DB_ABSOLUTE, $strFile, $strUser, $strGroup);
                }

                # If a link does not have the same destination, then delete it (it will be recreated later)
                if ($strType eq MANIFEST_LINK)
                {
                    if ($strType eq MANIFEST_LINK && $oManifest->get($strSection, $strName, MANIFEST_SUBKEY_DESTINATION) ne
                        $oPathManifest{name}{$strName}{link_destination})
                    {
                        &log(INFO, "removing link ${strFile} - destination changed");
                        unlink($strFile) or confess &log(ERROR, "unable to delete file ${strFile}");
                    }
                }
                # Else if file/path mode does not match, fix it
                else
                {
                    my $strMode = $oManifest->get($strSection, $strName, MANIFEST_SUBKEY_MODE);

                    if ($strType ne MANIFEST_LINK && $strMode ne $oPathManifest{name}{$strName}{mode})
                    {
                        &log(INFO, "setting ${strFile} mode to ${strMode}");

                        chmod(oct($strMode), $strFile)
                            or confess 'unable to set mode ${strMode} for ${strFile}';
                    }
                }
            }
            # If it does not then remove it
            else
            {
                # If a path then remove it, all the files should have already been deleted since we are going in reverse order
                if ($strType eq MANIFEST_PATH)
                {
                    &log(INFO, "removing path ${strFile}");
                    rmdir($strFile) or confess &log(ERROR, "unable to delete path ${strFile}, is it empty?");
                }
                # Else delete a file/link
                else
                {
                    # Delete only if this is not the recovery.conf file.  This is in case the user wants the recovery.conf file
                    # preserved.  It will be written/deleted/preserved as needed in recovery().
                    if (!($strName eq FILE_RECOVERY_CONF && $strType eq MANIFEST_FILE))
                    {
                        &log(INFO, "removing file/link ${strFile}");
                        unlink($strFile) or confess &log(ERROR, "unable to delete file/link ${strFile}");
                    }
                }

                $oRemoveHash{$strType} += 1;
            }
        }
    }

    # Loop through types (path, link, file) and emit info if any were removed
    foreach my $strFileType (sort (keys %oRemoveHash))
    {
        if ($oRemoveHash{$strFileType} > 0)
        {
            &log(INFO, "$oRemoveHash{$strFileType} ${strFileType}(s) removed during cleanup");
        }
    }
}

####################################################################################################################################
# BUILD
#
# Creates missing paths and links and corrects ownership/mode on existing paths and links.
####################################################################################################################################
sub build
{
    my $self = shift;            # Class hash
    my $oManifest = shift;       # Backup manifest

    # Build paths/links in each restore path
    foreach my $strSectionPathKey ($oManifest->keys(MANIFEST_SECTION_BACKUP_PATH))
    {
        my $strSectionPath = $oManifest->get(MANIFEST_SECTION_BACKUP_PATH, $strSectionPathKey, MANIFEST_SUBKEY_PATH);

        # Create all paths in the manifest that do not already exist
        my $strSection = "${strSectionPathKey}:path";

        foreach my $strName ($oManifest->keys($strSection))
        {
            # Skip the root path
            if ($strName eq '.')
            {
                next;
            }

            # Create the Path
            my $strPath = "${strSectionPath}/${strName}";

            if (!$self->{oFile}->exists(PATH_DB_ABSOLUTE, $strPath))
            {
                $self->{oFile}->path_create(PATH_DB_ABSOLUTE, $strPath,
                                            $oManifest->get($strSection, $strName, MANIFEST_SUBKEY_MODE));
            }
        }

        # Create all links in the manifest that do not already exist
        $strSection = "${strSectionPathKey}:link";

        if ($oManifest->test($strSection))
        {
            foreach my $strName ($oManifest->keys($strSection))
            {
                my $strLink = "${strSectionPath}/${strName}";

                if (!$self->{oFile}->exists(PATH_DB_ABSOLUTE, $strLink))
                {
                    $self->{oFile}->link_create(PATH_DB_ABSOLUTE,
                                                $oManifest->get($strSection, $strName, MANIFEST_SUBKEY_DESTINATION),
                                                PATH_DB_ABSOLUTE, $strLink);
                }
            }
        }
    }

    # Make sure that all paths required for the restore now exist
    foreach my $strPathKey ($oManifest->keys(MANIFEST_SECTION_BACKUP_PATH))
    {
        my $strPath = $oManifest->get(MANIFEST_SECTION_BACKUP_PATH, $strPathKey, MANIFEST_SUBKEY_PATH);

        if (!$self->{oFile}->exists(PATH_DB_ABSOLUTE,  $strPath))
        {
            confess &log(ERROR, "required db path '${strPath}' does not exist");
        }
    }
}

####################################################################################################################################
# RECOVERY
#
# Creates the recovery.conf file.
####################################################################################################################################
sub recovery
{
    my $self = shift;       # Class hash

    # Create recovery.conf path/file
    my $strRecoveryConf = $self->{strDbClusterPath} . '/' . FILE_RECOVERY_CONF;

    # See if recovery.conf already exists
    my $bRecoveryConfExists = $self->{oFile}->exists(PATH_DB_ABSOLUTE, $strRecoveryConf);

    # If RECOVERY_TYPE_PRESERVE then warn if recovery.conf does not exist and return
    if ($self->{strType} eq RECOVERY_TYPE_PRESERVE)
    {
        if (!$bRecoveryConfExists)
        {
            &log(WARN, "recovery type is $self->{strType} but recovery file does not exist at ${strRecoveryConf}");
        }

        return;
    }

    # In all other cases the old recovery.conf should be removed if it exists
    if ($bRecoveryConfExists)
    {
        $self->{oFile}->remove(PATH_DB_ABSOLUTE, $strRecoveryConf);
    }

    # If RECOVERY_TYPE_NONE then return
    if ($self->{strType} eq RECOVERY_TYPE_NONE)
    {
        return;
    }

    # Write recovery options read from the configuration file
    my $strRecovery = '';
    my $bRestoreCommandOverride = false;

    if (defined($self->{oRecoveryRef}))
    {
        foreach my $strKey (sort(keys(%{$self->{oRecoveryRef}})))
        {
            my $strPgKey = $strKey;
            $strPgKey =~ s/\-/\_/g;

            if ($strPgKey eq 'restore_command')
            {
                $bRestoreCommandOverride = true;
            }

            $strRecovery .= "$strPgKey = '${$self->{oRecoveryRef}}{$strKey}'\n";
        }
    }

    # Write the restore command
    if (!$bRestoreCommandOverride)
    {
        $strRecovery .=  "restore_command = '" . commandWrite(CMD_ARCHIVE_GET) . " %f \"%p\"'\n";
    }

    # If RECOVERY_TYPE_DEFAULT do not write target options
    if ($self->{strType} ne RECOVERY_TYPE_DEFAULT)
    {
        # Write the recovery target
        $strRecovery .= "recovery_target_$self->{strType} = '$self->{strTarget}'\n";

        # Write recovery_target_inclusive
        if ($self->{bTargetExclusive})
        {
            $strRecovery .= "recovery_target_inclusive = 'false'\n";
        }
    }

    # Write pause_at_recovery_target
    if ($self->{bTargetResume})
    {
        $strRecovery .= "pause_at_recovery_target = 'false'\n";
    }

    # Write recovery_target_timeline
    if (defined($self->{strTargetTimeline}))
    {
        $strRecovery .= "recovery_target_timeline = '$self->{strTargetTimeline}'\n";
    }

    # Write recovery.conf
    my $hFile;

    open($hFile, '>', $strRecoveryConf)
        or confess &log(ERROR, "unable to open ${strRecoveryConf}: $!");

    syswrite($hFile, $strRecovery)
        or confess "unable to write section ${strRecoveryConf}: $!";

    close($hFile)
        or confess "unable to close ${strRecoveryConf}: $!";

    &log(INFO, "wrote $strRecoveryConf");
}

####################################################################################################################################
# RESTORE
#
# Takes a backup and restores it back to the original or a remapped location.
####################################################################################################################################
sub restore
{
    my $self = shift;       # Class hash

    # Make sure that Postgres is not running
    if ($self->{oFile}->exists(PATH_DB_ABSOLUTE, $self->{strDbClusterPath} . '/' . FILE_POSTMASTER_PID))
    {
        confess &log(ERROR, 'unable to restore while Postgres is running', ERROR_POSTMASTER_RUNNING);
    }

    # Log the backup set to restore
    &log(INFO, "restore backup set " . $self->{strBackupPath});

    # Make sure the backup path is valid and load the manifest
    my $oManifest = $self->manifest_load();

    # Delete pg_control file.  This will be copied from the backup at the very end to prevent a partially restored database
    # from being started.
    $self->{oFile}->remove(PATH_DB_ABSOLUTE,
                           $oManifest->get(MANIFEST_SECTION_BACKUP_PATH, MANIFEST_KEY_BASE, MANIFEST_SUBKEY_PATH) .
                           '/' . FILE_PG_CONTROL);

    # Clean the restore paths
    $self->clean($oManifest);

    # Build paths/links in the restore paths
    $self->build($oManifest);

    # Get variables required for restore
    my $lCopyTimeBegin = $oManifest->getNumeric(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TIMESTAMP_COPY_START);
    my $bSourceCompression = $oManifest->getBool(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_COMPRESS);
    my $strCurrentUser = getpwuid($<);
    my $strCurrentGroup = getgrgid($();

    # Create hash containing files to restore
    my %oRestoreHash;
    my $lSizeTotal = 0;
    my $lSizeCurrent = 0;

    foreach my $strPathKey ($oManifest->keys(MANIFEST_SECTION_BACKUP_PATH))
    {
        my $strSection = "${strPathKey}:file";

        if ($oManifest->test($strSection))
        {
            foreach my $strFile ($oManifest->keys($strSection))
            {
                my $lSize = $oManifest->getNumeric($strSection, $strFile, MANIFEST_SUBKEY_SIZE);
                $lSizeTotal += $lSize;

                # Preface the file key with the size. This allows for sorting the files to restore by size.
                # Skip this for global/pg_control since it will be copied last.
                my $strFileKey;

                if ($strPathKey eq MANIFEST_KEY_BASE && $strFile eq FILE_PG_CONTROL)
                {
                    $strFileKey = $strFile;
                }
                else
                {
                    $strFileKey = sprintf("%016d-${strFile}", $lSize);
                }

                # Get restore information
                $oRestoreHash{$strPathKey}{$strFileKey}{file} = $strFile;
                $oRestoreHash{$strPathKey}{$strFileKey}{size} = $lSize;
                $oRestoreHash{$strPathKey}{$strFileKey}{source_path} = $strPathKey;
                $oRestoreHash{$strPathKey}{$strFileKey}{destination_path} =
                    $oManifest->get(MANIFEST_SECTION_BACKUP_PATH, $strPathKey, MANIFEST_SUBKEY_PATH);
                $oRestoreHash{$strPathKey}{$strFileKey}{reference} =
                    $oManifest->testBool(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_HARDLINK, undef, true) ? undef :
                    $oManifest->get($strSection, $strFile, MANIFEST_SUBKEY_REFERENCE, false);
                $oRestoreHash{$strPathKey}{$strFileKey}{modification_time} =
                    $oManifest->getNumeric($strSection, $strFile, MANIFEST_SUBKEY_TIMESTAMP);
                $oRestoreHash{$strPathKey}{$strFileKey}{mode} =
                    $oManifest->get($strSection, $strFile, MANIFEST_SUBKEY_MODE);
                $oRestoreHash{$strPathKey}{$strFileKey}{user} =
                    $oManifest->get($strSection, $strFile, MANIFEST_SUBKEY_USER);
                $oRestoreHash{$strPathKey}{$strFileKey}{group} =
                    $oManifest->get($strSection, $strFile, MANIFEST_SUBKEY_GROUP);

                # Checksum is only stored if size > 0
                if ($lSize > 0)
                {
                    $oRestoreHash{$strPathKey}{$strFileKey}{checksum} =
                        $oManifest->get($strSection, $strFile, MANIFEST_SUBKEY_CHECKSUM);
                }
            }
        }
    }

    # If multi-threaded then create threads to copy files
    if ($self->{iThreadTotal} > 1)
    {
        &log(DEBUG, "starting restore with $self->{iThreadTotal} threads");

        # Initialize the thread queues
        my @oyRestoreQueue;

        foreach my $strPathKey (sort (keys %oRestoreHash))
        {
            push(@oyRestoreQueue, Thread::Queue->new());

            foreach my $strFileKey (sort {$b cmp $a} (keys(%{$oRestoreHash{$strPathKey}})))
            {
                # Skip pg_control
                if ($strPathKey eq MANIFEST_KEY_BASE && $strFileKey eq FILE_PG_CONTROL)
                {
                    next;
                }

                $oyRestoreQueue[@oyRestoreQueue - 1]->enqueue($oRestoreHash{$strPathKey}{$strFileKey});
            }
        }

        # Initialize the param hash
        my %oParam;

        $oParam{copy_time_begin} = $lCopyTimeBegin;
        $oParam{size_total} = $lSizeTotal;
        $oParam{delta} = $self->{bDelta};
        $oParam{force} = $self->{bForce};
        $oParam{backup_path} = $self->{strBackupPath};
        $oParam{source_compression} = $bSourceCompression;
        $oParam{current_user} = $strCurrentUser;
        $oParam{current_group} = $strCurrentGroup;
        $oParam{queue} = \@oyRestoreQueue;

        # Run the threads
        for (my $iThreadIdx = 0; $iThreadIdx < $self->{iThreadTotal}; $iThreadIdx++)
        {
            threadGroupRun($iThreadIdx, 'restore', \%oParam);
        }

        # Complete thread queues
        while (!threadGroupComplete()) {};
    }
    else
    {
        &log(DEBUG, "starting restore in main process");

        # Restore file in main process
        foreach my $strPathKey (sort (keys %oRestoreHash))
        {
            foreach my $strFileKey (sort {$b cmp $a} (keys(%{$oRestoreHash{$strPathKey}})))
            {
                # Skip pg_control
                if ($strPathKey eq MANIFEST_KEY_BASE && $strFileKey eq FILE_PG_CONTROL)
                {
                    next;
                }

                $lSizeCurrent = restoreFile($oRestoreHash{$strPathKey}{$strFileKey}, $lCopyTimeBegin, $self->{bDelta},
                                            $self->{bForce}, $self->{strBackupPath}, $bSourceCompression, $strCurrentUser,
                                            $strCurrentGroup, $self->{oFile}, $lSizeTotal, $lSizeCurrent);
            }
        }
    }

    # Create recovery.conf file
    $self->recovery();

    # Copy pg_control last
    &log(INFO, 'restore ' . FILE_PG_CONTROL . ' (copied last to ensure aborted restores cannot be started)');

    restoreFile($oRestoreHash{&MANIFEST_KEY_BASE}{&FILE_PG_CONTROL}, $lCopyTimeBegin, $self->{bDelta}, $self->{bForce},
                $self->{strBackupPath}, $bSourceCompression, $strCurrentUser, $strCurrentGroup, $self->{oFile}, $lSizeTotal,
                $lSizeCurrent);

    &log(INFO, 'restore complete');
}

1;
