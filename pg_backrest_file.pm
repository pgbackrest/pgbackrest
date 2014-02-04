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

our @EXPORT = qw(file_init_archive file_init_backup
                 path_get path_type_get link_create path_create file_copy file_list_get manifest_get file_hash_get is_remote
                 PATH_DB PATH_DB_ABSOLUTE PATH_BACKUP PATH_BACKUP_ABSOLUTE PATH_BACKUP_CLUSTER PATH_BACKUP_TMP PATH_BACKUP_ARCHIVE);

# Extension and permissions
our $strCompressExtension = "gz";
my $strDefaultPathPermission = "0750";

# Command strings
my $strCommandChecksum;
my $strCommandCompress;
my $strCommandDecompress;
my $strCommandCat = "cat %file%";
my $strCommandManifest;
my $strCommandPsql;

# Module variables
my $strDbHost;                  # Database host
my $oDbSSH;                     # Database SSH object

my $strBackupHost;              # Backup host
my $oBackupSSH;                 # Backup SSH object
my $strBackupPath;              # Backup base path
our $strBackupClusterPath;       # Backup cluster path

# Process flags
my $bNoCompression;

####################################################################################################################################
# FILE_INIT_ARCHIVE
####################################################################################################################################
sub file_init_archive
{
    my $bNoCompressionParam = shift;
    my $strCommandChecksumParam = shift;
    my $strCommandCompressParam = shift;
    my $strCommandDecompressParam = shift;
    my $strBackupHostParam = shift;
    my $strBackupPathParam = shift;
    my $strCluster = shift;
    
    # Assign parameters to module variables
    $bNoCompression = $bNoCompressionParam;
    $strCommandChecksum = $strCommandChecksumParam;
    $strCommandCompress = $strCommandCompressParam;
    $strCommandDecompress = $strCommandDecompressParam;
    $strBackupPath = $strBackupPathParam;
    #$strCluster = $strClusterParam;
    $strBackupHost = $strBackupHostParam;
    
    # Make sure the backup path is defined
    if (!defined($strBackupPath))
    {
        confess &log(ERROR, "common:backup_path undefined");
    }
    
    # Create the backup cluster path
    $strBackupClusterPath = "${strBackupPath}/${strCluster}";

    # Connect SSH object if backup host is defined
    if (defined($strBackupHost))
    {
        &log(INFO, "connecting to backup ssh host ${strBackupHost}");

        # !!! This could be improved by redirecting stderr to a file to get a better error message
        $oBackupSSH = Net::OpenSSH->new($strBackupHost, master_stderr_discard => true);
        $oBackupSSH->error and confess &log(ERROR, "unable to connect to $strBackupHost}: " . $oBackupSSH->error);
    }
}

sub file_init_backup
{
    my $strCommandManifestParam = shift;
    my $strCommandPsqlParam = shift;
    my $strDbHostParam = shift;

    $strCommandManifest = $strCommandManifestParam;
    $strCommandPsql = $strCommandPsqlParam;
    $strDbHost = $strDbHostParam;
    
    # Connect SSH object if db host is defined
    if (defined($strDbHost))
    {
        &log(INFO, "connecting to database ssh host ${strDbHost}");

        # !!! This could be improved by redirecting stderr to a file to get a better error message
        $oDbSSH = Net::OpenSSH->new($strDbHost, master_stderr_discard => true);
        $oDbSSH->error and confess &log(ERROR, "unable to connect to ${strDbHost}: " . $oDbSSH->error);
    }
}

