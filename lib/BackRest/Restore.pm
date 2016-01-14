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
use BackRest::Common::Exception;
use BackRest::Common::Log;
use BackRest::Config::Config;
use BackRest::Db;
use BackRest::File;
use BackRest::Manifest;
use BackRest::RestoreFile;

####################################################################################################################################
# Operation constants
####################################################################################################################################
use constant OP_RESTORE                                             => 'Restore';

use constant OP_RESTORE_BUILD                                       => OP_RESTORE . '->build';
use constant OP_RESTORE_CLEAN                                       => OP_RESTORE . '->clean';
use constant OP_RESTORE_DESTROY                                     => OP_RESTORE . '->destroy';
use constant OP_RESTORE_MANIFEST_LOAD                               => OP_RESTORE . '->manifestLoad';
use constant OP_RESTORE_MANIFEST_OWNERSHIP_CHECK                    => OP_RESTORE . '->manifestOwnershipCheck';
use constant OP_RESTORE_NEW                                         => OP_RESTORE . '->new';
use constant OP_RESTORE_PROCESS                                     => OP_RESTORE . '->process';
use constant OP_RESTORE_RECOVERY                                    => OP_RESTORE . '->recovery';

####################################################################################################################################
# File constants
####################################################################################################################################
# Conf file with settings for recovery after restore
use constant FILE_RECOVERY_CONF                                     => 'recovery.conf';

# Tablespace map introduced in 9.5
use constant FILE_TABLESPACE_MAP                                    => 'tablespace_map';

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
    my ($strOperation) = logDebugParam(OP_RESTORE_NEW);

    # Initialize default file object
    $self->{oFile} = new BackRest::File
    (
        optionGet(OPTION_STANZA),
        optionRemoteTypeTest(BACKUP) ? optionGet(OPTION_REPO_REMOTE_PATH) : optionGet(OPTION_REPO_PATH),
        optionRemoteType(),
        protocolGet()
    );

    # Initialize variables
    $self->{strDbClusterPath} = optionGet(OPTION_DB_PATH);
    $self->{strBackupSet} = optionGet(OPTION_SET);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# DESTROY
####################################################################################################################################
sub DESTROY
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation
    ) =
        logDebugParam
    (
        OP_RESTORE_DESTROY
    );

    undef($self->{oFile});

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation
    );
}

####################################################################################################################################
# manifestOwnershipCheck
#
# Checks the users and groups that exist in the manifest and emits warnings for ownership that cannot be set properly, either
# because the current user does not have permissions or because the user/group does not exist.
####################################################################################################################################
sub manifestOwnershipCheck
{
    my $self = shift;               # Class hash

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oManifest                                  # Backup manifest
    ) =
        logDebugParam
        (
            OP_RESTORE_MANIFEST_OWNERSHIP_CHECK, \@_,
            {name => 'oManifest', trace => true}
        );

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

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# manifestLoad
#
# Loads the backup manifest and performs requested tablespace remaps.
####################################################################################################################################
sub manifestLoad
{
    my $self = shift;           # Class hash

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam (OP_RESTORE_MANIFEST_LOAD);

    # Error if the backup set does not exist
    if (!$self->{oFile}->exists(PATH_BACKUP_CLUSTER, $self->{strBackupSet}))
    {
        confess &log(ERROR, 'backup ' . $self->{strBackupSet} . ' does not exist');
    }

    # Copy the backup manifest to the db cluster path
    $self->{oFile}->copy(PATH_BACKUP_CLUSTER, $self->{strBackupSet} . '/' . FILE_MANIFEST,
                         PATH_DB_ABSOLUTE, $self->{strDbClusterPath} . '/' . FILE_MANIFEST);

    # Load the manifest into a hash
    my $oManifest = new BackRest::Manifest($self->{oFile}->pathGet(PATH_DB_ABSOLUTE,
                                                                   $self->{strDbClusterPath} . '/' . FILE_MANIFEST));

    # Remove the manifest now that it is in memory
    $self->{oFile}->remove(PATH_DB_ABSOLUTE, $self->{strDbClusterPath} . '/' . FILE_MANIFEST);

    # If backup is latest then set it equal to backup label, else verify that requested backup and label match
    my $strBackupLabel = $oManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_LABEL);

    if ($self->{strBackupSet} eq OPTION_DEFAULT_RESTORE_SET)
    {
        $self->{strBackupSet} = $strBackupLabel;
    }
    elsif ($self->{strBackupSet} ne $strBackupLabel)
    {
        confess &log(ASSERT, "request backup $self->{strBackupSet} and label ${strBackupLabel} do not match " .
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

                &log(INFO, "remap tablespace ${strTablespaceKey} to ${strTablespacePath}");
            }
        }
    }
    # If tablespaces have been remapped, update the manifest
    elsif (optionTest(OPTION_RESTORE_TABLESPACE_MAP))
    {
        my $oRemapRef = optionGet(OPTION_RESTORE_TABLESPACE_MAP);

        foreach my $strTablespaceKey (sort(keys(%$oRemapRef)))
        {
            my $strRemapPath = $$oRemapRef{$strTablespaceKey};
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

    $self->manifestOwnershipCheck($oManifest);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oManifest', value => $oManifest, trace => true}
    );
}

