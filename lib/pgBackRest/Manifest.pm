####################################################################################################################################
# MANIFEST MODULE
####################################################################################################################################
package pgBackRest::Manifest;
use parent 'pgBackRest::Common::Ini';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(dirname basename);
use Digest::SHA;
use Time::Local qw(timelocal);

use lib dirname($0);
use pgBackRest::Common::Exception;
use pgBackRest::Common::Ini;
use pgBackRest::Common::Log;
use pgBackRest::File;
use pgBackRest::FileCommon;

####################################################################################################################################
# File/path constants
####################################################################################################################################
use constant PATH_BACKUP_HISTORY                                    => 'backup.history';
    push @EXPORT, qw(PATH_BACKUP_HISTORY);
use constant FILE_MANIFEST                                          => 'backup.manifest';
    push @EXPORT, qw(FILE_MANIFEST);

####################################################################################################################################
# Default match factor
####################################################################################################################################
use constant MANIFEST_DEFAULT_MATCH_FACTOR                          => 0.1;
    push @EXPORT, qw(MANIFEST_DEFAULT_MATCH_FACTOR);

####################################################################################################################################
# MANIFEST Constants
####################################################################################################################################
use constant MANIFEST_TARGET_PGDATA                                 => 'pg_data';
    push @EXPORT, qw(MANIFEST_TARGET_PGDATA);
use constant MANIFEST_TARGET_PGTBLSPC                               => 'pg_tblspc';
    push @EXPORT, qw(MANIFEST_TARGET_PGTBLSPC);

use constant MANIFEST_VALUE_PATH                                    => 'path';
    push @EXPORT, qw(MANIFEST_VALUE_PATH);
use constant MANIFEST_VALUE_LINK                                    => 'link';
    push @EXPORT, qw(MANIFEST_VALUE_LINK);

# Manifest sections
use constant MANIFEST_SECTION_BACKUP                                => 'backup';
    push @EXPORT, qw(MANIFEST_SECTION_BACKUP);
use constant MANIFEST_SECTION_BACKUP_DB                             => 'backup:db';
    push @EXPORT, qw(MANIFEST_SECTION_BACKUP_DB);
use constant MANIFEST_SECTION_BACKUP_INFO                           => 'backup:info';
    push @EXPORT, qw(MANIFEST_SECTION_BACKUP_INFO);
use constant MANIFEST_SECTION_BACKUP_OPTION                         => 'backup:option';
    push @EXPORT, qw(MANIFEST_SECTION_BACKUP_OPTION);
use constant MANIFEST_SECTION_BACKUP_TARGET                         => 'backup:target';
    push @EXPORT, qw(MANIFEST_SECTION_BACKUP_TARGET);
use constant MANIFEST_SECTION_DB                                    => 'db';
    push @EXPORT, qw(MANIFEST_SECTION_DB);
use constant MANIFEST_SECTION_TARGET_PATH                           => 'target:path';
    push @EXPORT, qw(MANIFEST_SECTION_TARGET_PATH);
use constant MANIFEST_SECTION_TARGET_FILE                           => 'target:file';
    push @EXPORT, qw(MANIFEST_SECTION_TARGET_FILE);
use constant MANIFEST_SECTION_TARGET_LINK                           => 'target:link';
    push @EXPORT, qw(MANIFEST_SECTION_TARGET_LINK);

# Backup metadata required for restores
use constant MANIFEST_KEY_ARCHIVE_START                             => 'backup-archive-start';
    push @EXPORT, qw(MANIFEST_KEY_ARCHIVE_START);
use constant MANIFEST_KEY_ARCHIVE_STOP                              => 'backup-archive-stop';
    push @EXPORT, qw(MANIFEST_KEY_ARCHIVE_STOP);
use constant MANIFEST_KEY_LABEL                                     => 'backup-label';
    push @EXPORT, qw(MANIFEST_KEY_LABEL);
use constant MANIFEST_KEY_LSN_START                                 => 'backup-lsn-start';
    push @EXPORT, qw(MANIFEST_KEY_LSN_START);
use constant MANIFEST_KEY_LSN_STOP                                  => 'backup-lsn-stop';
    push @EXPORT, qw(MANIFEST_KEY_LSN_STOP);
use constant MANIFEST_KEY_PRIOR                                     => 'backup-prior';
    push @EXPORT, qw(MANIFEST_KEY_PRIOR);
use constant MANIFEST_KEY_TIMESTAMP_COPY_START                      => 'backup-timestamp-copy-start';
    push @EXPORT, qw(MANIFEST_KEY_TIMESTAMP_COPY_START);
use constant MANIFEST_KEY_TIMESTAMP_START                           => 'backup-timestamp-start';
    push @EXPORT, qw(MANIFEST_KEY_TIMESTAMP_START);
use constant MANIFEST_KEY_TIMESTAMP_STOP                            => 'backup-timestamp-stop';
    push @EXPORT, qw(MANIFEST_KEY_TIMESTAMP_STOP);
