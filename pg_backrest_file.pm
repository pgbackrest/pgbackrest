####################################################################################################################################
# FILE MODULE
####################################################################################################################################
package pg_backrest_file;

use strict;
use warnings;
use Carp;
use Net::OpenSSH;
use IPC::Open3;
use File::Basename;
use IPC::System::Simple qw(capture);

use lib dirname($0);
use pg_backrest_utility;

use Exporter qw(import);

our @EXPORT = qw(path_get path_type_get link_create path_create file_copy file_list_get manifest_get file_hash_get
                 PATH_DB PATH_DB_ABSOLUTE PATH_BACKUP PATH_BACKUP_CLUSTER PATH_BACKUP_TMP PATH_BACKUP_ARCHIVE);

# Extension and permissions
our $strCompressExtension = "gz";
our $strDefaultPathPermission = "0750";

# Command strings
our $strCommandChecksum;
our $strCommandCompress;
our $strCommandDecompress;
our $strCommandCopy;
our $strCommandCat = "cat %file%";
our $strCommandManifest;
our $strCommandPsql;

# Global variables
our $strDbHost;                  # Database host
our $oDbSSH;                     # Database SSH object
our $strDbClusterPath;           # Database cluster base path

our $strBackupHost;              # Backup host
our $oBackupSSH;                 # Backup SSH object
our $strBackupPath;              # Backup base path
our $strBackupClusterPath;       # Backup cluster path

# Process flags
our $bNoCompression;

####################################################################################################################################
# INIT
####################################################################################################################################
sub init
{
    
}

####################################################################################################################################
# PATH_GET
####################################################################################################################################
use constant 
{
    PATH_DB             => 'db',
    PATH_DB_ABSOLUTE    => 'db:absolute',
    PATH_BACKUP         => 'backup',
    PATH_BACKUP_CLUSTER => 'backup:cluster',
    PATH_BACKUP_TMP     => 'backup:tmp',
    PATH_BACKUP_ARCHIVE => 'backup:archive'
};

sub path_type_get
{
    my $strType = shift;
    
    # If db type
    if ($strType =~ /^db(\:.*){0,1}/)
    {
        return PATH_DB;
    }
    # Else if backup type
    elsif ($strType =~ /^backup(\:.*){0,1}/)
    {
        return PATH_BACKUP;
    }
    
    # Error when path type not recognized
    confess &log(ASSERT, "no known path types in '${strType}'");
}

sub path_get
{
    my $strType = shift;
    my $strFile = shift;
    my $bTemp = shift;

    if (defined($bTemp) && $bTemp && !($strType eq PATH_BACKUP_ARCHIVE || $strType eq PATH_BACKUP_TMP))
    {
        confess &log(ASSERT, "temp file not supported on path " . $strType);
    }

    # Parse paths on the db side
    if ($strType eq PATH_DB_ABSOLUTE)
    {
        return $strFile;
    }
    # Parse paths on the backup side
    elsif ($strType eq PATH_BACKUP)
    {
        if (!defined($strBackupPath))
        {
            confess &log(ASSERT, "\$strBackupPath not yet defined");
        }
        
        return $strBackupPath;
    }
    elsif ($strType eq PATH_BACKUP_CLUSTER || $strType eq PATH_BACKUP_TMP || $strType eq PATH_BACKUP_ARCHIVE)
    {
        if (!defined($strBackupClusterPath))
        {
            confess &log(ASSERT, "\$strBackupClusterPath not yet defined");
        }
        
        if ($strType eq PATH_BACKUP_CLUSTER)
        {
            return $strBackupClusterPath . (defined($strFile) ? "/${strFile}" : "");
        }
        elsif ($strType eq PATH_BACKUP_TMP)
        {
            if (defined($bTemp) && $bTemp)
            {
                return $strBackupClusterPath . "/backup.tmp/file.tmp";
            }
            
            return $strBackupClusterPath . "/backup.tmp" . (defined($strFile) ? "/${strFile}" : "");
        }
        elsif ($strType eq PATH_BACKUP_ARCHIVE)
        {
            if (defined($bTemp) && $bTemp)
            {
                return $strBackupClusterPath . "/archive/archive.tmp";
            }
            else
            {
                my $strArchive;
            
                if (defined($strFile))
                {
                    $strArchive = substr($strFile, 0, 24);
            
                    if ($strArchive !~ /^([0-F]){24}$/)
                    {
                        croak &log(ERROR, "$strFile not a valid archive file");
                    }
                }
            
                return $strBackupClusterPath . "/archive" . (defined($strArchive) ? "/" . 
                       substr($strArchive, 0, 16) : "") . "/" . $strFile;
            }
        }
    }

    # Error when path type not recognized
    confess &log(ASSERT, "no known path types in '${strType}'");
}

