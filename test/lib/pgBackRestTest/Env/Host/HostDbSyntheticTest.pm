####################################################################################################################################
# HostDbTest.pm - Database host
####################################################################################################################################
package pgBackRestTest::Env::Host::HostDbSyntheticTest;
use parent 'pgBackRestTest::Env::Host::HostDbCommonTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();
use Fcntl ':mode';
use File::Basename qw(basename dirname);
use File::stat;

use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::Common::Wait;
use pgBackRest::DbVersion;
use pgBackRest::Manifest;
use pgBackRest::Storage::Helper;
use pgBackRest::Version;

use pgBackRestTest::Env::Host::HostBackupTest;
use pgBackRestTest::Env::Host::HostBaseTest;
use pgBackRestTest::Env::Host::HostDbCommonTest;
use pgBackRestTest::Common::ContainerTest;
use pgBackRestTest::Common::FileTest;
use pgBackRestTest::Common::RunTest;

####################################################################################################################################
# new
####################################################################################################################################
sub new
{
    my $class = shift;          # Class name

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oParam,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'oParam', required => false, trace => true},
        );

    my $self = $class->SUPER::new(
        {
            strImage => containerRepo() . ':' . testRunGet()->vm() . "-test",
            strBackupDestination => $$oParam{strBackupDestination},
            oLogTest => $$oParam{oLogTest},
            bSynthetic => true,
            bStandby => $$oParam{bStandby},
            bRepoLocal => $oParam->{bRepoLocal},
            bRepoEncrypt => $oParam->{bRepoEncrypt},
        });
    bless $self, $class;

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self, trace => true}
    );
}

####################################################################################################################################
# dbFileCreate
#
# Create a file specifying content, mode, and time.
####################################################################################################################################
sub dbFileCreate
{
    my $self = shift;
    my $oManifestRef = shift;
    my $strTarget = shift;
    my $strFile = shift;
    my $strContent = shift;
    my $lTime = shift;
    my $strMode = shift;

    # Check that strTarget is a valid
    my $strPath = ${$oManifestRef}{&MANIFEST_SECTION_BACKUP_TARGET}{$strTarget}{&MANIFEST_SUBKEY_PATH};

    if (!defined($strPath))
    {
        confess &log(ERROR, "${strTarget} not a valid target: \n" . Dumper(${$oManifestRef}{&MANIFEST_SECTION_BACKUP_TARGET}));
    }

    # Get tablespace path if this is a tablespace
    my $strPgPath;

    if ($$oManifestRef{&MANIFEST_SECTION_BACKUP_DB}{&MANIFEST_KEY_DB_VERSION} >= PG_VERSION_90 &&
        index($strTarget, DB_PATH_PGTBLSPC . '/') == 0)
    {
        my $iCatalog = ${$oManifestRef}{&MANIFEST_SECTION_BACKUP_DB}{&MANIFEST_KEY_CATALOG};

        $strPgPath = 'PG_' . ${$oManifestRef}{&MANIFEST_SECTION_BACKUP_DB}{&MANIFEST_KEY_DB_VERSION} . "_${iCatalog}";
    }

    # Create actual file location
    my $strPathFile = $strPath .
                      (defined($strPgPath) ? "/${strPgPath}" : '') . "/${strFile}";

    if (index($strPathFile, '/') != 0)
    {
        $strPathFile = ${$oManifestRef}{&MANIFEST_SECTION_BACKUP_TARGET}{&MANIFEST_TARGET_PGDATA}{&MANIFEST_SUBKEY_PATH} . '/' .
                       (defined(dirname($strPathFile)) ? dirname($strPathFile) : '') . "/${strPathFile}";
    }

    # Create the file
    testFileCreate($strPathFile, $strContent, $lTime, $strMode);

    # Return path to created file
    return $strPathFile;
}

####################################################################################################################################
# dbFileRemove
####################################################################################################################################
sub dbFileRemove
{
    my $self = shift;
    my $oManifestRef = shift;
    my $strTarget = shift;
    my $strFile = shift;
    my $bIgnoreMissing = shift;

    # Get actual path location
    my $strDbFile = $self->manifestDbPathGet($oManifestRef, $strTarget, $strFile);

    # Remove the file
    if (!(defined($bIgnoreMissing) && $bIgnoreMissing && !(-e $strDbFile)))
    {
        testFileRemove($strDbFile);
    }

    return $strDbFile;
}