use constant MANIFEST_KEY_TYPE                                      => 'backup-type';
    push @EXPORT, qw(MANIFEST_KEY_TYPE);

# Options that were set when the backup was made
use constant MANIFEST_KEY_HARDLINK                                  => 'option-hardlink';
    push @EXPORT, qw(MANIFEST_KEY_HARDLINK);
use constant MANIFEST_KEY_ARCHIVE_CHECK                             => 'option-archive-check';
    push @EXPORT, qw(MANIFEST_KEY_ARCHIVE_CHECK);
use constant MANIFEST_KEY_ARCHIVE_COPY                              => 'option-archive-copy';
    push @EXPORT, qw(MANIFEST_KEY_ARCHIVE_COPY);
use constant MANIFEST_KEY_COMPRESS                                  => 'option-compress';
    push @EXPORT, qw(MANIFEST_KEY_COMPRESS);
use constant MANIFEST_KEY_ONLINE                                    => 'option-online';
    push @EXPORT, qw(MANIFEST_KEY_ONLINE);

# Information about the database that was backed up
use constant MANIFEST_KEY_DB_ID                                     => 'db-id';
    push @EXPORT, qw(MANIFEST_KEY_DB_ID);
use constant MANIFEST_KEY_SYSTEM_ID                                 => 'db-system-id';
    push @EXPORT, qw(MANIFEST_KEY_SYSTEM_ID);
use constant MANIFEST_KEY_CATALOG                                   => 'db-catalog-version';
    push @EXPORT, qw(MANIFEST_KEY_CATALOG);
use constant MANIFEST_KEY_CONTROL                                   => 'db-control-version';
    push @EXPORT, qw(MANIFEST_KEY_CONTROL);
use constant MANIFEST_KEY_DB_LAST_SYSTEM_ID                         => 'db-last-system-id';
    push @EXPORT, qw(MANIFEST_KEY_DB_LAST_SYSTEM_ID);
use constant MANIFEST_KEY_DB_VERSION                                => 'db-version';
    push @EXPORT, qw(MANIFEST_KEY_DB_VERSION);

# Subkeys used for path/file/link info
use constant MANIFEST_SUBKEY_CHECKSUM                               => 'checksum';
    push @EXPORT, qw(MANIFEST_SUBKEY_CHECKSUM);
use constant MANIFEST_SUBKEY_DESTINATION                            => 'destination';
    push @EXPORT, qw(MANIFEST_SUBKEY_DESTINATION);
use constant MANIFEST_SUBKEY_FILE                                   => 'file';
    push @EXPORT, qw(MANIFEST_SUBKEY_FILE);
use constant MANIFEST_SUBKEY_FUTURE                                 => 'future';
    push @EXPORT, qw(MANIFEST_SUBKEY_FUTURE);
use constant MANIFEST_SUBKEY_GROUP                                  => 'group';
    push @EXPORT, qw(MANIFEST_SUBKEY_GROUP);
use constant MANIFEST_SUBKEY_MODE                                   => 'mode';
    push @EXPORT, qw(MANIFEST_SUBKEY_MODE);
use constant MANIFEST_SUBKEY_TIMESTAMP                              => 'timestamp';
    push @EXPORT, qw(MANIFEST_SUBKEY_TIMESTAMP);
use constant MANIFEST_SUBKEY_TYPE                                   => 'type';
    push @EXPORT, qw(MANIFEST_SUBKEY_TYPE);
use constant MANIFEST_SUBKEY_PATH                                   => 'path';
    push @EXPORT, qw(MANIFEST_SUBKEY_PATH);
use constant MANIFEST_SUBKEY_REFERENCE                              => 'reference';
    push @EXPORT, qw(MANIFEST_SUBKEY_REFERENCE);
use constant MANIFEST_SUBKEY_REPO_SIZE                              => 'repo-size';
    push @EXPORT, qw(MANIFEST_SUBKEY_REPO_SIZE);
use constant MANIFEST_SUBKEY_SIZE                                   => 'size';
    push @EXPORT, qw(MANIFEST_SUBKEY_SIZE);
use constant MANIFEST_SUBKEY_TABLESPACE_ID                          => 'tablespace-id';
    push @EXPORT, qw(MANIFEST_SUBKEY_TABLESPACE_ID);
use constant MANIFEST_SUBKEY_TABLESPACE_NAME                        => 'tablespace-name';
    push @EXPORT, qw(MANIFEST_SUBKEY_TABLESPACE_NAME);
use constant MANIFEST_SUBKEY_USER                                   => 'user';
    push @EXPORT, qw(MANIFEST_SUBKEY_USER);

####################################################################################################################################
# Database locations for important files/paths
####################################################################################################################################
use constant DB_PATH_GLOBAL                                         => 'global';
    push @EXPORT, qw(DB_PATH_GLOBAL);
use constant DB_PATH_PGTBLSPC                                       => 'pg_tblspc';
    push @EXPORT, qw(DB_PATH_PGTBLSPC);