####################################################################################################################################
# LINK_CREATE
####################################################################################################################################
sub link_create
{
    my $strSourcePathType = shift;
    my $strSourceFile = shift;
    my $strDestinationPathType = shift;
    my $strDestinationFile = shift;
    my $bHard = shift;
    my $bRelative = shift;
    
    my $strSource = path_get($strSourcePathType, $strSourceFile);
    my $strDestination = path_get($strDestinationPathType, $strDestinationFile);

    # If the destination path does not exist, create it
    unless ($strDestinationPathType =~ /^backup\:.*/ and -e dirname($strDestination))
    {
        path_create(undef, dirname($strDestination), $strDefaultPathPermission);
    }

    unless (-e $strSource)
    {
        if (-e $strSource . ".${strCompressExtension}")
        {
            $strSource .= ".${strCompressExtension}";
            $strDestination .= ".${strCompressExtension}";
        }
        else
        {
            confess &log(ASSERT, "unable to find ${strSource}(.${strCompressExtension}) for checksum");
        }
    }
    
    # Create the link
    my $strCommand = "ln";
    
    if (!defined($bHard) || !$bHard)
    {
        $strCommand .= " -s";
    }
    
    $strCommand .= " ${strSource} ${strDestination}";
    &log(DEBUG, "        link_create: ${strCommand}");
    system($strCommand) == 0 or confess &log("unable to create link from ${strSource} to ${strDestination}");
}

####################################################################################################################################
# PATH_CREATE
####################################################################################################################################
sub path_create
{
    my $strPathType = shift;
    my $strPath = shift;
    my $strPermission = shift;
    
    # If no permission are given then use the default
    if (!defined($strPermission))
    {
        $strPermission = "0750";
    }

    # Get the path to create
    my $strPathCreate = $strPath;
    
    if (defined($strPathType))
    {
        $strPathCreate = path_get($strPathType, $strPath);
    }

    # Create the path
    system("mkdir -p -m ${strPermission} ${strPathCreate}") == 0 or confess &log(ERROR, "unable to create path ${strPathCreate}");
}

####################################################################################################################################
# IS_REMOTE
#
# Determine whether any operations are being performed remotely.  If $strPathType is defined, the function will return true if that
# path is remote.  If $strPathType is not defined, then function will return true if any path is remote.
####################################################################################################################################
sub is_remote
{
    my $strPathType = shift;
    
    # If the SSH object is defined then some paths are remote
    if (defined($oDbSSH) || defined($oBackupSSH))
    {
        # If path type is not defined but the SSH object is, then some paths are remote
        if (!defined($strPathType))
        {
            return true;
        }
    
        # If a host is defined for the path then it is remote
        if (defined($strBackupHost) && path_type_get($strPathType) eq PATH_BACKUP ||
            defined($strDbHost) && path_type_get($strPathType) eq PATH_DB)
        {
            return true;
        }
    }

    return false;
}

####################################################################################################################################
# REMOTE_GET
#
# Get remote SSH object depending on the path type.
####################################################################################################################################
sub remote_get
{
    my $strPathType = shift;
    
    # If the SSH object is defined then some paths are remote
    if (path_type_get($strPathType) eq PATH_DB && defined($oDbSSH))
    {
        return $oDbSSH;
    }

    if (path_type_get($strPathType) eq PATH_BACKUP && defined($oBackupSSH))
    {
        return $oBackupSSH
    }

    confess &log(ASSERT, "path type ${strPathType} does not have a defined ssh object");
}