####################################################################################################################################
# dbLinkCreate
#
# Create a file specifying content, mode, and time.
####################################################################################################################################
sub dbLinkCreate
{
    my $self = shift;
    my $oManifestRef = shift;
    my $strTarget = shift;
    my $strFile = shift;
    my $strDestination = shift;

    # Create actual file location
    my $strDbFile = $self->manifestDbPathGet($oManifestRef, $strTarget, $strFile);

    # Create the file
    testLinkCreate($strDbFile, $strDestination);

    # Return path to created file
    return $strDbFile;
}

####################################################################################################################################
# manifestDbPathGet
#
# Get the db path based on the target and file passed.
####################################################################################################################################
sub manifestDbPathGet
{
    my $self = shift;
    my $oManifestRef = shift;
    my $strTarget = shift;
    my $strFile = shift;

    # Determine the manifest key
    my $strDbPath = ${$oManifestRef}{&MANIFEST_SECTION_BACKUP_TARGET}{$strTarget}{&MANIFEST_SUBKEY_PATH};

    # If target is a tablespace and pg version >= 9.0
    if (defined(${$oManifestRef}{&MANIFEST_SECTION_BACKUP_TARGET}{$strTarget}{&MANIFEST_SUBKEY_TABLESPACE_ID}) &&
        $$oManifestRef{&MANIFEST_SECTION_BACKUP_DB}{&MANIFEST_KEY_DB_VERSION} >= PG_VERSION_90)
    {
        my $iCatalog = ${$oManifestRef}{&MANIFEST_SECTION_BACKUP_DB}{&MANIFEST_KEY_CATALOG};

        $strDbPath .= '/PG_' . ${$oManifestRef}{&MANIFEST_SECTION_BACKUP_DB}{&MANIFEST_KEY_DB_VERSION} . "_${iCatalog}";
    }

    $strDbPath .= defined($strFile) ? "/${strFile}" : '';

    return $strDbPath;
}

####################################################################################################################################
# manifestFileCreate
#
# Create a file specifying content, mode, and time and add it to the manifest.
####################################################################################################################################
sub manifestFileCreate
{
    my $self = shift;
    my $oManifestRef = shift;
    my $strTarget = shift;
    my $strFile = shift;
    my $strContent = shift;
    my $strChecksum = shift;
    my $lTime = shift;
    my $strMode = shift;
    my $bMaster = shift;
    my $strChecksumPageError = shift;

    # Determine the manifest key
    my $strManifestKey = $self->manifestKeyGet($oManifestRef, $strTarget, $strFile);

    # Create the file
    my $strPathFile = $self->dbFileCreate($oManifestRef, $strTarget, $strFile, $strContent, $lTime, $strMode);

    # Stat the file
    my $oStat = lstat($strPathFile);

    ${$oManifestRef}{&MANIFEST_SECTION_TARGET_FILE}{$strManifestKey}{&MANIFEST_SUBKEY_GROUP} = getgrgid($oStat->gid);
    ${$oManifestRef}{&MANIFEST_SECTION_TARGET_FILE}{$strManifestKey}{&MANIFEST_SUBKEY_USER} = getpwuid($oStat->uid);
    ${$oManifestRef}{&MANIFEST_SECTION_TARGET_FILE}{$strManifestKey}{&MANIFEST_SUBKEY_MODE} =
        sprintf('%04o', S_IMODE($oStat->mode));
    ${$oManifestRef}{&MANIFEST_SECTION_TARGET_FILE}{$strManifestKey}{&MANIFEST_SUBKEY_TIMESTAMP} = $oStat->mtime;
    ${$oManifestRef}{&MANIFEST_SECTION_TARGET_FILE}{$strManifestKey}{&MANIFEST_SUBKEY_SIZE} = $oStat->size;
    ${$oManifestRef}{&MANIFEST_SECTION_TARGET_FILE}{$strManifestKey}{&MANIFEST_SUBKEY_MASTER} =
        defined($bMaster) ? ($bMaster ? JSON::PP::true : JSON::PP::false) : JSON::PP::false;
    delete(${$oManifestRef}{&MANIFEST_SECTION_TARGET_FILE}{$strManifestKey}{&MANIFEST_SUBKEY_REFERENCE});

    my $bChecksumPage = defined($strChecksumPageError) ? false : (isChecksumPage($strManifestKey) ? true : undef);

    if (defined($bChecksumPage))
    {
        $oManifestRef->{&MANIFEST_SECTION_TARGET_FILE}{$strManifestKey}{&MANIFEST_SUBKEY_CHECKSUM_PAGE} =
            $bChecksumPage ? JSON::PP::true : JSON::PP::false;

        if (!$bChecksumPage && $strChecksumPageError ne '0')
        {
            my @iyChecksumPageError = eval($strChecksumPageError);

            $oManifestRef->{&MANIFEST_SECTION_TARGET_FILE}{$strManifestKey}{&MANIFEST_SUBKEY_CHECKSUM_PAGE_ERROR} =
                \@iyChecksumPageError;
        }
        else
        {
            delete($oManifestRef->{&MANIFEST_SECTION_TARGET_FILE}{$strManifestKey}{&MANIFEST_SUBKEY_CHECKSUM_PAGE_ERROR});
        }
    }

    if (defined($strChecksum))
    {
        ${$oManifestRef}{&MANIFEST_SECTION_TARGET_FILE}{$strManifestKey}{checksum} = $strChecksum;
    }
}