####################################################################################################################################
# clean
#
# Checks that the restore paths are empty, or if --force was used then it cleans files/paths/links from the restore directories that
# are not present in the manifest.
####################################################################################################################################
sub clean
{
    my $self = shift;               # Class hash

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oManifest                                  # Backup manifest

    ) =
        logDebugParam
        (
            OP_RESTORE_CLEAN, \@_,
            {name => 'oManifest', trace => true}
        );

    # Track if files/links/paths where removed
    my %oRemoveHash = (&MANIFEST_FILE => 0, &MANIFEST_PATH => 0, &MANIFEST_LINK => 0);

    # Check each restore directory in the manifest and make sure that it exists and is empty.
    # The --force option can be used to override the empty requirement.
    foreach my $strPathKey ($oManifest->keys(MANIFEST_SECTION_BACKUP_PATH))
    {
        my $strPath = $oManifest->get(MANIFEST_SECTION_BACKUP_PATH, $strPathKey, MANIFEST_SUBKEY_PATH);

        &log(INFO, "check/clean db path ${strPath}");

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
            if (!optionGet(OPTION_FORCE) && !optionGet(OPTION_DELTA))
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
                    &log(INFO, "set ownership ${strUser}:${strGroup} on ${strFile}");

                    $self->{oFile}->owner(PATH_DB_ABSOLUTE, $strFile, $strUser, $strGroup);
                }

                # If a link does not have the same destination, then delete it (it will be recreated later)
                if ($strType eq MANIFEST_LINK)
                {
                    if ($strType eq MANIFEST_LINK && $oManifest->get($strSection, $strName, MANIFEST_SUBKEY_DESTINATION) ne
                        $oPathManifest{name}{$strName}{link_destination})
                    {
                        &log(INFO, "remove link ${strFile} - destination changed");

                        unlink($strFile)
                            or confess &log(ERROR, "unable to remove link ${strFile}");
                    }
                }
                # Else if file/path mode does not match, fix it
                else
                {
                    my $strMode = $oManifest->get($strSection, $strName, MANIFEST_SUBKEY_MODE);

                    if ($strType ne MANIFEST_LINK && $strMode ne $oPathManifest{name}{$strName}{mode})
                    {
                        &log(INFO, "set mode ${strMode} on ${strFile}");

                        chmod(oct($strMode), $strFile)
                            or confess 'unable to set mode ${strMode} on ${strFile}';
                    }
                }
            }
            # If it does not then remove it
            else
            {
                # If a path then remove it, all the files should have already been deleted since we are going in reverse order
                if ($strType eq MANIFEST_PATH)
                {
                    &log(INFO, "remove path ${strFile}");
                    rmdir($strFile) or confess &log(ERROR, "unable to delete path ${strFile}, is it empty?");
                }
                # Else delete a file/link
                else
                {
                    # Delete only if this is not the recovery.conf file.  This is in case the user wants the recovery.conf file
                    # preserved.  It will be written/deleted/preserved as needed in recovery().
                    if (!($strName eq FILE_RECOVERY_CONF && $strType eq MANIFEST_FILE))
                    {
                        &log(INFO, "remove file/link ${strFile}");
                        unlink($strFile) or confess &log(ERROR, "unable to delete file/link ${strFile}");
                    }
                }

                $oRemoveHash{$strType} += 1;
            }
        }
    }

    # Loop through types (path, link, file) and emit info if any were removed
    my @stryMessage;

    foreach my $strFileType (sort (keys %oRemoveHash))
    {
        if ($oRemoveHash{$strFileType} > 0)
        {
            push(@stryMessage, "$oRemoveHash{$strFileType} ${strFileType}" . ($oRemoveHash{$strFileType} > 1 ? 's' : ''));
        }
    }

    if (@stryMessage)
    {
        &log(INFO, 'cleanup removed ' . join(', ', @stryMessage));
    }

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# build
#
# Creates missing paths and links and corrects ownership/mode on existing paths and links.
####################################################################################################################################
sub build
{
    my $self = shift;            # Class hash

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oManifest                                  # Backup manifest

    ) =
        logDebugParam
        (
            OP_RESTORE_BUILD, \@_,
            {name => 'oManifest', trace => true}
        );

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
                $self->{oFile}->pathCreate(PATH_DB_ABSOLUTE, $strPath,
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
                    $self->{oFile}->linkCreate(PATH_DB_ABSOLUTE,
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

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# recovery
#
# Creates the recovery.conf file.
####################################################################################################################################
sub recovery
{
    my $self = shift;       # Class hash
    my $fDbVersion = shift; # Version to restore

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam (OP_RESTORE_RECOVERY);

    # Create recovery.conf path/file
    my $strRecoveryConf = $self->{strDbClusterPath} . '/' . FILE_RECOVERY_CONF;

    # See if recovery.conf already exists
    my $bRecoveryConfExists = $self->{oFile}->exists(PATH_DB_ABSOLUTE, $strRecoveryConf);

    # If RECOVERY_TYPE_PRESERVE then warn if recovery.conf does not exist and return
    if (optionTest(OPTION_TYPE, RECOVERY_TYPE_PRESERVE))
    {
        if (!$bRecoveryConfExists)
        {
            &log(WARN, "recovery type is " . optionGet(OPTION_TYPE) . " but recovery file does not exist at ${strRecoveryConf}");
        }
    }
    else
    {
        # In all other cases the old recovery.conf should be removed if it exists
        if ($bRecoveryConfExists)
        {
            $self->{oFile}->remove(PATH_DB_ABSOLUTE, $strRecoveryConf);
        }

        # If RECOVERY_TYPE_NONE then return
        if (!optionTest(OPTION_TYPE, RECOVERY_TYPE_NONE))
        {
            # Write recovery options read from the configuration file
            my $strRecovery = '';
            my $bRestoreCommandOverride = false;

            if (optionTest(OPTION_RESTORE_RECOVERY_OPTION))
            {
                my $oRecoveryRef = optionGet(OPTION_RESTORE_RECOVERY_OPTION);

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
                $strRecovery .=  "restore_command = '" . commandWrite(CMD_ARCHIVE_GET) . " %f \"%p\"'\n";
            }

            # If type is RECOVERY_TYPE_IMMEDIATE
            if (optionTest(OPTION_TYPE, RECOVERY_TYPE_IMMEDIATE))
            {
                $strRecovery .= "recovery_target = '" . RECOVERY_TYPE_IMMEDIATE . "'\n";
            }
            # If type is not RECOVERY_TYPE_DEFAULT write target options
            elsif (!optionTest(OPTION_TYPE, RECOVERY_TYPE_DEFAULT))
            {
                # Write the recovery target
                $strRecovery .= "recovery_target_" . optionGet(OPTION_TYPE) . " = '" . optionGet(OPTION_TARGET) . "'\n";

                # Write recovery_target_inclusive
                if (optionGet(OPTION_TARGET_EXCLUSIVE, false))
                {
                    $strRecovery .= "recovery_target_inclusive = 'false'\n";
                }
            }

            # Write pause_at_recovery_target
            if (optionTest(OPTION_TARGET_ACTION))
            {
                my $strTargetAction = optionGet(OPTION_TARGET_ACTION);

                if ($strTargetAction ne OPTION_DEFAULT_RESTORE_TARGET_ACTION)
                {
                    if ($fDbVersion >= 9.5)
                    {
                        $strRecovery .= "recovery_target_action = '${strTargetAction}'\n";
                    }
                    elsif  ($fDbVersion >= 9.1)
                    {
                        $strRecovery .= "pause_at_recovery_target = 'false'\n";
                    }
                    else
                    {
                        confess &log(ERROR, OPTION_TARGET_ACTION .  ' option is only available in PostgreSQL >= 9.1')
                    }
                }
            }

            # Write recovery_target_timeline
            if (optionTest(OPTION_TARGET_TIMELINE))
            {
                $strRecovery .= "recovery_target_timeline = '" . optionGet(OPTION_TARGET_TIMELINE) . "'\n";
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
    my ($strOperation) = logDebugParam (OP_RESTORE_PROCESS);

    # Make sure that Postgres is not running
    if ($self->{oFile}->exists(PATH_DB_ABSOLUTE, $self->{strDbClusterPath} . '/' . FILE_POSTMASTER_PID))
    {
        confess &log(ERROR, 'unable to restore while Postgres is running', ERROR_POSTMASTER_RUNNING);
    }

    # Log the backup set to restore
    &log(INFO, "restore backup set " . $self->{strBackupSet});

    # Make sure the backup path is valid and load the manifest
    my $oManifest = $self->manifestLoad();

    # Delete pg_control file.  This will be copied from the backup at the very end to prevent a partially restored database
    # from being started by PostgreSQL.
    $self->{oFile}->remove(PATH_DB_ABSOLUTE,
                           $oManifest->get(MANIFEST_SECTION_BACKUP_PATH, MANIFEST_KEY_BASE, MANIFEST_SUBKEY_PATH) .
                           '/' . FILE_PG_CONTROL);

    # Clean the restore paths
    $self->clean($oManifest);

    # Build paths/links in the restore paths
    $self->build($oManifest);

    # Get variables required for restore
    my $lCopyTimeBegin = $oManifest->numericGet(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TIMESTAMP_COPY_START);
    my $bSourceCompression = $oManifest->boolGet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_COMPRESS);
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
                # Skip the tablespace_map file in versions >= 9.5 so Postgres does not rewrite links in the pg_tblspc directory.
                # The tablespace links have already been created by Restore::build().
                if ($strPathKey eq MANIFEST_KEY_BASE && $strFile eq FILE_TABLESPACE_MAP &&
                    $oManifest->get(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_DB_VERSION) >= 9.5)
                {
                    next;
                }

                my $lSize = $oManifest->numericGet($strSection, $strFile, MANIFEST_SUBKEY_SIZE);
                $lSizeTotal += $lSize;

                # Preface the file key with the size. This allows for sorting the files to restore by size.
                my $strFileKey;

                # Skip this for global/pg_control since it will be copied as the last step and needs to named in a way that it
                # can be found for the copy.
                if ($strPathKey eq MANIFEST_KEY_BASE && $strFile eq FILE_PG_CONTROL)
                {
                    $strFileKey = $strFile;
                    $oRestoreHash{$strPathKey}{$strFileKey}{skip} = true;
                }
                # Else continue normally
                else
                {
                    $strFileKey = sprintf("%016d-${strFile}", $lSize);
                    $oRestoreHash{$strPathKey}{$strFileKey}{skip} = false;
                }

                # Get restore information
                $oRestoreHash{$strPathKey}{$strFileKey}{file} = $strFile;
                $oRestoreHash{$strPathKey}{$strFileKey}{size} = $lSize;
                $oRestoreHash{$strPathKey}{$strFileKey}{source_path} = $strPathKey;
                $oRestoreHash{$strPathKey}{$strFileKey}{destination_path} =
                    $oManifest->get(MANIFEST_SECTION_BACKUP_PATH, $strPathKey, MANIFEST_SUBKEY_PATH);
                $oRestoreHash{$strPathKey}{$strFileKey}{reference} =
                    $oManifest->boolTest(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_HARDLINK, undef, true) ? undef :
                    $oManifest->get($strSection, $strFile, MANIFEST_SUBKEY_REFERENCE, false);
                $oRestoreHash{$strPathKey}{$strFileKey}{modification_time} =
                    $oManifest->numericGet($strSection, $strFile, MANIFEST_SUBKEY_TIMESTAMP);
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
    if (optionGet(OPTION_THREAD_MAX) > 1)
    {
        # Load module dynamically
        require BackRest::Protocol::ThreadGroup;
        BackRest::Protocol::ThreadGroup->import();


        logDebugMisc
        (
            $strOperation, 'restore with threads',
            {name => 'iThreadTotal', value => optionGet(OPTION_THREAD_MAX)}
        );

        # Initialize the thread queues
        my @oyRestoreQueue;

        foreach my $strPathKey (sort (keys %oRestoreHash))
        {
            push(@oyRestoreQueue, Thread::Queue->new());

            foreach my $strFileKey (sort {$b cmp $a} (keys(%{$oRestoreHash{$strPathKey}})))
            {
                # Skip files marked to be copied later
                next if ($oRestoreHash{$strPathKey}{$strFileKey}{skip});

                $oyRestoreQueue[@oyRestoreQueue - 1]->enqueue($oRestoreHash{$strPathKey}{$strFileKey});
            }
        }

        # Initialize the param hash
        my %oParam;

        $oParam{copy_time_begin} = $lCopyTimeBegin;
        $oParam{size_total} = $lSizeTotal;
        $oParam{delta} = optionGet(OPTION_DELTA);
        $oParam{force} = optionGet(OPTION_FORCE);
        $oParam{backup_path} = $self->{strBackupSet};
        $oParam{source_compression} = $bSourceCompression;
        $oParam{current_user} = $strCurrentUser;
        $oParam{current_group} = $strCurrentGroup;
        $oParam{queue} = \@oyRestoreQueue;

        # Run the threads
        for (my $iThreadIdx = 0; $iThreadIdx < optionGet(OPTION_THREAD_MAX); $iThreadIdx++)
        {
            threadGroupRun($iThreadIdx, 'restore', \%oParam);
        }

        # Complete thread queues
        while (!threadGroupComplete())
        {
            # Keep the protocol layer from timing out
            protocolGet()->keepAlive();
        };
    }
    else
    {
        logDebugMisc($strOperation, 'restore in main process');

        # Restore file in main process
        foreach my $strPathKey (sort (keys %oRestoreHash))
        {
            foreach my $strFileKey (sort {$b cmp $a} (keys(%{$oRestoreHash{$strPathKey}})))
            {
                # Skip files marked to be copied later
                next if ($oRestoreHash{$strPathKey}{$strFileKey}{skip});

                $lSizeCurrent = restoreFile($oRestoreHash{$strPathKey}{$strFileKey}, $lCopyTimeBegin, optionGet(OPTION_DELTA),
                                            optionGet(OPTION_FORCE), $self->{strBackupSet}, $bSourceCompression, $strCurrentUser,
                                            $strCurrentGroup, $self->{oFile}, $lSizeTotal, $lSizeCurrent);
            }
        }
    }

    # Create recovery.conf file
    $self->recovery($oManifest->get(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_DB_VERSION));

    # Copy pg_control last
    &log(INFO, 'restore ' . FILE_PG_CONTROL . ' (copied last to ensure aborted restores cannot be started)');

    restoreFile($oRestoreHash{&MANIFEST_KEY_BASE}{&FILE_PG_CONTROL}, $lCopyTimeBegin, optionGet(OPTION_DELTA),
                optionGet(OPTION_FORCE), $self->{strBackupSet}, $bSourceCompression, $strCurrentUser, $strCurrentGroup,
                $self->{oFile}, $lSizeTotal, $lSizeCurrent);

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

1;