####################################################################################################################################
# FILE_COPY
####################################################################################################################################
sub file_copy
{
    my $strSourcePathType = shift;
    my $strSourceFile = shift;
    my $strDestinationPathType = shift;
    my $strDestinationFile = shift;
    my $bNoCompressionOverride = shift;

    # Generate source, destination and tmp filenames
    my $strSource = path_get($strSourcePathType, $strSourceFile);
    my $strDestination = path_get($strDestinationPathType, $strDestinationFile);
    my $strDestinationTmp = path_get($strDestinationPathType, $strDestinationFile, true);

    # Is this already a compressed file?
    my $bAlreadyCompressed = $strSource =~ "^.*\.${strCompressExtension}\$";

    if ($bAlreadyCompressed && $strDestination !~ "^.*\.${strCompressExtension}\$")
    {
        $strDestination .= ".${strCompressExtension}";
    }

    # Does the file need compression?
    my $bCompress = !($bAlreadyCompressed ||
                      (defined($bNoCompressionOverride) && $bNoCompressionOverride) ||
                      (!defined($bNoCompressionOverride) && $bNoCompression));

    # If the destination file is to be compressed add the extenstion
    $strDestination .= $bCompress ? ".gz" : "";
                      
    # If the destination path is backup and does not exist, create it
    unless ($strDestinationPathType =~ /^backup\:.*/ and -e dirname($strDestination))
    {
        path_create(undef, dirname($strDestination), $strDefaultPathPermission);
    }

    # Generate the command string depending on compression/copy
    my $strCommand = $bCompress ? $strCommandCompress : $strCommandCat;
    $strCommand =~ s/\%file\%/${strSource}/g;

    # If this command is remote on only one side
    if (is_remote($strSourcePathType) && !is_remote($strDestinationPathType) ||
        !is_remote($strSourcePathType) && is_remote($strDestinationPathType))
    {
        # Else if the source is remote
        if (is_remote($strSourcePathType))
        {
            &log(DEBUG, "        file_copy: remote ${strSource} to local ${strDestination}");

            # Open the destination file for writing (will be streamed from the ssh session)
            my $hFile;
            open($hFile, ">", $strDestinationTmp) or confess &log(ERROR, "cannot open ${strDestination}");

            # Execute the command through ssh
            my $oSSH = remote_get($strSourcePathType);
            $oSSH->system({stdout_fh => $hFile}, $strCommand) or confess &log(ERROR, "unable to execute ssh '$strCommand'");

            # Close the destination file handle
            close($hFile) or confess &log(ERROR, "cannot close file");
        }
        # Else if the destination is remote
        elsif (is_remote($strDestinationPathType))
        {
            &log(DEBUG, "        file_copy: local ${strSource} ($strCommand) to remote ${strDestination}");

            # Open the input command as a stream
            my $hOut;
            my $pId = open3(undef, $hOut, undef, $strCommand) or confess(ERROR, "unable to execute '${strCommand}'");

            # Execute the command though ssh
            my $oSSH = remote_get($strDestinationPathType);
            $oSSH->system({stdin_fh => $hOut}, "cat > ${strDestinationTmp}") or confess &log(ERROR, "unable to execute ssh 'cat'");

            # Wait for the stream process to finish
            waitpid($pId, 0);
            my $iExitStatus = ${^CHILD_ERROR_NATIVE} >> 8;
            
            if ($iExitStatus != 0)
            {
                confess &log(ERROR, "command '${strCommand}' returned", $iExitStatus);
            }
        }
    }
    # If the source and destination are both remote but not the same remote
    elsif (is_remote($strSourcePathType) && is_remote($strDestinationPathType) &&
           path_type_get($strSourcePathType) ne path_type_get($strDestinationPathType))
    {
        &log(DEBUG, "        file_copy: remote ${strSource} to remote ${strDestination}");
        confess &log(ASSERT, "remote source and destination not supported");
    }
    # Else this is a local command or remote where both sides are the same remote
    else
    {
        # Complete the command by redirecting to the destination tmp file
        $strCommand .= " > ${strDestinationTmp}";

        if (is_remote())
        {
            &log(DEBUG, "        file_copy: remote ${strSourcePathType} '${strCommand}'");

            my $oSSH = remote_get($strSourcePathType);
            $oSSH->system($strCommand) or confess &log(ERROR, "unable to execute remote command ${strCommand}:" . oSSH->error);
        }
        else
        {
            &log(DEBUG, "        file_copy: local '${strCommand}'");

            system($strCommand) == 0 or confess &log(ERROR, "unable to copy local $strSource to local $strDestinationTmp");
        }
    }
    
    # Move the file from tmp to final destination
    rename($strDestinationTmp, $strDestination) or die &log(ERROR, "unable to move $strDestinationTmp to $strDestination");
}