use constant DB_PATH_PGXLOG                                         => 'pg_xlog';
    push @EXPORT, qw(DB_PATH_PGXLOG);

use constant DB_FILE_BACKUPLABEL                                    => 'backup_label';
    push @EXPORT, qw(DB_FILE_BACKUPLABEL);
use constant DB_FILE_BACKUPLABELOLD                                 => DB_FILE_BACKUPLABEL . '.old';
    push @EXPORT, qw(DB_FILE_BACKUPLABELOLD);
use constant DB_FILE_PGCONTROL                                      => DB_PATH_GLOBAL . '/pg_control';
    push @EXPORT, qw(DB_FILE_PGCONTROL);
use constant DB_FILE_PGVERSION                                      => 'PG_VERSION';
    push @EXPORT, qw(DB_FILE_PGVERSION);
use constant DB_FILE_POSTMASTEROPTS                                 => 'postmaster.opts';
    push @EXPORT, qw(DB_FILE_POSTMASTEROPTS);
use constant DB_FILE_POSTMASTERPID                                  => 'postmaster.pid';
    push @EXPORT, qw(DB_FILE_POSTMASTERPID);
use constant DB_FILE_RECOVERYCONF                                   => 'recovery.conf';
    push @EXPORT, qw(DB_FILE_RECOVERYCONF);
use constant DB_FILE_RECOVERYDONE                                   => 'recovery.done';
    push @EXPORT, qw(DB_FILE_RECOVERYDONE);
use constant DB_FILE_TABLESPACEMAP                                  => 'tablespace_map';
    push @EXPORT, qw(DB_FILE_TABLESPACEMAP);

####################################################################################################################################
# Manifest locations for important files/paths
####################################################################################################################################
use constant MANIFEST_PATH_PGXLOG                                   => MANIFEST_TARGET_PGDATA . '/' . DB_PATH_PGXLOG;
    push @EXPORT, qw(MANIFEST_PATH_PGXLOG);

use constant MANIFEST_FILE_BACKUPLABEL                              => MANIFEST_TARGET_PGDATA . '/' . DB_FILE_BACKUPLABEL;
    push @EXPORT, qw(MANIFEST_FILE_BACKUPLABEL);
use constant MANIFEST_FILE_PGCONTROL                                => MANIFEST_TARGET_PGDATA . '/' . DB_FILE_PGCONTROL;
    push @EXPORT, qw(MANIFEST_FILE_PGCONTROL);
use constant MANIFEST_FILE_TABLESPACEMAP                            => MANIFEST_TARGET_PGDATA . '/' . DB_FILE_TABLESPACEMAP;
    push @EXPORT, qw(MANIFEST_FILE_TABLESPACEMAP);

####################################################################################################################################
# Minimum ID for a user object in postgres
####################################################################################################################################
use constant DB_USER_OBJECT_MINIMUM_ID                              => 16384;
    push @EXPORT, qw(DB_USER_OBJECT_MINIMUM_ID);

####################################################################################################################################
# new
####################################################################################################################################
sub new
{
    my $class = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strFileName,                               # Manifest filename
        $bLoad                                      # Load the manifest?
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'strFileName', trace => true},
            {name => 'bLoad', required => false, trace => true}
        );

    # Set defaults
    $bLoad = defined($bLoad) ? $bLoad : true;

    # Init object and store variables
    my $self = $class->SUPER::new($strFileName, $bLoad);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# save
#
# Save the manifest.
####################################################################################################################################
sub save
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->save');

    # Call inherited save
    $self->SUPER::save();

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# get
#
# Get a value.
####################################################################################################################################
sub get
{
    my $self = shift;
    my $strSection = shift;
    my $strKey = shift;
    my $strSubKey = shift;
    my $bRequired = shift;
    my $oDefault = shift;

    my $oValue = $self->SUPER::get($strSection, $strKey, $strSubKey, false);

    if (!defined($oValue) && defined($strKey) && defined($strSubKey) &&
        ($strSection eq MANIFEST_SECTION_TARGET_FILE || $strSection eq MANIFEST_SECTION_TARGET_PATH ||
         $strSection eq MANIFEST_SECTION_TARGET_LINK) &&
        ($strSubKey eq MANIFEST_SUBKEY_USER || $strSubKey eq MANIFEST_SUBKEY_GROUP ||
         $strSubKey eq MANIFEST_SUBKEY_MODE) &&
        $self->test($strSection, $strKey))
    {
        $oValue = $self->SUPER::get("${strSection}:default", $strSubKey, undef, $bRequired, $oDefault);
    }
    else
    {
        $oValue = $self->SUPER::get($strSection, $strKey, $strSubKey, $bRequired, $oDefault);
    }

    return $oValue;
}

####################################################################################################################################
# boolGet
#
# Get a numeric value.
####################################################################################################################################
sub boolGet
{
    my $self = shift;
    my $strSection = shift;
    my $strValue = shift;
    my $strSubValue = shift;
    my $bRequired = shift;
    my $bDefault = shift;

    return $self->get($strSection, $strValue, $strSubValue, $bRequired,
                      defined($bDefault) ? ($bDefault ? INI_TRUE : INI_FALSE) : undef) ? true : false;
}

