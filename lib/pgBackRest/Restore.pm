####################################################################################################################################
# RESTORE MODULE
####################################################################################################################################
package pgBackRest::Restore;

use threads;
use threads::shared;
use Thread::Queue;
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Cwd qw(abs_path);
use File::Basename qw(basename dirname);
use File::stat qw(lstat);

use lib dirname($0);
use pgBackRest::BackupInfo;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Config::Config;
use pgBackRest::Db;
use pgBackRest::File;
use pgBackRest::FileCommon;
use pgBackRest::Manifest;
use pgBackRest::RestoreFile;

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

    # Initialize protocol
    $self->{oProtocol} = protocolGet();

    # Initialize default file object
    $self->{oFile} = new pgBackRest::File
    (
        optionGet(OPTION_STANZA),
        optionGet(OPTION_REPO_PATH),
        optionRemoteType(),
        $self->{oProtocol}
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
            {name => 'oManifest'}
        );

    # Create hashes to track valid/invalid users/groups
    my %oOwnerHash = ();

    # Create hash for each type to be checked
    my %oFileTypeHash =
    (
        &MANIFEST_SECTION_TARGET_PATH => true,
        &MANIFEST_SECTION_TARGET_LINK => true,
        &MANIFEST_SECTION_TARGET_FILE => true
    );

    # Create hash for default owners (user, group) for when the owner in the manifest cannot be used because it does not exist or
    # was not mapped to a name during the original backup.  It's preferred to use the owner of the PGDATA directory but if that was
    # not valid in the original backup then the current user/group will be used as a last resort.
    my %oOwnerTypeHash;

    if ($oManifest->test(MANIFEST_SECTION_TARGET_PATH, MANIFEST_TARGET_PGDATA, MANIFEST_SUBKEY_USER) &&
        !$oManifest->boolTest(MANIFEST_SECTION_TARGET_PATH, MANIFEST_TARGET_PGDATA, MANIFEST_SUBKEY_USER, false))
    {
        $oOwnerTypeHash{&MANIFEST_SUBKEY_USER} =
            $oManifest->get(MANIFEST_SECTION_TARGET_PATH, MANIFEST_TARGET_PGDATA, MANIFEST_SUBKEY_USER);
    }
    else
    {
        $oOwnerTypeHash{&MANIFEST_SUBKEY_USER} = getpwuid($<);
    }

    if ($oManifest->test(MANIFEST_SECTION_TARGET_PATH, MANIFEST_TARGET_PGDATA, MANIFEST_SUBKEY_GROUP) &&
        !$oManifest->boolTest(MANIFEST_SECTION_TARGET_PATH, MANIFEST_TARGET_PGDATA, MANIFEST_SUBKEY_GROUP, false))
    {
        $oOwnerTypeHash{&MANIFEST_SUBKEY_GROUP} =
            $oManifest->get(MANIFEST_SECTION_TARGET_PATH, MANIFEST_TARGET_PGDATA, MANIFEST_SUBKEY_GROUP);
    }
    else
    {
        $oOwnerTypeHash{&MANIFEST_SUBKEY_USER} = getgrgid($();
    }

    # Loop through owner types (user, group)
    foreach my $strOwnerType (sort (keys %oOwnerTypeHash))
    {
        # Loop through types (path, link, file)
        foreach my $strSection (sort (keys %oFileTypeHash))
        {
            foreach my $strName ($oManifest->keys($strSection))
            {
                my $strOwner = $oManifest->get($strSection, $strName, $strOwnerType);

                # If the owner was invalid then set it to something valid
                if ($oManifest->boolTest($strSection, $strName, $strOwnerType, false))
                {
                    $strOwner = $oOwnerTypeHash{$strOwnerType};

                    &log(WARN, "backup ${strOwnerType} for ${strName} was not mapped to a name, set to ${strOwner}");
                    $oManifest->set($strSection, $strName, $strOwnerType, $strOwner);
                }

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

        # Output warning for any invalid owners
        if (defined($oOwnerHash{$strOwnerType}))
        {
            foreach my $strOwner (sort (keys(%{$oOwnerHash{$strOwnerType}})))
            {
                if (!$oOwnerHash{$strOwnerType}{$strOwner})
                {
                    &log(WARN, "${strOwnerType} ${strOwner} in manifest " . ($< == 0 ? 'does not exist locally ' : '') .
                               "cannot be used for restore, set to $oOwnerTypeHash{$strOwnerType}");
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
    $self->{oFile}->copy(PATH_BACKUP_CLUSTER, "$self->{strBackupSet}/" . FILE_MANIFEST,
                         PATH_DB_ABSOLUTE, $self->{strDbClusterPath} . '/' . FILE_MANIFEST);

    # Load the manifest into a hash
    my $oManifest = new pgBackRest::Manifest($self->{oFile}->pathGet(PATH_DB_ABSOLUTE,
                                                                   $self->{strDbClusterPath} . '/' . FILE_MANIFEST));

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

    if ($self->{strDbClusterPath} ne $oManifest->get(MANIFEST_SECTION_BACKUP_TARGET, MANIFEST_TARGET_PGDATA, MANIFEST_SUBKEY_PATH))
    {
        &log(INFO, 'remap $PGDATA directory to ' . $self->{strDbClusterPath});

        $oManifest->set(MANIFEST_SECTION_BACKUP_TARGET, MANIFEST_TARGET_PGDATA, MANIFEST_SUBKEY_PATH, $self->{strDbClusterPath});
    }

    # Remap tablespaces when requested
    my $oTablespaceRemap;

    if (optionTest(OPTION_TABLESPACE_MAP))
    {
        my $oTablespaceRemapRequest = optionGet(OPTION_TABLESPACE_MAP);

        for my $strKey (sort(keys(%{$oTablespaceRemapRequest})))
        {
            my $bFound = false;

            for my $strTarget ($oManifest->keys(MANIFEST_SECTION_BACKUP_TARGET))
            {
                if ($oManifest->test(MANIFEST_SECTION_BACKUP_TARGET, $strTarget, MANIFEST_SUBKEY_TABLESPACE_ID, $strKey) ||
                    $oManifest->test(MANIFEST_SECTION_BACKUP_TARGET, $strTarget, MANIFEST_SUBKEY_TABLESPACE_NAME, $strKey))
                {
                    if (defined(${$oTablespaceRemap}{$strTarget}))
                    {
                        confess &log(ERROR, "tablespace ${strKey} has already been remapped to ${$oTablespaceRemap}{$strTarget}",
                                     ERROR_TABLESPACE_MAP);
                    }

                    ${$oTablespaceRemap}{$strTarget} = ${$oTablespaceRemapRequest}{$strKey};
                    $bFound = true;
                }
            }

            # Error if the tablespace was not found to be remapped
            if (!$bFound)
            {
                confess &log(ERROR, "cannot remap invalid tablespace ${strKey} to ${$oTablespaceRemapRequest}{$strKey}",
                             ERROR_TABLESPACE_MAP);
            }
        }
    }

    # Remap all tablespaces (except ones that were done individually above)
    if (optionTest(OPTION_TABLESPACE_MAP_ALL))
    {
        for my $strTarget ($oManifest->keys(MANIFEST_SECTION_BACKUP_TARGET))
        {
            if ($oManifest->isTargetTablespace($strTarget))
            {
                if (!defined(${$oTablespaceRemap}{$strTarget}))
                {
                    ${$oTablespaceRemap}{$strTarget} = optionGet(OPTION_TABLESPACE_MAP_ALL) . '/' .
                        $oManifest->get(MANIFEST_SECTION_BACKUP_TARGET, $strTarget, MANIFEST_SUBKEY_TABLESPACE_NAME);
                }
            }
        }
    }

    # Alter manifest for remapped tablespaces
    if (defined($oTablespaceRemap))
    {
        foreach my $strTarget (sort(keys(%{$oTablespaceRemap})))
        {
            my $strRemapPath = ${$oTablespaceRemap}{$strTarget};

            # Remap the tablespace in the manifest
            &log(INFO, "remap tablespace ${strTarget} directory to ${strRemapPath}");

            $oManifest->set(MANIFEST_SECTION_BACKUP_TARGET, $strTarget, MANIFEST_SUBKEY_PATH, $strRemapPath);
            $oManifest->set(MANIFEST_SECTION_TARGET_LINK, MANIFEST_TARGET_PGDATA . "/${strTarget}", MANIFEST_SUBKEY_DESTINATION,
                            $strRemapPath);
        }
    }

    $self->manifestOwnershipCheck($oManifest);

    # Remap links when requested
    my $oLinkRemap;

    if (optionTest(OPTION_LINK_MAP))
    {
        my $oLinkRemapRequest = optionGet(OPTION_LINK_MAP);

        for my $strKey (sort(keys(%{$oLinkRemapRequest})))
        {
            my $strTarget = MANIFEST_TARGET_PGDATA . "/${strKey}";

            # Only remap if a valid target link but not a tablespace
            if ($oManifest->isTargetValid($strTarget, false) &&
                $oManifest->isTargetLink($strTarget) && !$oManifest->isTargetTablespace($strTarget))
            {
                if (defined(${$oTablespaceRemap}{$strTarget}))
                {
                    confess &log(ERROR, "tablespace ${strKey} has already been remapped to ${$oLinkRemap}{$strTarget}",
                                 ERROR_LINK_MAP);
                }

                ${$oLinkRemap}{$strTarget} = ${$oLinkRemapRequest}{$strKey};

                &log(INFO, "remap link ${strTarget} destination to ${$oLinkRemap}{$strTarget}");
            }
            # Else error
            else
            {
                confess &log(ERROR, "cannot remap invalid link ${strKey} to ${$oLinkRemapRequest}{$strKey}",
                             ERROR_LINK_MAP);
            }
        }
    }

    # Remap all links (except ones that were done individually above)
    if (optionGet(OPTION_LINK_ALL))
    {
        for my $strTarget ($oManifest->keys(MANIFEST_SECTION_BACKUP_TARGET))
        {
            # If target is a link but not a tablespace and has not already been remapped when remap it
            if ($oManifest->isTargetLink($strTarget) && !$oManifest->isTargetTablespace($strTarget) &&
                !defined(${$oLinkRemap}{$strTarget}))
            {
                    ${$oLinkRemap}{$strTarget} =
                        $oManifest->get(MANIFEST_SECTION_BACKUP_TARGET, $strTarget, MANIFEST_SUBKEY_PATH);

                    if ($oManifest->isTargetFile($strTarget))
                    {
                        ${$oLinkRemap}{$strTarget} .= '/' .
                            $oManifest->get(MANIFEST_SECTION_BACKUP_TARGET, $strTarget, MANIFEST_SUBKEY_FILE);
                    }
            }
        }
    }

    # Alter manifest for remapped links
    for my $strTarget ($oManifest->keys(MANIFEST_SECTION_BACKUP_TARGET))
    {
        if ($oManifest->isTargetLink($strTarget) && !$oManifest->isTargetTablespace($strTarget))
        {
            # If the link will be remapped
            if (defined(${$oLinkRemap}{$strTarget}))
            {
                my $strTargetPath = ${$oLinkRemap}{$strTarget};

                # If this link is to a file then the specified path must be split into file and path parts
                if ($oManifest->isTargetFile($strTarget))
                {
                    $strTargetPath = dirname($strTargetPath);

                    # Error when the path is not deep enough to be valid
                    if (!defined($strTargetPath))
                    {
                        confess &log(ERROR, "${$oLinkRemap}{$strTarget} is not long enough to be target for ${strTarget}");
                    }

                    # Set the file part
                    $oManifest->set(MANIFEST_SECTION_BACKUP_TARGET, $strTarget, MANIFEST_SUBKEY_FILE,
                                    substr(${$oLinkRemap}{$strTarget}, length($strTargetPath) + 1));
                }

                # Set the target path
                $oManifest->set(MANIFEST_SECTION_BACKUP_TARGET, $strTarget, MANIFEST_SUBKEY_PATH, $strTargetPath);

                # Since this will be a link remove the associated path (??? perhaps this should be in build like it is for ts?)
                $oManifest->remove(MANIFEST_SECTION_TARGET_PATH, $strTarget);
            }
            # Else the link will be restored directly into $PGDATA instead
            else
            {
                if ($oManifest->test(MANIFEST_SECTION_BACKUP_TARGET, $strTarget, MANIFEST_SUBKEY_FILE))
                {
                    &log(WARN, 'file link ' . $oManifest->dbPathGet(undef, $strTarget) .
                               ' will be restored as a file at the same location');
                }
                else
                {
                    &log(WARN, 'contents of directory link ' . $oManifest->dbPathGet(undef, $strTarget) .
                               ' will be restored in a directory at the same location');
                }

                $oManifest->remove(MANIFEST_SECTION_BACKUP_TARGET, $strTarget);
                $oManifest->remove(MANIFEST_SECTION_TARGET_LINK, $strTarget);
            }
        }
    }

    # Make sure links are still valid after remapping
    $oManifest->linkCheck();

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
            {name => 'oManifest'}
        );

    # Track if files/links/paths where removed
    my %oRemoveHash =
    (
        &MANIFEST_SECTION_TARGET_FILE => 0,
        &MANIFEST_SECTION_TARGET_PATH => 0,
        &MANIFEST_SECTION_TARGET_LINK => 0
    );

    # Check that all targets exist and are empty (unless --force or --delta specified)
    my %oTargetFound;
    my $bDelta = optionGet(OPTION_FORCE) || optionGet(OPTION_DELTA);

    for my $strTarget ($oManifest->keys(MANIFEST_SECTION_BACKUP_TARGET))
    {
        ${$self->{oTargetPath}}{$strTarget} = $oManifest->get(MANIFEST_SECTION_BACKUP_TARGET, $strTarget, MANIFEST_SUBKEY_PATH);
        $oTargetFound{$strTarget} = false;

        my $strCheckPath = ${$self->{oTargetPath}}{$strTarget};

        if ($oManifest->isTargetLink($strTarget) && index($strCheckPath, '/') != 0)
        {
            my $strBasePath = $oManifest->get(MANIFEST_SECTION_BACKUP_TARGET, MANIFEST_TARGET_PGDATA, MANIFEST_SUBKEY_PATH);
            my $strTargetPath = dirname($oManifest->dbPathGet(undef, $strTarget));

            if ($strTargetPath ne '.')
            {
                $strBasePath .= "/${strTargetPath}";
            }

            ${$self->{oTargetPath}}{$strTarget} = pathAbsolute($strBasePath, $strCheckPath);

            $strCheckPath = ${$self->{oTargetPath}}{$strTarget};

            if ($oManifest->isTargetTablespace($strTarget))
            {
                $strCheckPath = dirname(${$self->{oTargetPath}}{$strTarget});
            }
        }

        &log(DETAIL, "check ${strCheckPath} exists");

        # Check if the path exists
        if (!$self->{oFile}->exists(PATH_DB_ABSOLUTE, $strCheckPath))
        {
            confess &log(ERROR, "cannot restore to missing path ${strCheckPath}", ERROR_PATH_MISSING);
        }

        # Check if the file exists
        if ($oManifest->isTargetFile($strTarget))
        {
            # Construct the file to check
            my $strCheckFile = "${strCheckPath}/" .
                               $oManifest->get(MANIFEST_SECTION_BACKUP_TARGET, $strTarget, MANIFEST_SUBKEY_FILE);

            # Check if the file exists
            if ($self->{oFile}->exists(PATH_DB_ABSOLUTE, $strCheckFile))
            {
                # If the file exists and this is not a delta then error
                if (!$bDelta)
                {
                    confess &log(ERROR, "cannot restore file '${strCheckFile}' that already exists - " .
                                        'try using --delta if this is what you intended', ERROR_RESTORE_PATH_NOT_EMPTY);
                }

                # Mark that a file was found
                $oTargetFound{$strTarget} = true;
            }
        }
        # Check the directory for files
        else
        {
            # If this is a tablespace search for the special directory that Postgres puts in every tablespace directory
            if ($oManifest->isTargetTablespace($strTarget))
            {
                # Construct the special tablespace path
                ${$self->{oTargetPath}}{$strTarget} = "${$self->{oTargetPath}}{$strTarget}/" . $oManifest->tablespacePathGet();

                # If this path does not exist then skip the rest of the checking - the path will be created later
                if (!$self->{oFile}->exists(PATH_DB_ABSOLUTE, ${$self->{oTargetPath}}{$strTarget}))
                {
                    next;
                }
            }

            # Build a manifest of the path to check for existing files
            my %oTargetManifest;
            $self->{oFile}->manifest(PATH_DB_ABSOLUTE, ${$self->{oTargetPath}}{$strTarget}, \%oTargetManifest);

            for my $strName (keys(%{$oTargetManifest{name}}))
            {
                # Skip the root path and backup.manifest in the base path
                if ($strName eq '.' ||
                    (($strName eq FILE_MANIFEST || $strName eq DB_FILE_RECOVERYCONF) && $strTarget eq MANIFEST_TARGET_PGDATA))
                {
                    next;
                }

                # The presense of any other file will cause an error (unless --force or --delta specified)
                if (!$bDelta)
                {
                    confess &log(ERROR, "cannot restore to path '${$self->{oTargetPath}}{$strTarget}' that contains files - " .
                                        'try using --delta if this is what you intended', ERROR_RESTORE_PATH_NOT_EMPTY);
                }

                # Mark that some files were found
                $oTargetFound{$strTarget} = true;
            }
        }
    }

    # Clean up each target starting from the most nested
    my %oFileChecked;

    for my $strTarget (sort {$b cmp $a} ($oManifest->keys(MANIFEST_SECTION_BACKUP_TARGET)))
    {
        if ($oTargetFound{$strTarget})
        {
            &log(INFO, "remove invalid files/paths/links from ${$self->{oTargetPath}}{$strTarget}");

            # OK for the special tablespace path to not exist yet - it will be created later
            if (!$self->{oFile}->exists(PATH_DB_ABSOLUTE, ${$self->{oTargetPath}}{$strTarget}) &&
                $oManifest->isTargetTablespace($strTarget))
            {
                next;
            }

            # Load path manifest so it can be compared to deleted files/paths/links that are not in the backup
            my %oTargetManifest;
            $self->{oFile}->manifest(PATH_DB_ABSOLUTE, ${$self->{oTargetPath}}{$strTarget}, \%oTargetManifest);

            # If the target is a file it doesn't matter whether it already exists or not.
            if ($oManifest->isTargetFile($strTarget))
            {
                next;
            }

            foreach my $strName (sort {$b cmp $a} (keys(%{$oTargetManifest{name}})))
            {
                # Skip the root path
                if ($strName eq '.' || ($strName eq FILE_MANIFEST && $strTarget eq MANIFEST_TARGET_PGDATA))
                {
                    next;
                }

                my $strOsFile = "${$self->{oTargetPath}}{$strTarget}/${strName}";
                my $strManifestFile = $oManifest->repoPathGet($strTarget, $strName);

                # Determine the file/path/link type
                my $strSection = MANIFEST_SECTION_TARGET_FILE;

                if ($oTargetManifest{name}{$strName}{type} eq 'd')
                {
                    $strSection = MANIFEST_SECTION_TARGET_PATH;
                }
                elsif ($oTargetManifest{name}{$strName}{type} eq 'l')
                {
                    $strSection = MANIFEST_SECTION_TARGET_LINK;
                }

                # Check to see if the file/path/link exists in the manifest
                if ($oManifest->test($strSection, $strManifestFile) &&
                    !defined($oFileChecked{$strSection}{$strManifestFile}))
                {
                    my $strUser = $oManifest->get($strSection, $strManifestFile, MANIFEST_SUBKEY_USER);
                    my $strGroup = $oManifest->get($strSection, $strManifestFile, MANIFEST_SUBKEY_GROUP);

                    # If ownership does not match, fix it
                    if (!defined($oTargetManifest{name}{$strName}{user}) ||
                        $strUser ne $oTargetManifest{name}{$strName}{user} ||
                        !defined($oTargetManifest{name}{$strName}{group}) ||
                        $strGroup ne $oTargetManifest{name}{$strName}{group})
                    {
                        &log(DETAIL, "set ownership ${strUser}:${strGroup} on ${strOsFile}");

                        $self->{oFile}->owner(PATH_DB_ABSOLUTE, $strOsFile, $strUser, $strGroup);
                    }

                    # If a link does not have the same destination, then delete it (it will be recreated later)
                    if ($strSection eq MANIFEST_SECTION_TARGET_LINK)
                    {
                        if ($oManifest->get($strSection, $strManifestFile, MANIFEST_SUBKEY_DESTINATION) ne
                            $oTargetManifest{name}{$strName}{link_destination})
                        {
                            &log(DETAIL, "remove link ${strOsFile} - destination changed");
                            fileRemove($strOsFile);
                        }
                    }
                    # Else if file/path mode does not match, fix it
                    else
                    {
                        my $strMode = $oManifest->get($strSection, $strManifestFile, MANIFEST_SUBKEY_MODE);

                        if ($strMode ne $oTargetManifest{name}{$strName}{mode})
                        {
                            &log(DETAIL, "set mode ${strMode} on ${strOsFile}");

                            chmod(oct($strMode), $strOsFile)
                                or confess 'unable to set mode ${strMode} on ${strOsFile}';
                        }
                    }
                }
                # If it does not then remove it
                else
                {
                    # If a path then remove it, all the files should have already been deleted since we are going in reverse order
                    if ($strSection eq MANIFEST_SECTION_TARGET_PATH)
                    {
                        &log(DETAIL, "remove path ${strOsFile}");
                        rmdir($strOsFile) or confess &log(ERROR, "unable to delete path ${strOsFile}, is it empty?");

                        $oRemoveHash{$strSection} += 1;
                    }
                    # Else delete a file/link
                    else
                    {
                        my $strType = (split('\:', $strSection))[1];

                        # Delete only if this is not the recovery.conf file.  This is in case the user wants the recovery.conf file
                        # preserved.  It will be written/deleted/preserved as needed in recovery().
                        if ($oManifest->dbPathGet(undef, $strManifestFile) eq DB_FILE_RECOVERYCONF)
                        {
                            &log(DETAIL, "preserve ${strType} ${strOsFile}");
                        }
                        else
                        {
                            &log(DETAIL, "remove ${strType} ${strOsFile}");
                            fileRemove($strOsFile);

                            $oRemoveHash{$strSection} += 1;
                        }
                    }
                }
            }
        }

        # Mark all files, paths, and links in this target as having been checked.  This prevents them from being eligible for
        # cleaning again in a higher level of the hierarchy.
        foreach my $strSection (sort (keys %oRemoveHash))
        {
            for my $strManifestFile ($oManifest->keys($strSection))
            {
                # If a link exactly matches the target then it should be checked at a higher level of the hierarchy.  Otherwise
                # determine if the file/path/link should be marked as checked.
                if ($strSection ne MANIFEST_SECTION_TARGET_LINK || $strManifestFile ne $strTarget)
                {
                    # If the file/path/link parent path matches the target then it should be marked as checked.
                    if (index($strManifestFile, "${strTarget}/") == 0)
                    {
                        # Mark the file/path/link as checked
                        $oFileChecked{$strSection}{$strManifestFile} = true;
                    }
                }
            }
        }
    }

    # Loop through types (path, link, file) and emit info if any were removed
    my @stryMessage;

    foreach my $strFileType (sort (keys %oRemoveHash))
    {
        if ($oRemoveHash{$strFileType} > 0)
        {
            push(@stryMessage, "$oRemoveHash{$strFileType} " . (split('\:', $strFileType))[1] .
                 ($oRemoveHash{$strFileType} > 1 ? 's' : ''));
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
# Creates missing paths and links and sets ownership/mode.
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
            {name => 'oManifest'}
        );

    # Create target paths (except for MANIFEST_TARGET_PGDATA because that must already exist)
    foreach my $strTarget ($oManifest->keys(MANIFEST_SECTION_BACKUP_TARGET))
    {
        if ($strTarget ne MANIFEST_TARGET_PGDATA)
        {
            my $strPath = ${$self->{oTargetPath}}{$strTarget};

            if ($oManifest->isTargetTablespace($strTarget))
            {
                $strPath = dirname($strPath);
            }

            if (!$self->{oFile}->exists(PATH_DB_ABSOLUTE, $strPath))
            {
                $self->{oFile}->pathCreate(PATH_DB_ABSOLUTE, $strPath,
                    $oManifest->get(MANIFEST_SECTION_TARGET_PATH, $strTarget, MANIFEST_SUBKEY_MODE));

                # Set ownership (??? this could be done better inside the file functions)
                my $strUser = $oManifest->get(MANIFEST_SECTION_TARGET_PATH, $strTarget, MANIFEST_SUBKEY_USER);
                my $strGroup = $oManifest->get(MANIFEST_SECTION_TARGET_PATH, $strTarget, MANIFEST_SUBKEY_GROUP);

                if ($strUser ne getpwuid($<) || $strGroup ne getgrgid($())
                {
                    $self->{oFile}->owner(PATH_DB_ABSOLUTE, $strPath, $strUser, $strGroup);
                }
            }
        }

        $oManifest->remove(MANIFEST_SECTION_TARGET_PATH, $strTarget);
    }

    # Create paths and links by level.  It's done this way so that all paths and links can be created within the hierarchy of
    # $PGDATA without having to worry about where linked files and directories are pointed at.
    my $iLevel = 1;
    my $iFound;

    do
    {
        $iFound = 0;

        &log(DEBUG, "build level ${iLevel} paths/links");

        # Iterate path/link sections
        foreach my $strSection (&MANIFEST_SECTION_TARGET_PATH, &MANIFEST_SECTION_TARGET_LINK)
        {
            # Iterate paths/links in the section
            foreach my $strName ($oManifest->keys($strSection))
            {
                # Skip the root path since this must already exist
                if ($strName eq '.')
                {
                    next;
                }

                # Determine the level by splitting the path
                my @stryName = split('\/', $oManifest->dbPathGet(undef, $strName));

                # Only create the path/link of at the current level
                if (@stryName == $iLevel)
                {
                    my $strDbPath = $oManifest->dbPathGet($self->{strDbClusterPath}, $strName);

                    # If the path/link does not already exist then create it.  The clean() method should have determined if the
                    # permissions, destinations, etc. are correct
                    if (!$self->{oFile}->exists(PATH_DB_ABSOLUTE, $strDbPath))
                    {
                        # Create a path
                        if ($strSection eq &MANIFEST_SECTION_TARGET_PATH)
                        {
                            $self->{oFile}->pathCreate(PATH_DB_ABSOLUTE, $strDbPath,
                                $oManifest->get($strSection, $strName, MANIFEST_SUBKEY_MODE));
                        }
                        # Else create a link
                        else
                        {
                            # Retrieve the link destination
                            my $strDestination = $oManifest->get($strSection, $strName, MANIFEST_SUBKEY_DESTINATION);

                            # In order to create relative links they must be converted to absolute links and then made relative
                            # again by linkCreate().  It's possible to modify linkCreate() to accept relative paths but that could
                            # have an impact elsewhere and doesn't seem worth it.
                            $self->{oFile}->linkCreate(PATH_DB_ABSOLUTE, pathAbsolute(dirname($strDbPath), $strDestination),
                                                       PATH_DB_ABSOLUTE, $strDbPath, undef, (index($strDestination, '/') != 0));
                        }

                        # Set ownership (??? this could be done better inside the file functions)
                        my $strUser = $oManifest->get($strSection, $strName, MANIFEST_SUBKEY_USER);
                        my $strGroup = $oManifest->get($strSection, $strName, MANIFEST_SUBKEY_GROUP);

                        if ($strUser ne getpwuid($<) || $strGroup ne getgrgid($())
                        {
                            $self->{oFile}->owner(PATH_DB_ABSOLUTE, $strDbPath, $strUser, $strGroup);
                        }
                    }

                    # Indicate that at least one path/link was found at this level
                    $iFound++;
                }
            }
        }

        # Move to the next path/link level
        $iLevel++;
    }
    while ($iFound > 0);

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
    my $self = shift;           # Class hash
    my $strDbVersion = shift;   # Version to restore

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam (OP_RESTORE_RECOVERY);

    # Create recovery.conf path/file
    my $strRecoveryConf = $self->{strDbClusterPath} . '/' . DB_FILE_RECOVERYCONF;

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
                    if ($strDbVersion >= PG_VERSION_95)
                    {
                        $strRecovery .= "recovery_target_action = '${strTargetAction}'\n";
                    }
                    elsif  ($strDbVersion >= PG_VERSION_91)
                    {
                        $strRecovery .= "pause_at_recovery_target = 'false'\n";
                    }
                    else
                    {
                        confess &log(ERROR, OPTION_TARGET_ACTION .  ' option is only available in PostgreSQL >= ' . PG_VERSION_91)
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

    if (!fileExists($self->{strDbClusterPath}))
    {
        confess &log(ERROR, "\$PGDATA directory $self->{strDbClusterPath} does not exist");
    }

    # Make sure that Postgres is not running
    if ($self->{oFile}->exists(PATH_DB_ABSOLUTE, $self->{strDbClusterPath} . '/' . DB_FILE_POSTMASTERPID))
    {
        confess &log(ERROR, 'unable to restore while Postgres is running', ERROR_POSTMASTER_RUNNING);
    }

    # If the restore will be destructive attempt to verify that $PGDATA is valid
    if ((optionGet(OPTION_DELTA) || optionGet(OPTION_FORCE)) &&
        !($self->{oFile}->exists(PATH_DB_ABSOLUTE, $self->{strDbClusterPath} . '/' . DB_FILE_PGVERSION) ||
          $self->{oFile}->exists(PATH_DB_ABSOLUTE, $self->{strDbClusterPath} . '/' . FILE_MANIFEST)))
    {
        &log(WARN, '--delta or --force specified but unable to find \'' . DB_FILE_PGVERSION . '\' or \'' . FILE_MANIFEST .
                   '\' in \'' . $self->{strDbClusterPath} . '\' to confirm that this is a valid $PGDATA directory.' .
                   '  --delta and --force have been disabled and if any files exist in the destination directories the restore' .
                   ' will be aborted.');

        optionSet(OPTION_DELTA, false);
        optionSet(OPTION_FORCE, false);
    }

    # Copy backup info, load it, then delete
    $self->{oFile}->copy(PATH_BACKUP_CLUSTER, FILE_BACKUP_INFO,
                         PATH_DB_ABSOLUTE, $self->{strDbClusterPath} . '/' . FILE_BACKUP_INFO);

    my $oBackupInfo = new pgBackRest::BackupInfo($self->{strDbClusterPath}, false);

    $self->{oFile}->remove(PATH_DB_ABSOLUTE, $self->{strDbClusterPath} . '/' . FILE_BACKUP_INFO, undef, false);

    # If set to restore is latest then get the actual set
    if ($self->{strBackupSet} eq OPTION_DEFAULT_RESTORE_SET)
    {
        $self->{strBackupSet} = $oBackupInfo->last(BACKUP_TYPE_INCR);

        if (!defined($self->{strBackupSet}))
        {
            confess &log(ERROR, "no backup sets to restore", ERROR_BACKUP_SET_INVALID);
        }
    }
    # Otherwise check to make sure specified set is valid
    else
    {
        if (!$oBackupInfo->current($self->{strBackupSet}))
        {
            confess &log(ERROR, "backup set $self->{strBackupSet} is not valid", ERROR_BACKUP_SET_INVALID);
        }
    }

    # Log the backup set to restore
    &log(INFO, "restore backup set " . $self->{strBackupSet});

    # Make sure the backup path is valid and load the manifest
    my $oManifest = $self->manifestLoad();

    # Delete pg_control file.  This will be copied from the backup at the very end to prevent a partially restored database
    # from being started by PostgreSQL.
    $self->{oFile}->remove(PATH_DB_ABSOLUTE, $oManifest->dbPathGet(
                           $oManifest->get(MANIFEST_SECTION_BACKUP_TARGET, MANIFEST_TARGET_PGDATA, MANIFEST_SUBKEY_PATH),
                           MANIFEST_FILE_PGCONTROL));

    # Clean the restore paths
    $self->clean($oManifest);

    # Build paths/links in the restore paths
    $self->build($oManifest);

    # Get variables required for restore
    my $lCopyTimeBegin = $oManifest->numericGet(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TIMESTAMP_COPY_START);
    my $bSourceCompression = $oManifest->boolGet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_COMPRESS);
    my $strCurrentUser = getpwuid($<);
    my $strCurrentGroup = getgrgid($();

    # Build an expression to match files that should be zeroed for filtered restores
    my $strDbFilter;

    if (optionTest(OPTION_DB_INCLUDE))
    {
        # Build a list of databases from the manifest since the db name/id mappings will not be available for an offline restore.
        my %oDbList;

        foreach my $strFile ($oManifest->keys(MANIFEST_SECTION_TARGET_FILE))
        {
            if ($strFile =~ ('^' . MANIFEST_TARGET_PGDATA . '\/base\/[0-9]+\/PG\_VERSION'))
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
        &log(DETAIL, 'databases for include/exclude (' . join(', ', sort(keys(%oDbList))) . ')');

        # Remove included databases from the list
        my $oDbInclude = optionGet(OPTION_DB_INCLUDE);

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
                        $strDbFilter .=
                            '|(^' . $strTarget . '\/' . $oManifest->tablespacePathGet() . '\/' . $strDbKey . '\/)';
                    }
                }
            }
        }

        # Output the generated filter for debugging
        &log(DETAIL, "database filter: $strDbFilter");
    }

    # Create hash containing files to restore
    my %oRestoreHash;
    my $lSizeTotal = 0;
    my $lSizeCurrent = 0;

    foreach my $strFile ($oManifest->keys(MANIFEST_SECTION_TARGET_FILE))
    {
        # Skip the tablespace_map file in versions >= 9.5 so Postgres does not rewrite links in DB_PATH_PGTBLSPC.
        # The tablespace links have already been created by Restore::build().
        if ($strFile eq MANIFEST_FILE_TABLESPACEMAP &&
            $oManifest->get(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_DB_VERSION) >= PG_VERSION_95)
        {
            next;
        }

        my $lSize = $oManifest->numericGet(MANIFEST_SECTION_TARGET_FILE, $strFile, MANIFEST_SUBKEY_SIZE);

        # By default put everything into a single queue
        my $strQueueKey = MANIFEST_TARGET_PGDATA;

        # If multiple threads and the file belongs in a tablespace then put in a tablespace-specific queue
        if (optionGet(OPTION_THREAD_MAX) > 1 && index($strFile, DB_PATH_PGTBLSPC . '/') == 0)
        {
            $strQueueKey = DB_PATH_PGTBLSPC . '/' . (split('\/', $strFile))[1];
        }

        # Preface the file key with the size. This allows for sorting the files to restore by size.
        my $strFileKey;

        # Skip this for global/pg_control since it will be copied as the last step and needs to named in a way that it
        # can be found for the copy.
        if ($strFile eq MANIFEST_FILE_PGCONTROL)
        {
            $strFileKey = $strFile;
            $oRestoreHash{$strQueueKey}{$strFileKey}{skip} = true;
        }
        # Else continue normally
        else
        {
            $strFileKey = sprintf("%016d-${strFile}", $lSize);
            $oRestoreHash{$strQueueKey}{$strFileKey}{skip} = false;

            $lSizeTotal += $lSize;
        }

        # Zero files that should be filtered
        if (defined($strDbFilter) && $strFile =~ $strDbFilter && $strFile !~ /\/PG\_VERSION$/)
        {
            $oRestoreHash{$strQueueKey}{$strFileKey}{zero} = true;
        }

        # Get restore information
        $oRestoreHash{$strQueueKey}{$strFileKey}{repo_file} = $strFile;
        $oRestoreHash{$strQueueKey}{$strFileKey}{db_file} =  $oManifest->dbPathGet($self->{strDbClusterPath}, $strFile);
        $oRestoreHash{$strQueueKey}{$strFileKey}{size} = $lSize;
        $oRestoreHash{$strQueueKey}{$strFileKey}{reference} =
            $oManifest->boolTest(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_HARDLINK, undef, true) ? undef :
            $oManifest->get(MANIFEST_SECTION_TARGET_FILE, $strFile, MANIFEST_SUBKEY_REFERENCE, false);
        $oRestoreHash{$strQueueKey}{$strFileKey}{modification_time} =
            $oManifest->numericGet(MANIFEST_SECTION_TARGET_FILE, $strFile, MANIFEST_SUBKEY_TIMESTAMP);
        $oRestoreHash{$strQueueKey}{$strFileKey}{mode} =
            $oManifest->get(MANIFEST_SECTION_TARGET_FILE, $strFile, MANIFEST_SUBKEY_MODE);
        $oRestoreHash{$strQueueKey}{$strFileKey}{user} =
            $oManifest->get(MANIFEST_SECTION_TARGET_FILE, $strFile, MANIFEST_SUBKEY_USER);
        $oRestoreHash{$strQueueKey}{$strFileKey}{group} =
            $oManifest->get(MANIFEST_SECTION_TARGET_FILE, $strFile, MANIFEST_SUBKEY_GROUP);

        # Checksum is only stored if size > 0
        if ($lSize > 0)
        {
            $oRestoreHash{$strQueueKey}{$strFileKey}{checksum} =
                $oManifest->get(MANIFEST_SECTION_TARGET_FILE, $strFile, MANIFEST_SUBKEY_CHECKSUM);
        }
    }

    # If multi-threaded then create threads to copy files
    if (optionGet(OPTION_THREAD_MAX) > 1)
    {
        # Load module dynamically
        require pgBackRest::Protocol::ThreadGroup;
        pgBackRest::Protocol::ThreadGroup->import();

        logDebugMisc
        (
            $strOperation, 'restore with threads',
            {name => 'iThreadTotal', value => optionGet(OPTION_THREAD_MAX)}
        );

        # Initialize the thread queues
        my @oyRestoreQueue;

        foreach my $strQueueKey (sort (keys %oRestoreHash))
        {
            push(@oyRestoreQueue, Thread::Queue->new());

            foreach my $strFileKey (sort {$b cmp $a} (keys(%{$oRestoreHash{$strQueueKey}})))
            {
                # Skip files marked to be copied later
                next if ($oRestoreHash{$strQueueKey}{$strFileKey}{skip});

                $oyRestoreQueue[@oyRestoreQueue - 1]->enqueue($oRestoreHash{$strQueueKey}{$strFileKey});
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
            $self->{oProtocol}->keepAlive();
        };
    }
    else
    {
        logDebugMisc($strOperation, 'restore in main process');

        # Restore file in main process
        foreach my $strQueueKey (sort (keys %oRestoreHash))
        {
            foreach my $strFileKey (sort {$b cmp $a} (keys(%{$oRestoreHash{$strQueueKey}})))
            {
                # Skip files marked to be copied later
                next if ($oRestoreHash{$strQueueKey}{$strFileKey}{skip});

                $lSizeCurrent = restoreFile($oRestoreHash{$strQueueKey}{$strFileKey}, $lCopyTimeBegin, optionGet(OPTION_DELTA),
                                            optionGet(OPTION_FORCE), $self->{strBackupSet}, $bSourceCompression, $strCurrentUser,
                                            $strCurrentGroup, $self->{oFile}, $lSizeTotal, $lSizeCurrent);

                # Keep the protocol layer from timing out while checksumming
                $self->{oProtocol}->keepAlive();
            }
        }
    }

    # Create recovery.conf file
    $self->recovery($oManifest->get(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_DB_VERSION));

    # Copy pg_control last
    &log(INFO, 'restore ' . $oManifest->dbPathGet(undef, MANIFEST_FILE_PGCONTROL) .
         ' (copied last to ensure aborted restores cannot be started)');

    restoreFile($oRestoreHash{&MANIFEST_TARGET_PGDATA}{&MANIFEST_FILE_PGCONTROL}, $lCopyTimeBegin, optionGet(OPTION_DELTA),
                optionGet(OPTION_FORCE), $self->{strBackupSet}, $bSourceCompression, $strCurrentUser, $strCurrentGroup,
                $self->{oFile});

    # Finally remove the manifest to indicate the restore is complete
    $self->{oFile}->remove(PATH_DB_ABSOLUTE, $self->{strDbClusterPath} . '/' . FILE_MANIFEST, undef, false);

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

1;