####################################################################################################################################
# manifestFileRemove
#
# Remove a file from disk and (optionally) the manifest.
####################################################################################################################################
sub manifestFileRemove
{
    my $self = shift;
    my $oManifestRef = shift;
    my $strTarget = shift;
    my $strFile = shift;

    # Determine the manifest key
    my $strManifestKey = $self->manifestKeyGet($oManifestRef, $strTarget, $strFile);

    # Remove the file
    $self->dbFileRemove($oManifestRef, $strTarget, $strFile, true);

    # Remove from manifest
    delete(${$oManifestRef}{&MANIFEST_SECTION_TARGET_FILE}{$strManifestKey});
}

####################################################################################################################################
# manifestKeyGet
#
# Get the manifest key based on the target and file/path/link passed.
####################################################################################################################################
sub manifestKeyGet
{
    my $self = shift;
    my $oManifestRef = shift;
    my $strTarget = shift;
    my $strFile = shift;

    # Determine the manifest key
    my $strManifestKey = $strTarget;

    # If target is a tablespace and pg version >= 9.0
    if (defined(${$oManifestRef}{&MANIFEST_SECTION_BACKUP_TARGET}{$strTarget}{&MANIFEST_SUBKEY_TABLESPACE_ID}) &&
        $$oManifestRef{&MANIFEST_SECTION_BACKUP_DB}{&MANIFEST_KEY_DB_VERSION} >= PG_VERSION_90)
    {
        my $iCatalog = ${$oManifestRef}{&MANIFEST_SECTION_BACKUP_DB}{&MANIFEST_KEY_CATALOG};

        $strManifestKey .= '/PG_' . ${$oManifestRef}{&MANIFEST_SECTION_BACKUP_DB}{&MANIFEST_KEY_DB_VERSION} . "_${iCatalog}";
    }

    $strManifestKey .= (defined($strFile) ? "/$strFile" : '');

    return $strManifestKey;
}