####################################################################################################################################
# numericGet
#
# Get a numeric value.
####################################################################################################################################
sub numericGet
{
    my $self = shift;
    my $strSection = shift;
    my $strValue = shift;
    my $strSubValue = shift;
    my $bRequired = shift;
    my $nDefault = shift;

    return $self->get($strSection, $strValue, $strSubValue, $bRequired,
                      defined($nDefault) ? $nDefault + 0 : undef) + 0;
}

####################################################################################################################################
# tablespacePathGet
#
# Get the unique path assigned by Postgres for the tablespace.
####################################################################################################################################
sub tablespacePathGet
{
    my $self = shift;

    return('PG_' . $self->get(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_DB_VERSION) .
           '_' . $self->get(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_CATALOG));
}

####################################################################################################################################
# dbPathGet
#
# Convert a repo path to where the file actually belongs in the db.
####################################################################################################################################
sub dbPathGet
{
    my $self = shift;
    my $strDbPath = shift;
    my $strFile = shift;

    my $strDbFile = defined($strDbPath) ? "${strDbPath}/" : '';

    if (index($strFile, MANIFEST_TARGET_PGDATA . '/') == 0)
    {
        $strDbFile .= substr($strFile, length(MANIFEST_TARGET_PGDATA) + 1);
    }
    else
    {
        $strDbFile .= $strFile;
    }

    return $strDbFile;
}

####################################################################################################################################
# repoPathGet
#
# Convert a database path to where to file is located in the repo.
####################################################################################################################################
sub repoPathGet
{
    my $self = shift;
    my $strTarget = shift;
    my $strFile = shift;

    my $strRepoFile = $strTarget;

    if ($self->isTargetTablespace($strTarget))
    {
        $strRepoFile .= '/' . $self->tablespacePathGet();
    }

    if (defined($strFile))
    {
        $strRepoFile .= "/${strFile}";
    }

    return $strRepoFile;
}

####################################################################################################################################
# isTargetValid
#
# Determine if a target is valid.
####################################################################################################################################
sub isTargetValid
{
    my $self = shift;
    my $strTarget = shift;
    my $bError = shift;

    if (!defined($strTarget))
    {
        confess &log(ASSERT, 'target is not defined');
    }

    if (!$self->test(MANIFEST_SECTION_BACKUP_TARGET, $strTarget))
    {
        if (defined($bError) && $bError)
        {
            confess &log(ASSERT, "${strTarget} is not a valid target");
        }

        return false;
    }

    return true;
}

####################################################################################################################################
# isTargetLink
#
# Determine if a target is a link.
####################################################################################################################################
sub isTargetLink
{
    my $self = shift;
    my $strTarget = shift;

    $self->isTargetValid($strTarget, true);

    return $self->test(MANIFEST_SECTION_BACKUP_TARGET, $strTarget, MANIFEST_SUBKEY_TYPE, MANIFEST_VALUE_LINK);
}

####################################################################################################################################
# isTargetFile
#
# Determine if a target is a file link.
####################################################################################################################################
sub isTargetFile
{
    my $self = shift;
    my $strTarget = shift;

    $self->isTargetValid($strTarget, true);

    return $self->test(MANIFEST_SECTION_BACKUP_TARGET, $strTarget, MANIFEST_SUBKEY_FILE);
}

####################################################################################################################################
# isTargetTablespace
#
# Determine if a target is a tablespace.
####################################################################################################################################
sub isTargetTablespace
{
    my $self = shift;
    my $strTarget = shift;

    $self->isTargetValid($strTarget, true);

    return $self->test(MANIFEST_SECTION_BACKUP_TARGET, $strTarget, MANIFEST_SUBKEY_TABLESPACE_ID);
}

