####################################################################################################################################
# BACKUP FILE MODULE
####################################################################################################################################
package BackRest::BackupFile;

use threads;
use Thread::Queue;
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
use File::Basename qw(dirname);

use lib dirname($0);
use BackRest::Exception;
use BackRest::File;
use BackRest::Manifest;
use BackRest::Utility;

####################################################################################################################################
# backupFile
####################################################################################################################################
sub backupFile
{
    my $oFile = shift;                  # File object
    my $strSourceFile = shift;          # Source file to backup
    my $strDestinationFile = shift;     # Destination backup file
    my $bDestinationCompress = shift;   # Compress destination file
    my $strChecksum = shift;            # File checksum to be checked
    my $lModificationTime = shift;      # File modification time
    my $lSizeFile = shift;              # File size
    my $lSizeTotal = shift;             # Total size of the files to be copied
    my $lSizeCurrent = shift;           # Size of files copied so far

    my $bCopyResult;                # Copy result
    my $strCopyChecksum;            # Copy checksum
    my $lCopySize;                  # Copy Size

    # Add the size of the current file to keep track of percent complete
    $lSizeCurrent += $lSizeFile;

    # If checksum is defined then the file already exists but needs to be checked
    my $bCopy = true;

    if (defined($strChecksum))
    {
        ($strCopyChecksum, $lCopySize) = $oFile->hash_size(PATH_BACKUP_TMP, $strDestinationFile);

        $bCopy = !($strCopyChecksum eq $strChecksum && $lCopySize == $lSizeFile);

        if ($bCopy)
        {
            &log(WARN, "resumed backup file ${strDestinationFile} should have checksum ${strChecksum} but " .
                       "actually has checksum ${strCopyChecksum}.  The file will be recopied and backup will " .
                       "continue but this may be an issue unless the backup temp path is known to be corrupted.");
        }
    }

    if ($bCopy)
    {
        # Copy the file from the database to the backup (will return false if the source file is missing)
        ($bCopyResult, $strCopyChecksum, $lCopySize) =
            $oFile->copy(PATH_DB_ABSOLUTE, $strSourceFile,
                         PATH_BACKUP_TMP, $strDestinationFile .
                             ($bDestinationCompress ? '.' . $oFile->{strCompressExtension} : ''),
                         false,                   # Source is not compressed since it is the db directory
                         $bDestinationCompress,   # Destination should be compressed based on backup settings
                         true,                    # Ignore missing files
                         $lModificationTime,      # Set modification time - this is required for resume
                         undef,                   # Do not set original mode
                         true);                   # Create the destination directory if it does not exist

        if (!$bCopyResult)
        {
            # If file is missing assume the database removed it (else corruption and nothing we can do!)
            &log(INFO, "skip file removed by database: " . $strSourceFile);

            return false, $lSizeCurrent, undef, undef;
        }
    }

    # Ouput log
    &log(INFO, (defined($strChecksum) && !$bCopy ? 'checksum resumed file' : 'backup file') .
               " $strSourceFile (" . file_size_format($lCopySize) .
               ($lSizeTotal > 0 ? ', ' . int($lSizeCurrent * 100 / $lSizeTotal) . '%' : '') . ')' .
               ($lCopySize != 0 ? " checksum ${strCopyChecksum}" : ''));

    return true, $lSizeCurrent, $lCopySize, $strCopyChecksum;
}

our @EXPORT = qw(backupFile);

####################################################################################################################################
# backupManifestUpdate
####################################################################################################################################
sub backupManifestUpdate
{
    my $oManifest = shift;
    my $strSection = shift;
    my $strFile = shift;
    my $bCopied = shift;
    my $lSize = shift;
    my $strChecksum = shift;
    my $lManifestSaveSize = shift;
    my $lManifestSaveCurrent = shift;

    # If copy was successful store the checksum and size
    if ($bCopied)
    {
        $oManifest->set($strSection, $strFile, MANIFEST_SUBKEY_SIZE, $lSize + 0);

        if ($lSize > 0)
        {
            $oManifest->set($strSection, $strFile, MANIFEST_SUBKEY_CHECKSUM, $strChecksum);
        }

        # Determine whether to save the manifest
        $lManifestSaveCurrent += $lSize;

        if ($lManifestSaveCurrent >= $lManifestSaveSize)
        {
            $oManifest->save();
            &log(DEBUG, 'manifest saved');

            $lManifestSaveCurrent = 0;
        }
    }
    # Else the file was removed during backup so remove from manifest
    else
    {
        $oManifest->remove($strSection, $strFile);
    }

    return $lManifestSaveCurrent;
}

push @EXPORT, qw(backupManifestUpdate);

1;