####################################################################################################################################
# manifestLinkCreate
#
# Create a link and add it to the manifest.
####################################################################################################################################
sub manifestLinkCreate
{
    my $self = shift;
    my $oManifestRef = shift;
    my $strPath = shift;
    my $strFile = shift;
    my $strDestination = shift;
    my $bMaster = shift;

    # Determine the manifest key
    my $strManifestKey = $self->manifestKeyGet($oManifestRef, $strPath, $strFile);

    # Load target
    ${$oManifestRef}{&MANIFEST_SECTION_BACKUP_TARGET}{$strManifestKey}{&MANIFEST_SUBKEY_PATH} = $strDestination;
    ${$oManifestRef}{&MANIFEST_SECTION_BACKUP_TARGET}{$strManifestKey}{&MANIFEST_SUBKEY_TYPE} = MANIFEST_VALUE_LINK;

    # Create the link
    my $strDbFile = $self->dbLinkCreate($oManifestRef, $strPath, $strFile, $strDestination);

    # Stat the link
    my $oStat = lstat($strDbFile);

    # Check for errors in stat
    if (!defined($oStat))
    {
        confess 'unable to stat ${strDbFile}';
    }

    # Load file into manifest
    ${$oManifestRef}{&MANIFEST_SECTION_TARGET_LINK}{$strManifestKey}{&MANIFEST_SUBKEY_GROUP} = getgrgid($oStat->gid);
    ${$oManifestRef}{&MANIFEST_SECTION_TARGET_LINK}{$strManifestKey}{&MANIFEST_SUBKEY_USER} = getpwuid($oStat->uid);
    ${$oManifestRef}{&MANIFEST_SECTION_TARGET_LINK}{$strManifestKey}{&MANIFEST_SUBKEY_DESTINATION} = $strDestination;

    # Stat what the link is pointing to
    my $strDestinationFile = $strDestination;

    if (index($strDestinationFile, '/') != 0)
    {
        $strDestinationFile = ${$oManifestRef}{&MANIFEST_SECTION_BACKUP_TARGET}{&MANIFEST_TARGET_PGDATA}{&MANIFEST_SUBKEY_PATH} . '/' .
                              (defined(dirname($strPath)) ? dirname($strPath) : '') . "/${strDestination}";
    }

    $oStat = lstat($strDestinationFile);

    my $strSection = MANIFEST_SECTION_TARGET_PATH;

    if (S_ISREG($oStat->mode))
    {
        $strSection = MANIFEST_SECTION_TARGET_FILE;
        ${$oManifestRef}{$strSection}{$strManifestKey}{&MANIFEST_SUBKEY_SIZE} = $oStat->size;
        ${$oManifestRef}{$strSection}{$strManifestKey}{&MANIFEST_SUBKEY_TIMESTAMP} = $oStat->mtime;
        (${$oManifestRef}{$strSection}{$strManifestKey}{&MANIFEST_SUBKEY_CHECKSUM}) = storageTest()->hashSize($strDestinationFile);
        ${$oManifestRef}{$strSection}{$strManifestKey}{&MANIFEST_SUBKEY_MASTER} =
            defined($bMaster) ? ($bMaster ? JSON::PP::true : JSON::PP::false) : JSON::PP::false;

        ${$oManifestRef}{&MANIFEST_SECTION_BACKUP_TARGET}{$strManifestKey}{&MANIFEST_SUBKEY_FILE} =
            basename(${$oManifestRef}{&MANIFEST_SECTION_BACKUP_TARGET}{$strManifestKey}{&MANIFEST_SUBKEY_PATH});
        ${$oManifestRef}{&MANIFEST_SECTION_BACKUP_TARGET}{$strManifestKey}{&MANIFEST_SUBKEY_PATH} =
            dirname(${$oManifestRef}{&MANIFEST_SECTION_BACKUP_TARGET}{$strManifestKey}{&MANIFEST_SUBKEY_PATH});
    }
    # Allow a link to a link to be created to test that backrest errors out correctly
    elsif (S_ISLNK($oStat->mode))
    {
        $strSection = MANIFEST_SECTION_TARGET_LINK;
        ${$oManifestRef}{$strSection}{$strManifestKey}{&MANIFEST_SUBKEY_DESTINATION} = $strDestination;
    }
    elsif (!S_ISDIR($oStat->mode))
    {
        confess &log(ASSERT, "unrecognized file type for file $strDestinationFile");
    }

    ${$oManifestRef}{$strSection}{$strManifestKey}{&MANIFEST_SUBKEY_GROUP} = getgrgid($oStat->gid);
    ${$oManifestRef}{$strSection}{$strManifestKey}{&MANIFEST_SUBKEY_USER} = getpwuid($oStat->uid);
    ${$oManifestRef}{$strSection}{$strManifestKey}{&MANIFEST_SUBKEY_MODE} = sprintf('%04o', S_IMODE($oStat->mode));
}

####################################################################################################################################
# manifestLinkMap
#
# Remap links to new directories/files
####################################################################################################################################
sub manifestLinkMap
{
    my $self = shift;
    my $oManifestRef = shift;
    my $strTarget = shift;
    my $strDestination = shift;

    if ($$oManifestRef{&MANIFEST_SECTION_BACKUP_TARGET}{$strTarget}{&MANIFEST_SUBKEY_TYPE} ne MANIFEST_VALUE_LINK)
    {
        confess "cannot map target ${strTarget} because it is not a link";
    }

    if (defined($$oManifestRef{&MANIFEST_SECTION_BACKUP_TARGET}{$strTarget}{&MANIFEST_SUBKEY_TABLESPACE_ID}))
    {
        confess "tablespace ${strTarget} cannot be remapped with this function";
    }

    if (defined($strDestination))
    {
        confess "GENERAL LINK REMAP NOT IMPLEMENTED";
    }
    else
    {
        delete($$oManifestRef{&MANIFEST_SECTION_BACKUP_TARGET}{$strTarget});
        delete($$oManifestRef{&MANIFEST_SECTION_TARGET_LINK}{$strTarget});
    }
}