####################################################################################################################################
# build
#
# Build the manifest object.
####################################################################################################################################
sub build
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oFile,
        $strPath,
        $oLastManifest,
        $bOnline,
        $oTablespaceMapRef,
        $oDatabaseMapRef,
        $strLevel,
        $bTablespace,
        $strParentPath,
        $strFilter
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->build', \@_,
            {name => 'oFile'},
            {name => 'strPath'},
            {name => 'oLastManifest', required => false},
            {name => 'bOnline'},
            {name => 'oTablespaceMapRef', required => false},
            {name => 'oDatabaseMapRef', required => false},
            {name => 'strLevel', required => false},
            {name => 'bTablespace', required => false},
            {name => 'strParentPath', required => false},
            {name => 'strFilter', required => false}
        );

    if (!defined($strLevel))
    {
        $strLevel = MANIFEST_TARGET_PGDATA;

        # If not online then build the tablespace map from pg_tblspc path
        if (!$bOnline && !defined($oTablespaceMapRef))
        {
            $oTablespaceMapRef = {};

            my %oTablespaceManifestHash;
            $oFile->manifest(PATH_DB_ABSOLUTE, $strPath . '/' . DB_PATH_PGTBLSPC, \%oTablespaceManifestHash);

            foreach my $strName (sort(CORE::keys(%{$oTablespaceManifestHash{name}})))
            {
                if ($strName eq '.' or $strName eq '..')
                {
                    next;
                }

                logDebugMisc($strOperation, "found tablespace ${strName}");

                ${$oTablespaceMapRef}{oid}{$strName}{name} = "ts${strName}";
            }
        }
    }

    $self->set(MANIFEST_SECTION_BACKUP_TARGET, $strLevel, MANIFEST_SUBKEY_PATH, $strPath);
    $self->set(MANIFEST_SECTION_BACKUP_TARGET, $strLevel, MANIFEST_SUBKEY_TYPE,
               $strLevel eq MANIFEST_TARGET_PGDATA ? MANIFEST_VALUE_PATH : MANIFEST_VALUE_LINK);

    if ($bTablespace)
    {
        my $iTablespaceId = (split('\/', $strLevel))[1];

        if (!defined(${$oTablespaceMapRef}{oid}{$iTablespaceId}{name}))
        {
            confess &log(ASSERT, "tablespace with oid ${iTablespaceId} not found in tablespace map\n" .
                                 "HINT: was a tablespace created or dropped during the backup?");
        }

        $self->set(MANIFEST_SECTION_BACKUP_TARGET, $strLevel, MANIFEST_SUBKEY_TABLESPACE_ID, $iTablespaceId);
        $self->set(MANIFEST_SECTION_BACKUP_TARGET, $strLevel, MANIFEST_SUBKEY_TABLESPACE_NAME,
                   ${$oTablespaceMapRef}{oid}{$iTablespaceId}{name});
    }

    if (index($strPath, '/') != 0)
    {
        if (!defined($strParentPath))
        {
            confess &log(ASSERT, "cannot get manifest for '${strPath}' when no parent path is specified");
        }

        $strPath = pathAbsolute($strParentPath, $strPath);
    }

    # Get the manifest for this level
    my %oManifestHash;
    $oFile->manifest(PATH_DB_ABSOLUTE, $strPath, \%oManifestHash);
    my $strManifestType = MANIFEST_VALUE_LINK;

    # Loop though all paths/files/links in the manifest
    foreach my $strName (sort(CORE::keys(%{$oManifestHash{name}})))
    {
        my $strFile = $strLevel;

        if ($strName ne '.')
        {
            # Make sure the current file matches the filter or any files under the filter
            if (defined($strFilter) && $strName ne $strFilter && index($strName, "${strFilter}/") != 0)
            {
                next;
            }

            if ($strManifestType eq MANIFEST_VALUE_LINK)
            {
                if ($oManifestHash{name}{$strName}{type} eq 'l')
                {
                    confess &log(ERROR, 'link ' .
                        $self->dbPathGet(
                            $self->get(MANIFEST_SECTION_BACKUP_TARGET, MANIFEST_TARGET_PGDATA, MANIFEST_SUBKEY_PATH), $strLevel) .
                            ' (' . $self->get(MANIFEST_SECTION_BACKUP_TARGET, $strLevel, MANIFEST_SUBKEY_PATH) . ')' .
                            ' cannot reference another link', ERROR_LINK_DESTINATION);
                }

                $strFile = dirname($strFile);
                $self->set(MANIFEST_SECTION_BACKUP_TARGET, $strLevel, MANIFEST_SUBKEY_PATH,
                           dirname($self->get(MANIFEST_SECTION_BACKUP_TARGET, $strLevel, MANIFEST_SUBKEY_PATH)));
                $self->set(MANIFEST_SECTION_BACKUP_TARGET, $strLevel, MANIFEST_SUBKEY_FILE, $strName);
            }

            $strFile .= "/${strName}";
        }
        else
        {
            $strManifestType = MANIFEST_VALUE_PATH;
        }

        # Skip certain files during backup
        if ($strFile =~ ('^' . MANIFEST_PATH_PGXLOG . '.*\/') && $bOnline ||  # pg_xlog/ - this will be reconstructed
            $strLevel eq MANIFEST_TARGET_PGDATA &&
            ($strName eq DB_FILE_BACKUPLABELOLD ||                  # backup_label.old - old backup labels are not useful
             $strName eq DB_FILE_POSTMASTEROPTS ||                  # postmaster.opts - not useful for backup
             $strName eq DB_FILE_POSTMASTERPID ||                   # postmaster.pid - to avoid confusing postgres after restore
             $strName eq DB_FILE_RECOVERYCONF ||                    # recovery.conf - doesn't make sense to backup this file
             $strName eq DB_FILE_RECOVERYDONE))                     # recovery.done - doesn't make sense to backup this file
        {
            next;
        }

        my $cType = $oManifestHash{name}{$strName}{type};
        my $strSection = MANIFEST_SECTION_TARGET_PATH;

        if ($cType eq 'f')
        {
            $strSection = MANIFEST_SECTION_TARGET_FILE;
        }
        elsif ($cType eq 'l')
        {
            $strSection = MANIFEST_SECTION_TARGET_LINK;
        }
        elsif ($cType ne 'd')
        {
            confess &log(ASSERT, "unrecognized file type $cType for file $strName");
        }

        # Make sure that DB_PATH_PGTBLSPC contains only absolute links that do not point inside PGDATA
        my $bTablespace = false;

        if (index($strName, DB_PATH_PGTBLSPC . '/') == 0 && $strLevel eq MANIFEST_TARGET_PGDATA)
        {
            $bTablespace = true;
            $strFile = MANIFEST_TARGET_PGDATA . '/' . $strName;

            # Check for files in DB_PATH_PGTBLSPC that are not links
            if ($oManifestHash{name}{$strName}{type} ne 'l')
            {
                confess &log(ERROR, "${strName} is not a symlink - " . DB_PATH_PGTBLSPC . ' should contain only symlinks',
                             ERROR_LINK_EXPECTED);
            }

            # Check for tablespaces in PGDATA
            if (index($oManifestHash{name}{$strName}{link_destination}, "${strPath}/") == 0 ||
                (index($oManifestHash{name}{$strName}{link_destination}, '/') != 0 &&
                 index(pathAbsolute($strPath . '/' . DB_PATH_PGTBLSPC,
                       $oManifestHash{name}{$strName}{link_destination}) . '/', "${strPath}/") == 0))
            {
                confess &log(ERROR, 'tablespace symlink ' . $oManifestHash{name}{$strName}{link_destination} .
                             ' destination must not be in $PGDATA', ERROR_TABLESPACE_IN_PGDATA);
            }
        }

        # User and group required for all types
        if (defined($oManifestHash{name}{$strName}{user}))
        {
            $self->set($strSection, $strFile, MANIFEST_SUBKEY_USER, $oManifestHash{name}{$strName}{user});
        }
        else
        {
            $self->boolSet($strSection, $strFile, MANIFEST_SUBKEY_USER, false);
        }

        if (defined($oManifestHash{name}{$strName}{group}))
        {
            $self->set($strSection, $strFile, MANIFEST_SUBKEY_GROUP, $oManifestHash{name}{$strName}{group});
        }
        else
        {
            $self->boolSet($strSection, $strFile, MANIFEST_SUBKEY_GROUP, false);
        }

        # Mode for required file and path type only
        if ($cType eq 'f' || $cType eq 'd')
        {
            $self->set($strSection, $strFile, MANIFEST_SUBKEY_MODE, $oManifestHash{name}{$strName}{mode});
        }

        # Modification time and size required for file type only
        if ($cType eq 'f')
        {
            $self->set($strSection, $strFile, MANIFEST_SUBKEY_TIMESTAMP,
                       $oManifestHash{name}{$strName}{modification_time} + 0);
            $self->set($strSection, $strFile, MANIFEST_SUBKEY_SIZE, $oManifestHash{name}{$strName}{size} + 0);
        }

        # Link destination required for link type only
        if ($cType eq 'l')
        {
            my $strLinkDestination = $oManifestHash{name}{$strName}{link_destination};
            $self->set($strSection, $strFile, MANIFEST_SUBKEY_DESTINATION, $strLinkDestination);

            # If this is a tablespace then set the filter to use for the next level
            my $strFilter;

            if ($bTablespace)
            {
                $strFilter = $self->tablespacePathGet();

                $self->set(MANIFEST_SECTION_TARGET_PATH, MANIFEST_TARGET_PGTBLSPC, undef,
                           $self->get(MANIFEST_SECTION_TARGET_PATH, MANIFEST_TARGET_PGDATA));

                # PGDATA prefix was only needed for the link so strip it off before recursing
                $strFile = substr($strFile, length(MANIFEST_TARGET_PGDATA) + 1);
            }

            $strPath = dirname("${strPath}/${strName}");

            $self->build($oFile, $strLinkDestination, undef, $bOnline, $oTablespaceMapRef, $oDatabaseMapRef,
                         $strFile, $bTablespace, $strPath, $strFilter, $strLinkDestination);
        }
    }

    # If this is the base level then do post-processing
    if ($strLevel eq MANIFEST_TARGET_PGDATA)
    {
        my $bTimeInFuture = false;

        # Wait for the remainder of the second when doing online backups.  This is done because most filesystems only have a one
        # second resolution and Postgres will still be modifying files during the second that the manifest is built and this could
        # lead to an invalid diff/incr backup later when using timestamps to determine which files have changed.  Offline backups do
        # not wait because it makes testing much faster and Postgres should not be running (if it is the backup will not be
        # consistent anyway and the one-second resolution problem is the least of our worries).
        my $lTimeBegin = $oFile->wait(PATH_DB_ABSOLUTE, $bOnline);

        # Check that links are valid
        $self->linkCheck();

        if (defined($oLastManifest))
        {
            $self->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_PRIOR, undef,
                       $oLastManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_LABEL));
        }

        # Store database map information when provided during an online backup.
        foreach my $strDbName (sort(keys(%{${$oDatabaseMapRef}{name}})))
        {
            $self->numericSet(MANIFEST_SECTION_DB, $strDbName, MANIFEST_KEY_DB_ID,
                              ${$oDatabaseMapRef}{'name'}{$strDbName}{&MANIFEST_KEY_DB_ID});
            $self->numericSet(MANIFEST_SECTION_DB, $strDbName, MANIFEST_KEY_DB_LAST_SYSTEM_ID,
                              ${$oDatabaseMapRef}{'name'}{$strDbName}{&MANIFEST_KEY_DB_LAST_SYSTEM_ID});
        }

        # Loop though all files
        foreach my $strName ($self->keys(MANIFEST_SECTION_TARGET_FILE))
        {
            # If modification time is in the future (in this backup OR the last backup) set warning flag and do not
            # allow a reference
            if ($self->numericGet(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_TIMESTAMP) > $lTimeBegin ||
                (defined($oLastManifest) &&
                 $oLastManifest->test(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_FUTURE, 'y')))
            {
                $bTimeInFuture = true;

                # Only mark as future if still in the future in the current backup
                if ($self->numericGet(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_TIMESTAMP) > $lTimeBegin)
                {
                    $self->set(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_FUTURE, 'y');
                }
            }
            # Else check if modification time and size are unchanged since last backup
            elsif (defined($oLastManifest) && $oLastManifest->test(MANIFEST_SECTION_TARGET_FILE, $strName) &&
                   $self->numericGet(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_SIZE) ==
                       $oLastManifest->get(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_SIZE) &&
                   $self->numericGet(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_TIMESTAMP) ==
                       $oLastManifest->get(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_TIMESTAMP))
            {
                # Copy reference from previous backup if possible
                if ($oLastManifest->test(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_REFERENCE))
                {
                    $self->set(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_REFERENCE,
                               $oLastManifest->get(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_REFERENCE));
                }
                # Otherwise the reference is to the previous backup
                else
                {
                    $self->set(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_REFERENCE,
                               $oLastManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_LABEL));
                }

                # Copy the checksum from previous manifest
                if ($oLastManifest->test(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_CHECKSUM))
                {
                    $self->set(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_CHECKSUM,
                               $oLastManifest->get(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_CHECKSUM));
                }

                if ($oLastManifest->test(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_REPO_SIZE))
                {
                    $self->set(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_REPO_SIZE,
                               $oLastManifest->get(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_REPO_SIZE));
                }
            }
        }

        # Warn if any files in the current backup are in the future
        if ($bTimeInFuture)
        {
            &log(WARN, "some files have timestamps in the future - they will be copied to prevent possible race conditions");
        }

        # Record the time when copying will start
        $self->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TIMESTAMP_COPY_START, undef, $lTimeBegin + ($bOnline ? 1 : 0));

        # Build default sections
        $self->buildDefault();
    }

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# linkCheck
#
# Check all link targets and make sure none of them are a subset of another link.  In theory it would be possible to resolve the
# dependencies and generate a valid backup/restore but it's really complicated and there don't seem to be any compelling use cases.
####################################################################################################################################
sub linkCheck
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->linkCheck');

    # Working variable
    my $strBasePath = $self->get(MANIFEST_SECTION_BACKUP_TARGET, MANIFEST_TARGET_PGDATA, MANIFEST_SUBKEY_PATH);

    foreach my $strTargetParent ($self->keys(MANIFEST_SECTION_BACKUP_TARGET))
    {
        if ($self->isTargetLink($strTargetParent))
        {
            my $strParentPath = $self->get(MANIFEST_SECTION_BACKUP_TARGET, $strTargetParent, MANIFEST_SUBKEY_PATH);

            foreach my $strTargetChild ($self->keys(MANIFEST_SECTION_BACKUP_TARGET))
            {
                if ($self->isTargetLink($strTargetChild) && $strTargetParent ne $strTargetChild)
                {
                    my $strChildPath = $self->get(MANIFEST_SECTION_BACKUP_TARGET, $strTargetChild, MANIFEST_SUBKEY_PATH);

                    if (index(pathAbsolute($strBasePath, $strChildPath), pathAbsolute($strBasePath, $strParentPath)) == 0)
                    {
                        confess &log(ERROR, 'link ' . $self->dbPathGet($strBasePath, $strTargetChild) .
                                            " (${strChildPath}) references a subdirectory of or" .
                                            " the same directory as link " . $self->dbPathGet($strBasePath, $strTargetParent) .
                                            " (${strParentPath})", ERROR_LINK_DESTINATION);
                    }
                }
            }
        }
    }
}