####################################################################################################################################
# PATH_GET
####################################################################################################################################
use constant 
{
    PATH_DB              => 'db',
    PATH_DB_ABSOLUTE     => 'db:absolute',
    PATH_BACKUP          => 'backup',
    PATH_BACKUP_ABSOLUTE => 'backup:absolute',
    PATH_BACKUP_CLUSTER  => 'backup:cluster',
    PATH_BACKUP_TMP      => 'backup:tmp',
    PATH_BACKUP_ARCHIVE  => 'backup:archive'
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
    # Parse absolute db path
    if ($strType eq PATH_BACKUP_ABSOLUTE)
    {
        return $strFile;
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
                    $strArchive = substr(basename($strFile), 0, 24);

                    if ($strArchive !~ /^([0-F]){24}$/)
                    {
                        return $strBackupClusterPath . "/archive/${strFile}";
                    }
                }

                return $strBackupClusterPath . "/archive" . (defined($strArchive) ? "/" . 
                       substr($strArchive, 0, 16) : "") . (defined($strFile) ? "/" . $strFile : "");
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
    
    if (path_type_get($strSourcePathType) ne path_type_get($strDestinationPathType))
    {
        confess &log(ASSERT, "path types must be equal in link create");
    }

    my $strSource = path_get($strSourcePathType, $strSourceFile);
    my $strDestination = path_get($strDestinationPathType, $strDestinationFile);

    # If the destination path is backup and does not exist, create it
    if (path_type_get($strDestinationPathType) eq PATH_BACKUP)
    {
        path_create(PATH_BACKUP_ABSOLUTE, dirname($strDestination));
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
    
    # Run remotely
    if (is_remote($strSourcePathType))
    {
        &log(DEBUG, "        link_create: remote ${strSourcePathType} '${strCommand}'");

        my $oSSH = remote_get($strSourcePathType);
        $oSSH->system($strCommand) or confess &log("unable to create link from ${strSource} to ${strDestination}");
    }
    # Run locally
    else
    {
        &log(DEBUG, "        link_create: local '${strCommand}'");
        system($strCommand) == 0 or confess &log("unable to create link from ${strSource} to ${strDestination}");
    }
}

####################################################################################################################################
# PATH_CREATE
#
# Creates a path locally or remotely.  Currently does not error if the path already exists.  Also does not set permissions if the
# path aleady exists.
####################################################################################################################################
sub path_create
{
    my $strPathType = shift;
    my $strPath = shift;
    my $strPermission = shift;
    
    # If no permissions are given then use the default
    if (!defined($strPermission))
    {
        $strPermission = $strDefaultPathPermission;
    }

    # Get the path to create
    my $strPathCreate = $strPath;
    
    if (defined($strPathType))
    {
        $strPathCreate = path_get($strPathType, $strPath);
    }

    my $strCommand = "mkdir -p -m ${strPermission} ${strPathCreate}";

    # Run remotely
    if (is_remote($strPathType))
    {
        &log(DEBUG, "        path_create: remote ${strPathType} '${strCommand}'");

        my $oSSH = remote_get($strPathType);
        $oSSH->system($strCommand) or confess &log("unable to create remote path ${strPathType}:${strPath}");
    }
    # Run locally
    else
    {
        &log(DEBUG, "        path_create: local '${strCommand}'");
        system($strCommand) == 0 or confess &log(ERROR, "unable to create path ${strPath}");
    }
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
    
    # Get the db SSH object
    if (path_type_get($strPathType) eq PATH_DB && defined($oDbSSH))
    {
        return $oDbSSH;
    }

    # Get the backup SSH object
    if (path_type_get($strPathType) eq PATH_BACKUP && defined($oBackupSSH))
    {
        return $oBackupSSH
    }

    # Error when no ssh object is found
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
    if (path_type_get($strDestinationPathType) eq PATH_BACKUP)
    {
        path_create(PATH_BACKUP_ABSOLUTE, dirname($strDestination));
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

        if (is_remote($strSourcePathType))
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
    rename($strDestinationTmp, $strDestination) or confess &log(ERROR, "unable to move $strDestinationTmp to $strDestination");
}

####################################################################################################################################
# FILE_HASH_GET
####################################################################################################################################
sub file_hash_get
{
    my $strPathType = shift;
    my $strFile = shift;
    
    # For now this operation is not supported remotely.  Not currently needed.
    if (is_remote($strPathType))
    {
        confess &log(ASSERT, "remote operation not supported");
    }
    
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
    my $strPathType = shift;
    my $strPath = shift;
    my $strExpression = shift;
    my $strSortOrder = shift;

    # For now this operation is not supported remotely.  Not currently needed.
    if (is_remote($strPathType))
    {
        confess &log(ASSERT, "remote operation not supported");
    }
    
    my $strPathList = path_get($strPathType, $strPath);
    
    my $hDir;
    
    opendir $hDir, $strPathList or confess &log(ERROR, "unable to open path ${strPathList}");
    my @stryFileAll = readdir $hDir or confess &log(ERROR, "unable to get files for path ${strPathList}, expression ${strExpression}");
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