####################################################################################################################################
# manifestLinkRemove
#
# Create a link and add it to the manifest.
####################################################################################################################################
sub manifestLinkRemove
{
    my $self = shift;
    my $oManifestRef = shift;
    my $strPath = shift;
    my $strFile = shift;

    # Delete the link
    my $strDbFile = $self->dbFileRemove($oManifestRef, $strPath, $strFile);

    # Determine the manifest key
    my $strManifestKey = $self->manifestKeyGet($oManifestRef, $strPath, $strFile);

    # Delete from manifest
    delete(${$oManifestRef}{&MANIFEST_SECTION_BACKUP_TARGET}{$strManifestKey});
    delete(${$oManifestRef}{&MANIFEST_SECTION_TARGET_LINK}{$strManifestKey});
    delete(${$oManifestRef}{&MANIFEST_SECTION_TARGET_FILE}{$strManifestKey});
    delete(${$oManifestRef}{&MANIFEST_SECTION_TARGET_PATH}{$strManifestKey});
}

####################################################################################################################################
# manifestPathCreate
#
# Create a path specifying mode and add it to the manifest.
####################################################################################################################################
sub manifestPathCreate
{
    my $self = shift;
    my $oManifestRef = shift;
    my $strPath = shift;
    my $strSubPath = shift;
    my $strMode = shift;

    # Determine the manifest key
    my $strManifestKey = $self->manifestKeyGet($oManifestRef, $strPath, $strSubPath);

    # Create the db path
    my $strDbPath = $self->dbPathCreate($oManifestRef, $strPath, $strSubPath, defined($strMode) ? $strMode : '0700');

    # Stat the file
    my $oStat = lstat($strDbPath);

    # Check for errors in stat
    if (!defined($oStat))
    {
        confess 'unable to stat ${strSubPath}';
    }

    # Load file into manifest
    my $strSection = MANIFEST_SECTION_TARGET_PATH;

    ${$oManifestRef}{$strSection}{$strManifestKey}{&MANIFEST_SUBKEY_GROUP} = getgrgid($oStat->gid);
    ${$oManifestRef}{$strSection}{$strManifestKey}{&MANIFEST_SUBKEY_USER} = getpwuid($oStat->uid);
    ${$oManifestRef}{$strSection}{$strManifestKey}{&MANIFEST_SUBKEY_MODE} = sprintf('%04o', S_IMODE($oStat->mode));
}

####################################################################################################################################
# manifestReference
#
# Update all files that do not have a reference with the supplied reference.
####################################################################################################################################
sub manifestReference
{
    my $self = shift;
    my $oManifestRef = shift;
    my $strReference = shift;
    my $bClear = shift;

    # Set prior backup
    if (defined($strReference))
    {
        ${$oManifestRef}{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_PRIOR} = $strReference;
    }
    else
    {
        delete(${$oManifestRef}{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_PRIOR});
    }

    # Find all file sections
    foreach my $strSectionFile (sort(keys(%$oManifestRef)))
    {
        # Skip non-file sections
        if ($strSectionFile !~ /\:file$/)
        {
            next;
        }

        foreach my $strFile (sort(keys(%{${$oManifestRef}{$strSectionFile}})))
        {
            if (!defined($strReference))
            {
                delete(${$oManifestRef}{$strSectionFile}{$strFile}{&MANIFEST_SUBKEY_REFERENCE});
            }
            elsif (defined($bClear) && $bClear)
            {
                if (defined(${$oManifestRef}{$strSectionFile}{$strFile}{&MANIFEST_SUBKEY_REFERENCE}) &&
                    ${$oManifestRef}{$strSectionFile}{$strFile}{&MANIFEST_SUBKEY_REFERENCE} ne $strReference)
                {
                    delete(${$oManifestRef}{$strSectionFile}{$strFile}{&MANIFEST_SUBKEY_REFERENCE});
                }
            }
            elsif (!defined(${$oManifestRef}{$strSectionFile}{$strFile}{&MANIFEST_SUBKEY_REFERENCE}))
            {
                ${$oManifestRef}{$strSectionFile}{$strFile}{&MANIFEST_SUBKEY_REFERENCE} = $strReference;
            }
        }
    }
}