####################################################################################################################################
# fileAdd
#
# Add files to the manifest that were generated after the initial manifest build, e.g. backup_label, tablespace_map, and copied WAL
# files.  Since the files were not in the original cluster the user, group, and mode must be defaulted.
####################################################################################################################################
sub fileAdd
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strManifestFile,
        $lModificationTime,
        $lSize,
        $strChecksum
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->fileAdd', \@_,
            {name => 'strManifestFile'},
            {name => 'lModificationTime'},
            {name => 'lSize'},
            {name => 'lChecksum'}
        );

    # Set manifest values
    if (!$self->test(MANIFEST_SECTION_TARGET_FILE . ':default', MANIFEST_SUBKEY_USER) ||
        !$self->test(MANIFEST_SECTION_TARGET_FILE . ':default', MANIFEST_SUBKEY_USER, undef,
                     $self->get(MANIFEST_SECTION_TARGET_PATH, MANIFEST_TARGET_PGDATA, MANIFEST_SUBKEY_USER)))
    {
        $self->set(MANIFEST_SECTION_TARGET_FILE, $strManifestFile, MANIFEST_SUBKEY_USER,
                   $self->get(MANIFEST_SECTION_TARGET_PATH, MANIFEST_TARGET_PGDATA, MANIFEST_SUBKEY_USER));
    }

    if (!$self->test(MANIFEST_SECTION_TARGET_FILE . ':default', MANIFEST_SUBKEY_GROUP) ||
        !$self->test(MANIFEST_SECTION_TARGET_FILE . ':default', MANIFEST_SUBKEY_GROUP, undef,
                     $self->get(MANIFEST_SECTION_TARGET_PATH, MANIFEST_TARGET_PGDATA, MANIFEST_SUBKEY_GROUP)))
    {
        $self->set(MANIFEST_SECTION_TARGET_FILE, $strManifestFile, MANIFEST_SUBKEY_GROUP,
                   $self->get(MANIFEST_SECTION_TARGET_PATH, MANIFEST_TARGET_PGDATA, MANIFEST_SUBKEY_GROUP));
    }

    if (!$self->test(MANIFEST_SECTION_TARGET_FILE . ':default', MANIFEST_SUBKEY_MODE) ||
        !$self->test(MANIFEST_SECTION_TARGET_FILE . ':default', MANIFEST_SUBKEY_MODE, undef, '0600'))
    {
        $self->set(MANIFEST_SECTION_TARGET_FILE, $strManifestFile, MANIFEST_SUBKEY_MODE, '0600');
    }

    $self->set(MANIFEST_SECTION_TARGET_FILE, $strManifestFile, MANIFEST_SUBKEY_TIMESTAMP, $lModificationTime);
    $self->set(MANIFEST_SECTION_TARGET_FILE, $strManifestFile, MANIFEST_SUBKEY_SIZE, $lSize);
    $self->set(MANIFEST_SECTION_TARGET_FILE, $strManifestFile, MANIFEST_SUBKEY_CHECKSUM, $strChecksum);
}

