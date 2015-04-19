####################################################################################################################################
# RESTORE FILE MODULE
####################################################################################################################################
package BackRest::RestoreFile;

use threads;
use threads::shared;
use Thread::Queue;
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use File::Basename qw(dirname);
use File::stat qw(lstat);
use Exporter qw(import);

use lib dirname($0);
use BackRest::Exception;
use BackRest::Utility;
use BackRest::Config;
use BackRest::Manifest;
use BackRest::File;

####################################################################################################################################
# restoreFile
#
# Restores a single file.
####################################################################################################################################
sub restoreFile
{
    my $strSourcePath = shift;      # Source path of the file
    my $strFileName = shift;        # File to restore
    my $lCopyTimeBegin = shift;     # Time that the backup begain - used for size/timestamp deltas
    my $bDelta = shift;             # Is restore a delta?
    my $bForce = shift;             # Force flag
    my $strBackupPath = shift;      # Backup path
    my $bSourceCompression = shift; # Is the source compressed?
    my $strCurrentUser = shift;     # Current OS user
    my $strCurrentGroup = shift;    # Current OS group
    my $oManifest = shift;          # Backup manifest
    my $oFile = shift;              # File object (only provided in single-threaded mode)

    my $strSection = "${strSourcePath}:file";                                 # Backup section with file info
    my $strDestinationPath = $oManifest->get(MANIFEST_SECTION_BACKUP_PATH,    # Destination path stored in manifest
                                             $strSourcePath);
    $strSourcePath =~ s/\:/\//g;                                              # Replace : with / in source path

    # If the file is a reference to a previous backup and hardlinks are off, then fetch it from that backup
    my $strReference = $oManifest->test(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_HARDLINK, undef, 'y') ? undef :
                           $oManifest->get($strSection, $strFileName, MANIFEST_SUBKEY_REFERENCE, false);

    # Generate destination file name
    my $strDestinationFile = $oFile->path_get(PATH_DB_ABSOLUTE, "${strDestinationPath}/${strFileName}");

    if ($oFile->exists(PATH_DB_ABSOLUTE, $strDestinationFile))
    {
        # Perform delta if requested
        if ($bDelta)
        {
            # If force then use size/timestamp delta
            if ($bForce)
            {
                my $oStat = lstat($strDestinationFile);

                # Make sure that timestamp/size are equal and that timestamp is before the copy start time of the backup
                if (defined($oStat) &&
                    $oStat->size == $oManifest->get($strSection, $strFileName, MANIFEST_SUBKEY_SIZE) &&
                    $oStat->mtime == $oManifest->get($strSection, $strFileName, MANIFEST_SUBKEY_MODIFICATION_TIME) &&
                    $oStat->mtime < $lCopyTimeBegin)
                {
                    &log(DEBUG, "${strDestinationFile} exists and matches size " . $oStat->size .
                                " and modification time " . $oStat->mtime);
                    return;
                }
            }
            else
            {
                my ($strChecksum, $lSize) = $oFile->hash_size(PATH_DB_ABSOLUTE, $strDestinationFile);
                my $strManifestChecksum = $oManifest->get($strSection, $strFileName, MANIFEST_SUBKEY_CHECKSUM, false, 'INVALID');

                if (($lSize == $oManifest->get($strSection, $strFileName, MANIFEST_SUBKEY_SIZE) && $lSize == 0) ||
                    ($strChecksum eq $strManifestChecksum))
                {
                    &log(DEBUG, "${strDestinationFile} exists and is zero size or matches backup checksum");

                    # Even if hash is the same set the time back to backup time.  This helps with unit testing, but also
                    # presents a pristine version of the database.
                    utime($oManifest->get($strSection, $strFileName, MANIFEST_SUBKEY_MODIFICATION_TIME),
                          $oManifest->get($strSection, $strFileName, MANIFEST_SUBKEY_MODIFICATION_TIME),
                          $strDestinationFile)
                        or confess &log(ERROR, "unable to set time for ${strDestinationFile}");

                    return;
                }
            }
        }

        $oFile->remove(PATH_DB_ABSOLUTE, $strDestinationFile);
    }

    # Set user and group if running as root (otherwise current user and group will be used for restore)
    # Copy the file from the backup to the database
    my ($bCopyResult, $strCopyChecksum, $lCopySize) =
        $oFile->copy(PATH_BACKUP_CLUSTER, (defined($strReference) ? $strReference : $strBackupPath) .
                     "/${strSourcePath}/${strFileName}" .
                     ($bSourceCompression ? '.' . $oFile->{strCompressExtension} : ''),
                     PATH_DB_ABSOLUTE, $strDestinationFile,
                     $bSourceCompression,   # Source is compressed based on backup settings
                     undef, undef,
                     $oManifest->get($strSection, $strFileName, MANIFEST_SUBKEY_MODIFICATION_TIME),
                     $oManifest->get($strSection, $strFileName, MANIFEST_SUBKEY_MODE),
                     undef,
                     $oManifest->get($strSection, $strFileName, MANIFEST_SUBKEY_USER),
                     $oManifest->get($strSection, $strFileName, MANIFEST_SUBKEY_GROUP));

    if ($lCopySize != 0 && $strCopyChecksum ne $oManifest->get($strSection, $strFileName, MANIFEST_SUBKEY_CHECKSUM))
    {
        confess &log(ERROR, "error restoring ${strDestinationFile}: actual checksum ${strCopyChecksum} " .
                            "does not match expected checksum " .
                            $oManifest->get($strSection, $strFileName, MANIFEST_SUBKEY_CHECKSUM), ERROR_CHECKSUM);
    }
}

our @EXPORT = qw(restoreFile);

1;