####################################################################################################################################
# manifestTablespaceCreate
#
# Create a tablespace specifying mode and add it to the manifest.
####################################################################################################################################
sub manifestTablespaceCreate
{
    my $self = shift;
    my $oManifestRef = shift;
    my $iOid = shift;
    my $strMode = shift;

    # Load linked path into manifest
    my $strLinkPath = $self->tablespacePath($iOid);
    my $strTarget = MANIFEST_TARGET_PGTBLSPC . "/${iOid}";
    my $oStat = lstat($strLinkPath);

    ${$oManifestRef}{&MANIFEST_SECTION_TARGET_PATH}{$strTarget}{&MANIFEST_SUBKEY_GROUP} = getgrgid($oStat->gid);
    ${$oManifestRef}{&MANIFEST_SECTION_TARGET_PATH}{$strTarget}{&MANIFEST_SUBKEY_USER} = getpwuid($oStat->uid);
    ${$oManifestRef}{&MANIFEST_SECTION_TARGET_PATH}{$strTarget}{&MANIFEST_SUBKEY_MODE} =
        sprintf('%04o', S_IMODE($oStat->mode));

    # Create the tablespace path if it does not exist
    my $strTablespacePath = $strLinkPath;
    my $strPathTarget = $strTarget;

    if ($$oManifestRef{&MANIFEST_SECTION_BACKUP_DB}{&MANIFEST_KEY_DB_VERSION} >= PG_VERSION_90)
    {
        my $iCatalog = ${$oManifestRef}{&MANIFEST_SECTION_BACKUP_DB}{&MANIFEST_KEY_CATALOG};
        my $strTablespaceId = 'PG_' . ${$oManifestRef}{&MANIFEST_SECTION_BACKUP_DB}{&MANIFEST_KEY_DB_VERSION} . "_${iCatalog}";

        $strTablespacePath .= "/${strTablespaceId}";
        $strPathTarget .= "/${strTablespaceId}";
    }

    if (!-e $strTablespacePath)
    {
        storageTest()->pathCreate($strTablespacePath, {strMode => defined($strMode) ? $strMode : '0700'});
    }

    # Load tablespace path into manifest
    $oStat = lstat($strTablespacePath);

    ${$oManifestRef}{&MANIFEST_SECTION_TARGET_PATH}{&MANIFEST_TARGET_PGTBLSPC} =
        ${$oManifestRef}{&MANIFEST_SECTION_TARGET_PATH}{&MANIFEST_TARGET_PGDATA};

    ${$oManifestRef}{&MANIFEST_SECTION_TARGET_PATH}{$strPathTarget}{&MANIFEST_SUBKEY_GROUP} = getgrgid($oStat->gid);
    ${$oManifestRef}{&MANIFEST_SECTION_TARGET_PATH}{$strPathTarget}{&MANIFEST_SUBKEY_USER} = getpwuid($oStat->uid);
    ${$oManifestRef}{&MANIFEST_SECTION_TARGET_PATH}{$strPathTarget}{&MANIFEST_SUBKEY_MODE} =
        sprintf('%04o', S_IMODE($oStat->mode));

    # Create the link in DB_PATH_PGTBLSPC
    my $strLink = $self->dbBasePath() . '/' . DB_PATH_PGTBLSPC . "/${iOid}";

    symlink($strLinkPath, $strLink)
        or confess "unable to link ${strLink} to ${strLinkPath}";

    # Load link into the manifest
    $oStat = lstat($strLink);
    my $strLinkTarget = MANIFEST_TARGET_PGDATA . "/${strTarget}";

    ${$oManifestRef}{&MANIFEST_SECTION_TARGET_LINK}{$strLinkTarget}{&MANIFEST_SUBKEY_GROUP} = getgrgid($oStat->gid);
    ${$oManifestRef}{&MANIFEST_SECTION_TARGET_LINK}{$strLinkTarget}{&MANIFEST_SUBKEY_USER} = getpwuid($oStat->uid);
    ${$oManifestRef}{&MANIFEST_SECTION_TARGET_LINK}{$strLinkTarget}{&MANIFEST_SUBKEY_DESTINATION} = $strLinkPath;

    # Load tablespace target into the manifest
    ${$oManifestRef}{&MANIFEST_SECTION_BACKUP_TARGET}{$strTarget}{&MANIFEST_SUBKEY_PATH} = $strLinkPath;
    ${$oManifestRef}{&MANIFEST_SECTION_BACKUP_TARGET}{$strTarget}{&MANIFEST_SUBKEY_TYPE} = MANIFEST_VALUE_LINK;
    ${$oManifestRef}{&MANIFEST_SECTION_BACKUP_TARGET}{$strTarget}{&MANIFEST_SUBKEY_TABLESPACE_ID} = $iOid;
    ${$oManifestRef}{&MANIFEST_SECTION_BACKUP_TARGET}{$strTarget}{&MANIFEST_SUBKEY_TABLESPACE_NAME} = "ts${iOid}";
}