####################################################################################################################################
# buildDefault
#
# Builds the default section.
####################################################################################################################################
sub buildDefault
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->buildDefault');

    # Set defaults for subkeys that tend to repeat
    foreach my $strSection (&MANIFEST_SECTION_TARGET_FILE, &MANIFEST_SECTION_TARGET_PATH, &MANIFEST_SECTION_TARGET_LINK)
    {
        foreach my $strSubKey (&MANIFEST_SUBKEY_USER, &MANIFEST_SUBKEY_GROUP, &MANIFEST_SUBKEY_MODE)
        {
            # Links don't have a mode so skip
            next if ($strSection eq MANIFEST_SECTION_TARGET_LINK && $strSubKey eq &MANIFEST_SUBKEY_MODE);

            my %oDefault;
            my $iSectionTotal = 0;

            foreach my $strFile ($self->keys($strSection))
            {
                if (!$self->boolTest($strSection, $strFile, $strSubKey, false))
                {
                    my $strValue = $self->get($strSection, $strFile, $strSubKey);

                    if (defined($oDefault{$strValue}))
                    {
                        $oDefault{$strValue}++;
                    }
                    else
                    {
                        $oDefault{$strValue} = 1;
                    }
                }

                $iSectionTotal++;
            }

            my $strMaxValue;
            my $iMaxValueTotal = 0;

            foreach my $strValue (keys(%oDefault))
            {
                if ($oDefault{$strValue} > $iMaxValueTotal)
                {
                    $iMaxValueTotal = $oDefault{$strValue};
                    $strMaxValue = $strValue;
                }
            }

            if (defined($strMaxValue) > 0 && $iMaxValueTotal > $iSectionTotal * MANIFEST_DEFAULT_MATCH_FACTOR)
            {
                $self->set("${strSection}:default", $strSubKey, undef, $strMaxValue);

                foreach my $strFile ($self->keys($strSection))
                {
                    if ($self->test($strSection, $strFile, $strSubKey, $strMaxValue))
                    {
                        $self->remove($strSection, $strFile, $strSubKey);
                    }
                }
            }
        }
    }

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

1;
