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
    $self->{strBackupSet} = cfgOption(CFGOPT_SET);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
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
            __PACKAGE__ . '->manifestOwnershipCheck', \@_,
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

        if (!defined($oOwnerTypeHash{&MANIFEST_SUBKEY_USER}))
        {
            confess &log(ERROR_USER_MISSING, 'current user uid does not map to a name');
        }
    }

    if ($oManifest->test(MANIFEST_SECTION_TARGET_PATH, MANIFEST_TARGET_PGDATA, MANIFEST_SUBKEY_GROUP) &&
        !$oManifest->boolTest(MANIFEST_SECTION_TARGET_PATH, MANIFEST_TARGET_PGDATA, MANIFEST_SUBKEY_GROUP, false))
    {
        $oOwnerTypeHash{&MANIFEST_SUBKEY_GROUP} =
            $oManifest->get(MANIFEST_SECTION_TARGET_PATH, MANIFEST_TARGET_PGDATA, MANIFEST_SUBKEY_GROUP);
    }
    else
    {
        $oOwnerTypeHash{&MANIFEST_SUBKEY_GROUP} = getgrgid($();

        if (!defined($oOwnerTypeHash{&MANIFEST_SUBKEY_GROUP}))
        {
            confess &log(ERROR_GROUP_MISSING, 'current user gid does not map to a name');
        }
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
    my
    (
        $strOperation,
        $strCipherPass,                                             # Passphrase to decrypt the manifest file if encrypted
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->manifestLoad', \@_,
            {name => 'strCipherPass', required => false, redact => true},
        );

    # Error if the backup set does not exist
    if (!storageRepo()->exists(STORAGE_REPO_BACKUP . "/$self->{strBackupSet}/" . FILE_MANIFEST))
    {
        confess &log(ERROR, "backup '$self->{strBackupSet}' does not exist");
    }

    # Copy the backup manifest to the db cluster path
    storageDb()->copy(
        storageRepo()->openRead(STORAGE_REPO_BACKUP . "/$self->{strBackupSet}/" . FILE_MANIFEST, {strCipherPass => $strCipherPass}),
        $self->{strDbClusterPath} . '/' . FILE_MANIFEST);

    # Load the manifest into a hash
    my $oManifest = new pgBackRest::Manifest(
        storageDb()->pathGet($self->{strDbClusterPath} . '/' . FILE_MANIFEST), {oStorage => storageDb()});

    # If backup is latest then set it equal to backup label, else verify that requested backup and label match
    my $strBackupLabel = $oManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_LABEL);

    if ($self->{strBackupSet} eq cfgOptionDefault(CFGOPT_SET))
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

    if (cfgOptionTest(CFGOPT_TABLESPACE_MAP))
    {
        my $oTablespaceRemapRequest = cfgOption(CFGOPT_TABLESPACE_MAP);

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
    if (cfgOptionTest(CFGOPT_TABLESPACE_MAP_ALL))
    {
        for my $strTarget ($oManifest->keys(MANIFEST_SECTION_BACKUP_TARGET))
        {
            if ($oManifest->isTargetTablespace($strTarget))
            {
                if (!defined(${$oTablespaceRemap}{$strTarget}))
                {
                    ${$oTablespaceRemap}{$strTarget} = cfgOption(CFGOPT_TABLESPACE_MAP_ALL) . '/' .
                        $oManifest->get(MANIFEST_SECTION_BACKUP_TARGET, $strTarget, MANIFEST_SUBKEY_TABLESPACE_NAME);
                }
            }
        }
    }

    # Issue a warning message when we remap tablespaces in postgre < 9.2
    if ($oManifest->get(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_DB_VERSION) < PG_VERSION_92 &&
       (cfgOptionTest(CFGOPT_TABLESPACE_MAP) || cfgOptionTest(CFGOPT_TABLESPACE_MAP_ALL)))
    {
        &log(WARN, "update pg_tablespace.spclocation with new tablespace location in PostgreSQL < " . PG_VERSION_92);
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

    if (cfgOptionTest(CFGOPT_LINK_MAP))
    {
        my $oLinkRemapRequest = cfgOption(CFGOPT_LINK_MAP);

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
    if (cfgOption(CFGOPT_LINK_ALL))
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

                    # Set the link target
                    $oManifest->set(
                        MANIFEST_SECTION_TARGET_LINK, $strTarget, MANIFEST_SUBKEY_DESTINATION, ${$oLinkRemap}{$strTarget});
                }
                else
                {
                    # Set the link target
                    $oManifest->set(MANIFEST_SECTION_TARGET_LINK, $strTarget, MANIFEST_SUBKEY_DESTINATION, $strTargetPath);

                    # Since this will be a link remove the associated path (??? perhaps this should be in build like it is for ts?)
                    $oManifest->remove(MANIFEST_SECTION_TARGET_PATH, $strTarget);
                }

                # Set the target path
                $oManifest->set(MANIFEST_SECTION_BACKUP_TARGET, $strTarget, MANIFEST_SUBKEY_PATH, $strTargetPath);
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
            __PACKAGE__ . '->clean', \@_,
            {name => 'oManifest'}
        );

    # Db storage
    my $oStorageDb = storageDb();

    # Track if files/links/paths where removed
    my %oRemoveHash =
    (
        &MANIFEST_SECTION_TARGET_FILE => 0,
        &MANIFEST_SECTION_TARGET_PATH => 0,
        &MANIFEST_SECTION_TARGET_LINK => 0
    );

    # Check that all targets exist and are empty (unless --force or --delta specified)
    my %oTargetFound;
    my $bDelta = cfgOption(CFGOPT_FORCE) || cfgOption(CFGOPT_DELTA);

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

            ${$self->{oTargetPath}}{$strTarget} = $oStorageDb->pathAbsolute($strBasePath, $strCheckPath);

            $strCheckPath = ${$self->{oTargetPath}}{$strTarget};

            if ($oManifest->isTargetTablespace($strTarget))
            {
                $strCheckPath = dirname(${$self->{oTargetPath}}{$strTarget});
            }
        }

        &log(DETAIL, "check ${strCheckPath} exists");

        # Check if the path exists
        if (!$oStorageDb->pathExists($strCheckPath))
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
            if ($oStorageDb->exists($strCheckFile))
            {
                # If the file exists and this is not a delta then error
                if (!$bDelta)
                {
                    confess &log(ERROR, "cannot restore file '${strCheckFile}' that already exists - " .
                                        'try using --delta if this is what you intended', ERROR_PATH_NOT_EMPTY);
                }

                # Mark that a file was found
                $oTargetFound{$strTarget} = true;
            }
        }
        # Check the directory for files
        else
        {
            # If this is a tablespace search for the special directory that Postgres puts in versions 9.0 and above
            if ($oManifest->isTargetTablespace($strTarget))
            {
                # Construct the special tablespace path
                ${$self->{oTargetPath}}{$strTarget} = "${$self->{oTargetPath}}{$strTarget}" .
                    (($oManifest->dbVersion() >= PG_VERSION_90) ? "/" . $oManifest->tablespacePathGet() : "");

                # If this path does not exist then skip the rest of the checking - the path will be created later
                if (!$oStorageDb->pathExists(${$self->{oTargetPath}}{$strTarget}))
                {
                    next;
                }
            }

            # Build a manifest of the path to check for existing files
            my $hTargetManifest = $oStorageDb->manifest(${$self->{oTargetPath}}{$strTarget});

            for my $strName (keys(%{$hTargetManifest}))
            {
                # Skip the root path and backup.manifest in the base path
                if ($strName eq '.' ||
                    (($strName eq FILE_MANIFEST || $strName eq DB_FILE_RECOVERYCONF) && $strTarget eq MANIFEST_TARGET_PGDATA))
                {
                    next;
                }

                # The presence of any other file will cause an error (unless --force or --delta specified)
                if (!$bDelta)
                {
                    confess &log(ERROR, "cannot restore to path '${$self->{oTargetPath}}{$strTarget}' that contains files - " .
                                        'try using --delta if this is what you intended', ERROR_PATH_NOT_EMPTY);
                }

                # Mark that some files were found
                $oTargetFound{$strTarget} = true;
            }
        }
    }

    # Clean up each target starting from the most nested
    my %oFileChecked;

    for my $strTarget ($oManifest->keys(MANIFEST_SECTION_BACKUP_TARGET, INI_SORT_REVERSE))
    {
        if ($oTargetFound{$strTarget})
        {
            &log(INFO, "remove invalid files/paths/links from ${$self->{oTargetPath}}{$strTarget}");

            # OK for the special tablespace path to not exist yet - it will be created later
            if (!$oStorageDb->pathExists(${$self->{oTargetPath}}{$strTarget}) &&
                $oManifest->isTargetTablespace($strTarget))
            {
                next;
            }

            # Load path manifest so it can be compared to deleted files/paths/links that are not in the backup
            my $hTargetManifest = $oStorageDb->manifest(${$self->{oTargetPath}}{$strTarget});

            # If the target is a file it doesn't matter whether it already exists or not.
            if ($oManifest->isTargetFile($strTarget))
            {
                next;
            }

            foreach my $strName (sort {$b cmp $a} (keys(%{$hTargetManifest})))
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

                if ($hTargetManifest->{$strName}{type} eq 'd')
                {
                    $strSection = MANIFEST_SECTION_TARGET_PATH;
                }
                elsif ($hTargetManifest->{$strName}{type} eq 'l')
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
                    if (!defined($hTargetManifest->{$strName}{user}) ||
                        $strUser ne $hTargetManifest->{$strName}{user} ||
                        !defined($hTargetManifest->{$strName}{group}) ||
                        $strGroup ne $hTargetManifest->{$strName}{group})
                    {
                        &log(DETAIL, "set ownership ${strUser}:${strGroup} on ${strOsFile}");

                        $oStorageDb->owner($strOsFile, $strUser, $strGroup);
                    }

                    # If a link does not have the same destination, then delete it (it will be recreated later)
                    if ($strSection eq MANIFEST_SECTION_TARGET_LINK)
                    {
                        if ($oManifest->get($strSection, $strManifestFile, MANIFEST_SUBKEY_DESTINATION) ne
                            $hTargetManifest->{$strName}{link_destination})
                        {
                            &log(DETAIL, "remove link ${strOsFile} - destination changed");
                            $oStorageDb->remove($strOsFile);
                        }
                    }
                    # Else if file/path mode does not match, fix it
                    else
                    {
                        my $strMode = $oManifest->get($strSection, $strManifestFile, MANIFEST_SUBKEY_MODE);

                        if ($strMode ne $hTargetManifest->{$strName}{mode})
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
                            $oStorageDb->remove($strOsFile);

                            # Removing a file can be expensive so send protocol keep alive
                            protocolKeepAlive();

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
            __PACKAGE__ . '->build', \@_,
            {name => 'oManifest'}
        );

    # Db storage
    my $oStorageDb = storageDb();

    # Create target paths (except for MANIFEST_TARGET_PGDATA because that must already exist)
    foreach my $strTarget ($oManifest->keys(MANIFEST_SECTION_BACKUP_TARGET))
    {
        if ($strTarget ne MANIFEST_TARGET_PGDATA)
        {
            my $strPath = ${$self->{oTargetPath}}{$strTarget};

            if ($oManifest->isTargetTablespace($strTarget))
            {
                # ??? On PG versions < 9.0 this is getting a path above what is really the tblspc if the database is in a tblspc
                $strPath = dirname($strPath);
            }

            # ??? This may be dead code as the clean function requires the top level target directories to exist
            if (!$oStorageDb->pathExists($strPath))
            {
                $oStorageDb->pathCreate(
                    $strPath, {strMode => $oManifest->get(MANIFEST_SECTION_TARGET_PATH, $strTarget, MANIFEST_SUBKEY_MODE)});

                # Set ownership (??? this could be done better inside the file functions)
                my $strUser = $oManifest->get(MANIFEST_SECTION_TARGET_PATH, $strTarget, MANIFEST_SUBKEY_USER);
                my $strGroup = $oManifest->get(MANIFEST_SECTION_TARGET_PATH, $strTarget, MANIFEST_SUBKEY_GROUP);

                if ($strUser ne getpwuid($<) || $strGroup ne getgrgid($())
                {
                    $oStorageDb->owner($strPath, $strUser, $strGroup);
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
                    if (!$oStorageDb->pathExists($strDbPath) && !$oStorageDb->exists($strDbPath))
                    {
                        # Create a path
                        if ($strSection eq &MANIFEST_SECTION_TARGET_PATH)
                        {
                            $oStorageDb->pathCreate(
                                $strDbPath, {strMode => $oManifest->get($strSection, $strName, MANIFEST_SUBKEY_MODE)});
                        }
                        # Else create a link
                        else
                        {
                            # Retrieve the link destination
                            my $strDestination = $oManifest->get($strSection, $strName, MANIFEST_SUBKEY_DESTINATION);

                            # In order to create relative links they must be converted to absolute links and then made relative
                            # again by linkCreate().  It's possible to modify linkCreate() to accept relative paths but that could
                            # have an impact elsewhere and doesn't seem worth it.
                            $oStorageDb->linkCreate(
                                $oStorageDb->pathAbsolute(
                                    dirname($strDbPath), $strDestination), $strDbPath,
                                    {bRelative => (index($strDestination, '/') != 0, bIgnoreExists => true)});
                        }

                        # Set ownership (??? this could be done better inside the file functions)
                        my $strUser = $oManifest->get($strSection, $strName, MANIFEST_SUBKEY_USER);
                        my $strGroup = $oManifest->get($strSection, $strName, MANIFEST_SUBKEY_GROUP);

                        if ($strUser ne getpwuid($<) || $strGroup ne getgrgid($())
                        {
                            $oStorageDb->owner($strDbPath, $strUser, $strGroup);
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

    if (!$oStorageDb->pathExists($self->{strDbClusterPath}))
    {
        confess &log(ERROR, "\$PGDATA directory $self->{strDbClusterPath} does not exist");
    }

    # Make sure that Postgres is not running
    if ($oStorageDb->exists($self->{strDbClusterPath} . '/' . DB_FILE_POSTMASTERPID))
    {
        confess &log(ERROR,
            "unable to restore while PostgreSQL is running\n" .
            "HINT: presence of '" . DB_FILE_POSTMASTERPID . "' in '$self->{strDbClusterPath}' indicates PostgreSQL is running.\n" .
            "HINT: remove '" . DB_FILE_POSTMASTERPID . "' only if PostgreSQL is not running.",
            ERROR_POSTMASTER_RUNNING);
    }

    # If the restore will be destructive attempt to verify that $PGDATA is valid
    if ((cfgOption(CFGOPT_DELTA) || cfgOption(CFGOPT_FORCE)) &&
        !($oStorageDb->exists($self->{strDbClusterPath} . '/' . DB_FILE_PGVERSION) ||
          $oStorageDb->exists($self->{strDbClusterPath} . '/' . FILE_MANIFEST)))
    {
        &log(WARN, '--delta or --force specified but unable to find \'' . DB_FILE_PGVERSION . '\' or \'' . FILE_MANIFEST .
                   '\' in \'' . $self->{strDbClusterPath} . '\' to confirm that this is a valid $PGDATA directory.' .
                   '  --delta and --force have been disabled and if any files exist in the destination directories the restore' .
                   ' will be aborted.');

        cfgOptionSet(CFGOPT_DELTA, false);
        cfgOptionSet(CFGOPT_FORCE, false);
    }

    # Copy backup info, load it, then delete
    $oStorageDb->copy(
        storageRepo()->openRead(STORAGE_REPO_BACKUP . qw(/) . FILE_BACKUP_INFO),
        $self->{strDbClusterPath} . '/' . FILE_BACKUP_INFO);

    my $oBackupInfo = new pgBackRest::Backup::Info($self->{strDbClusterPath}, false, undef, {oStorage => storageDb()});

    $oStorageDb->remove($self->{strDbClusterPath} . '/' . FILE_BACKUP_INFO);

    # If set to restore is latest then get the actual set
    if ($self->{strBackupSet} eq cfgOptionDefault(CFGOPT_SET))
    {
        $self->{strBackupSet} = $oBackupInfo->last(CFGOPTVAL_BACKUP_TYPE_INCR);

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
    my $oManifest = $self->manifestLoad($oBackupInfo->cipherPassSub());

    # Delete pg_control file.  This will be copied from the backup at the very end to prevent a partially restored database
    # from being started by PostgreSQL.
    $oStorageDb->remove(
        $oManifest->dbPathGet(
            $oManifest->get(MANIFEST_SECTION_BACKUP_TARGET, MANIFEST_TARGET_PGDATA, MANIFEST_SUBKEY_PATH),
            MANIFEST_FILE_PGCONTROL));

    # Clean the restore paths
    $self->clean($oManifest);

    # Build paths/links in the restore paths
    $self->build($oManifest);

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
        &log(DETAIL, 'databases for include/exclude (' . join(', ', sort(keys(%oDbList))) . ')');

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
        &log(DETAIL, "database filter: " . (defined($strDbFilter) ? "${strDbFilter}" : ''));
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
                $self->{strBackupSet}, $oManifest->boolGet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_COMPRESS)],
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
    foreach my $strPath ($oManifest->keys(MANIFEST_SECTION_TARGET_PATH))
    {
        # This is already synced as a subpath of pg_data
        next if $strPath eq MANIFEST_TARGET_PGTBLSPC;

        $oStorageDb->pathSync($oManifest->dbPathGet($self->{strDbClusterPath}, $strPath));
    }

    # Sync targets that don't have paths above
    foreach my $strTarget ($oManifest->keys(MANIFEST_SECTION_BACKUP_TARGET))
    {
        # Already synced
        next if $strTarget eq MANIFEST_TARGET_PGDATA;

        $oStorageDb->pathSync(
            $oStorageDb->pathAbsolute(
                $self->{strDbClusterPath} . ($strTarget =~ ('^' . MANIFEST_TARGET_PGTBLSPC) ? '/' . MANIFEST_TARGET_PGTBLSPC : ''),
                $oManifest->get(MANIFEST_SECTION_BACKUP_TARGET, $strTarget, MANIFEST_VALUE_PATH)));
    }

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