####################################################################################################################################
# FILE_HASH_GET
####################################################################################################################################
sub file_hash_get
{
    my $strPathType = shift;
    my $strFile = shift;
    
    if (!defined($strCommandChecksum))
    {
        confess &log(ASSERT, "\$strCommandChecksum not defined");
    }
    
    my $strPath = path_get($strPathType, $strFile);
    my $strCommand;
    
    if (-e $strPath)
    {
        $strCommand = $strCommandChecksum;
        $strCommand =~ s/\%file\%/$strFile/g;
    }
    elsif (-e $strPath . ".${strCompressExtension}")
    {
        $strCommand = $strCommandDecompress;
        $strCommand =~ s/\%file\%/${strPath}/g;
        $strCommand .= " | " . $strCommandChecksum;
        $strCommand =~ s/\%file\%//g;
    }
    else
    {
        confess &log(ASSERT, "unable to find $strPath(.${strCompressExtension}) for checksum");
    }
    
    return trim(capture($strCommand)) or confess &log(ERROR, "unable to checksum ${strPath}");
}

####################################################################################################################################
# FILE_LIST_GET
####################################################################################################################################
sub file_list_get
{
    my $strPath = shift;
    my $strExpression = shift;
    my $strSortOrder = shift;
    
    my $hDir;
    
    opendir $hDir, $strPath or die &log(ERROR, "unable to open path ${strPath}");
    my @stryFileAll = readdir $hDir or die &log(ERROR, "unable to get files for path ${strPath}, expression ${strExpression}");
    close $hDir;
    
    my @stryFile;

    if (@stryFileAll)
    {
        @stryFile = grep(/$strExpression/i, @stryFileAll)
    }
    
    if (@stryFile)
    {
        if (defined($strSortOrder) && $strSortOrder eq "reverse")
        {
            return sort {$b cmp $a} @stryFile;
        }
        else
        {
            return sort @stryFile;
        }
    }
    
    return @stryFile;
}

####################################################################################################################################
# MANIFEST_GET
#
# Builds a path/file manifest starting with the base path and including all subpaths.  The manifest contains all the information
# needed to perform a backup or a delta with a previous backup.
####################################################################################################################################
sub manifest_get
{
    my $strPathType = shift;
    my $strPath = shift;

    &log(DEBUG, "manifest: " . $strCommandManifest);

    # Get the root path for the manifest
    my $strPathManifest = path_get($strPathType, $strPath);

    # Builds the manifest command
    my $strCommand = $strCommandManifest;
    $strCommand =~ s/\%path\%/${strPathManifest}/g;
    
    # Run the manifest command
    my $strManifest;

    # Run remotely
    if (is_remote($strPathType))
    {
        &log(DEBUG, "        manifest_get: remote ${strPathType}:${strPathManifest}");

        my $oSSH = remote_get($strPathType);
        $strManifest = $oSSH->capture($strCommand) or confess &log(ERROR, "unable to execute remote command '${strCommand}'");
    }
    # Run locally
    else
    {
        &log(DEBUG, "        manifest_get: local ${strPathType}:${strPathManifest}");
        $strManifest = capture($strCommand) or confess &log(ERROR, "unable to execute local command '${strCommand}'");
    }

    # Load the manifest into a hash
    return data_hash_build("name\ttype\tuser\tgroup\tpermission\tmodification_time\tinode\tsize\tlink_destination\n" .
                           $strManifest, "\t", ".");
}

1;