####################################################################################################################################
# manifestTablespaceDrop
#
# Drop a tablespace add remove it from the manifest.
####################################################################################################################################
sub manifestTablespaceDrop
{
    my $self = shift;
    my $oManifestRef = shift;
    my $iOid = shift;
    my $iIndex = shift;

    # Remove tablespace path/file/link from manifest
    my $strTarget = DB_PATH_PGTBLSPC . "/${iOid}";

    # Remove manifest path, link, target
    delete(${$oManifestRef}{&MANIFEST_SECTION_BACKUP_TARGET}{$strTarget});
    delete(${$oManifestRef}{&MANIFEST_SECTION_TARGET_LINK}{&MANIFEST_TARGET_PGDATA . "/${strTarget}"});
    delete(${$oManifestRef}{&MANIFEST_SECTION_TARGET_PATH}{$strTarget});

    # Remove nested manifest files and paths
    foreach my $strSection (&MANIFEST_SECTION_TARGET_PATH, &MANIFEST_SECTION_TARGET_FILE)
    {
        foreach my $strFile (keys(%{${$oManifestRef}{$strSection}}))
        {
            if (index($strFile, "${strTarget}/") == 0)
            {
                delete($$oManifestRef{$strSection}{$strFile});
            }
        }
    }

    # Drop the link in DB_PATH_PGTBLSPC
    testFileRemove($self->dbBasePath($iIndex) . "/${strTarget}");
}

####################################################################################################################################
# dbPathCreate
#
# Create a path specifying mode.
####################################################################################################################################
sub dbPathCreate
{
    my $self = shift;
    my $oManifestRef = shift;
    my $strTarget = shift;
    my $strSubPath = shift;
    my $strMode = shift;

    # Create final file location
    my $strFinalPath = ${$oManifestRef}{&MANIFEST_SECTION_BACKUP_TARGET}{$strTarget}{&MANIFEST_SUBKEY_PATH};

    # Get tablespace path if this is a tablespace
    if ($$oManifestRef{&MANIFEST_SECTION_BACKUP_DB}{&MANIFEST_KEY_DB_VERSION} >= PG_VERSION_90 &&
        index($strTarget, DB_PATH_PGTBLSPC . '/') == 0)
    {
        my $iCatalog = ${$oManifestRef}{&MANIFEST_SECTION_BACKUP_DB}{&MANIFEST_KEY_CATALOG};

        $strFinalPath .= '/PG_' . ${$oManifestRef}{&MANIFEST_SECTION_BACKUP_DB}{&MANIFEST_KEY_DB_VERSION} . "_${iCatalog}";
    }

    $strFinalPath .= (defined($strSubPath) ? "/${strSubPath}" : '');

    # Create the path
    if (!(-e $strFinalPath))
    {
        storageTest()->pathCreate($strFinalPath, {strMode => $strMode});
    }

    return $strFinalPath;
}

####################################################################################################################################
# dbPathMode
#
# Change the mode of a path.
####################################################################################################################################
sub dbPathMode
{
    my $self = shift;
    my $oManifestRef = shift;
    my $strTarget = shift;
    my $strPath = shift;
    my $strMode = shift;

    # Get the db path
    my $strDbPath = $self->manifestDbPathGet($oManifestRef, $strTarget, $strPath);

    testPathMode($strDbPath, $strMode);

    return $strDbPath;
}

####################################################################################################################################
# dbPathRemove
#
# Remove a path.
####################################################################################################################################
sub dbPathRemove
{
    my $self = shift;
    my $oManifestRef = shift;
    my $strTarget = shift;
    my $strPath = shift;

    # Get the db path
    my $strDbPath = $self->manifestDbPathGet($oManifestRef, $strTarget, $strPath);

    # Create the path
    testPathRemove($strDbPath);

    return $strDbPath;
}

1;
