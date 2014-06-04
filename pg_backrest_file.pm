####################################################################################################################################
# FILE MODULE
####################################################################################################################################
package pg_backrest_file;

use threads;
use strict;
use warnings;
use Carp;

use Moose;
use Net::OpenSSH;
use IPC::Open3;
use File::Basename;
use IPC::System::Simple qw(capture);
use Digest::SHA;
use File::stat;
use Fcntl ':mode';
use IO::Compress::Gzip qw(gzip $GzipError);

use lib dirname($0);
use pg_backrest_utility;

use Exporter qw(import);
our @EXPORT = qw(PATH_ABSOLUTE PATH_DB PATH_DB_ABSOLUTE PATH_BACKUP PATH_BACKUP_ABSOLUTE PATH_BACKUP_CLUSTER PATH_BACKUP_TMP PATH_BACKUP_ARCHIVE);

# Extension and permissions
has strCompressExtension => (is => 'ro', default => 'gz');
has strDefaultPathPermission => (is => 'bare', default => '0750');
has strDefaultFilePermission => (is => 'ro', default => '0640');

# Command strings
has strCommand => (is => 'bare');
has strCommandChecksum => (is => 'bare');
has strCommandCompress => (is => 'bare');
has strCommandDecompress => (is => 'bare');
has strCommandCat => (is => 'bare', default => 'cat %file%');
has strCommandManifest => (is => 'bare');

# Lock path
has strLockPath => (is => 'bare');

# Files to hold stderr
#has strBackupStdErrFile => (is => 'bare');
#has strDbStdErrFile => (is => 'bare');

# Module variables
has strDbUser => (is => 'bare');                # Database user
has strDbHost => (is => 'bare');                # Database host
has oDbSSH => (is => 'bare');                   # Database SSH object

has strBackupUser => (is => 'bare');            # Backup user
has strBackupHost => (is => 'bare');            # Backup host
has oBackupSSH => (is => 'bare');               # Backup SSH object
has strBackupPath => (is => 'bare');            # Backup base path
has strBackupClusterPath => (is => 'bare');     # Backup cluster path

# Process flags
has bNoCompression => (is => 'bare');
has strStanza => (is => 'bare');
has iThreadIdx => (is => 'bare');

####################################################################################################################################
# COMMAND Error Constants
####################################################################################################################################
use constant
{
    COMMAND_ERR_FILE_MISSING           => 1,
    COMMAND_ERR_FILE_READ              => 2,
    COMMAND_ERR_FILE_MOVE              => 3,
    COMMAND_ERR_FILE_TYPE              => 4,
    COMMAND_ERR_LINK_READ              => 5,
    COMMAND_ERR_PATH_MISSING           => 6,
    COMMAND_ERR_PATH_CREATE            => 7
};

####################################################################################################################################
# PATH_GET Constants
####################################################################################################################################
use constant
{
    PATH_ABSOLUTE        => 'absolute',
    PATH_DB              => 'db',
    PATH_DB_ABSOLUTE     => 'db:absolute',
    PATH_BACKUP          => 'backup',
    PATH_BACKUP_ABSOLUTE => 'backup:absolute',
    PATH_BACKUP_CLUSTER  => 'backup:cluster',
    PATH_BACKUP_TMP      => 'backup:tmp',
    PATH_BACKUP_ARCHIVE  => 'backup:archive',
    PATH_LOCK_ERR        => 'lock:err'
};

####################################################################################################################################
# CONSTRUCTOR
####################################################################################################################################
sub BUILD
{
    my $self = shift;
    
    # Make sure the backup path is defined
    if (defined($self->{strBackupPath}))
    {
        # Create the backup cluster path
        $self->{strBackupClusterPath} = $self->{strBackupPath} . "/" . $self->{strStanza};
    }

    # Create the ssh options string
    if (defined($self->{strBackupHost}) || defined($self->{strDbHost}))
    {
        if (defined($self->{strBackupHost}) && defined($self->{strDbHost}))
        {
            confess &log(ASSERT, "backup and db hosts cannot both be remote");
        }
        
        my $strOptionSSHRequestTTY = "RequestTTY=yes";
        my $strOptionSSHCompression = "Compression=no";

        if ($self->{bNoCompression})
        {
            $strOptionSSHCompression = "Compression=yes";
        }

        # Connect SSH object if backup host is defined
        if (!defined($self->{oBackupSSH}) && defined($self->{strBackupHost}))
        {
            &log(TRACE, "connecting to backup ssh host " . $self->{strBackupHost});
            
            $self->{oBackupSSH} = Net::OpenSSH->new($self->{strBackupHost}, timeout => 300, user => $self->{strBackupUser},
#                                      default_stderr_file => $self->path_get(PATH_LOCK_ERR, "file"),
                                      master_opts => [-o => $strOptionSSHCompression, -o => $strOptionSSHRequestTTY]);
            $self->{oBackupSSH}->error and confess &log(ERROR, "unable to connect to $self->{strBackupHost}: " . $self->{oBackupSSH}->error);
        }

        # Connect SSH object if db host is defined
        if (!defined($self->{oDbSSH}) && defined($self->{strDbHost}))
        {
            &log(TRACE, "connecting to database ssh host $self->{strDbHost}");

            $self->{oDbSSH} = Net::OpenSSH->new($self->{strDbHost}, timeout => 300, user => $self->{strDbUser},
#                                  default_stderr_file => $self->path_get(PATH_LOCK_ERR, "file"),
                                  master_opts => [-o => $strOptionSSHCompression, -o => $strOptionSSHRequestTTY]);
            $self->{oDbSSH}->error and confess &log(ERROR, "unable to connect to $self->{strDbHost}: " . $self->{oDbSSH}->error);
        }
    }
}

####################################################################################################################################
# CLONE
####################################################################################################################################
sub clone
{
    my $self = shift;
    my $iThreadIdx = shift;

    return pg_backrest_file->new
    (
        strCompressExtension => $self->{strCompressExtension},
        strDefaultPathPermission => $self->{strDefaultPathPermission},
        strDefaultFilePermission => $self->{strDefaultFilePermission},
        strCommand => $self->{strCommand},
        strCommandChecksum => $self->{strCommandChecksum},
        strCommandCompress => $self->{strCommandCompress},
        strCommandDecompress => $self->{strCommandDecompress},
        strCommandCat => $self->{strCommandCat},
        strCommandManifest => $self->{strCommandManifest},
        strDbUser => $self->{strDbUser},
        strDbHost => $self->{strDbHost},
        strBackupUser => $self->{strBackupUser},
        strBackupHost => $self->{strBackupHost},
        strBackupPath => $self->{strBackupPath},
        strBackupClusterPath => $self->{strBackupClusterPath},
        bNoCompression => $self->{bNoCompression},
        strStanza => $self->{strStanza},
        iThreadIdx => $iThreadIdx,
        strLockPath => $self->{strLockPath}
    );
}

####################################################################################################################################
# ERROR_GET
####################################################################################################################################
sub error_get
{
    my $self = shift;
    
    my $strErrorFile = $self->path_get(PATH_LOCK_ERR, "file");
    
    open my $hFile, '<', $strErrorFile or return "error opening ${strErrorFile} to read STDERR output";
    
    my $strError = do {local $/; <$hFile>};
    close $hFile;
    
    return trim($strError);
}

####################################################################################################################################
# ERROR_CLEAR
####################################################################################################################################
sub error_clear
{
    my $self = shift;

    unlink($self->path_get(PATH_LOCK_ERR, "file"));
}

####################################################################################################################################
# PATH_TYPE_GET
####################################################################################################################################
sub path_type_get
{
    my $self = shift;
    my $strType = shift;

    # If db type
    if ($strType eq PATH_ABSOLUTE)
    {
        return PATH_ABSOLUTE;
    }
    elsif ($strType =~ /^db(\:.*){0,1}/)
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

####################################################################################################################################
# PATH_GET
####################################################################################################################################
sub path_get
{
    my $self = shift;
    my $strType = shift;    # Base type of the path to get (PATH_DB_ABSOLUTE, PATH_BACKUP_TMP, etc)
    my $strFile = shift;    # File to append to the base path (can include a path as well)
    my $bTemp = shift;      # Return the temp file for this path type - only some types have temp files

    # Make sure that any absolute path starts with /, otherwise it will actually be relative
    my $bAbsolute = $strType =~ /.*absolute.*/;

    if ($bAbsolute && $strFile !~ /^\/.*/)
    {
        confess &log(ASSERT, "absolute path ${strType}:${strFile} must start with /");
    }

    # Only allow temp files for PATH_BACKUP_ARCHIVE and PATH_BACKUP_TMP and any absolute path
    $bTemp = defined($bTemp) ? $bTemp : false;

    if ($bTemp && !($strType eq PATH_BACKUP_ARCHIVE || $strType eq PATH_BACKUP_TMP || $bAbsolute))
    {
        confess &log(ASSERT, "temp file not supported on path " . $strType);
    }

    # Get absolute path
    if ($bAbsolute)
    {
        if (defined($bTemp) && $bTemp)
        {
            return $strFile . ".backrest.tmp";
        }

        return $strFile;
    }

    # Make sure the base backup path is defined (since all other path types are backup)
    if (!defined($self->{strBackupPath}))
    {
        confess &log(ASSERT, "\$strBackupPath not yet defined");
    }

    # Get base backup path
    if ($strType eq PATH_BACKUP)
    {
        return $self->{strBackupPath} . (defined($strFile) ? "/${strFile}" : "");
    }

    # Make sure the cluster is defined
    if (!defined($self->{strStanza}))
    {
        confess &log(ASSERT, "\$strStanza not yet defined");
    }

    # Get the lock error path
    if ($strType eq PATH_LOCK_ERR)
    {
        my $strTempPath = $self->{strLockPath};
        
        if (!defined($strTempPath))
        {
            return undef;
        }

        return ${strTempPath} . (defined($strFile) ? "/${strFile}" .
                                (defined($self->{iThreadIdx}) ? ".$self->{iThreadIdx}" : "") . ".err" : "");
    }

    # Get the backup tmp path
    if ($strType eq PATH_BACKUP_TMP)
    {
        my $strTempPath = "$self->{strBackupPath}/temp/$self->{strStanza}.tmp";

        if ($bTemp)
        {
            return "${strTempPath}/file.tmp" . (defined($self->{iThreadIdx}) ? ".$self->{iThreadIdx}" : "");
        }

        return "${strTempPath}" . (defined($strFile) ? "/${strFile}" : "");
    }

    # Get the backup archive path
    if ($strType eq PATH_BACKUP_ARCHIVE)
    {
        my $strArchivePath = "$self->{strBackupPath}/archive/$self->{strStanza}";
        my $strArchive;

        if ($bTemp)
        {
            return "${strArchivePath}/file.tmp" . (defined($self->{iThreadIdx}) ? ".$self->{iThreadIdx}" : "");
        }

        if (defined($strFile))
        {
            $strArchive = substr(basename($strFile), 0, 24);

            if ($strArchive !~ /^([0-F]){24}$/)
            {
                return "${strArchivePath}/${strFile}";
            }
        }

        return $strArchivePath . (defined($strArchive) ? "/" . substr($strArchive, 0, 16) : "") .
               (defined($strFile) ? "/" . $strFile : "");
    }

    if ($strType eq PATH_BACKUP_CLUSTER)
    {
        return $self->{strBackupPath} . "/backup/$self->{strStanza}" . (defined($strFile) ? "/${strFile}" : "");
    }

    # Error when path type not recognized
    confess &log(ASSERT, "no known path types in '${strType}'");
}

####################################################################################################################################
# IS_REMOTE
#
# Determine whether any operations are being performed remotely.  If $strPathType is defined, the function will return true if that
# path is remote.  If $strPathType is not defined, then function will return true if any path is remote.
####################################################################################################################################
sub is_remote
{
    my $self = shift;
    my $strPathType = shift;

    # If the SSH object is defined then some paths are remote
    if (defined($self->{oDbSSH}) || defined($self->{oBackupSSH}))
    {
        # If path type is not defined but the SSH object is, then some paths are remote
        if (!defined($strPathType))
        {
            return true;
        }

        # If a host is defined for the path then it is remote
        if (defined($self->{strBackupHost}) && $self->path_type_get($strPathType) eq PATH_BACKUP ||
            defined($self->{strDbHost}) && $self->path_type_get($strPathType) eq PATH_DB)
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
    my $self = shift;
    my $strPathType = shift;
    
    # Get the db SSH object
    if ($self->path_type_get($strPathType) eq PATH_DB && defined($self->{oDbSSH}))
    {
        return $self->{oDbSSH};
    }

    # Get the backup SSH object
    if ($self->path_type_get($strPathType) eq PATH_BACKUP && defined($self->{oBackupSSH}))
    {
        return $self->{oBackupSSH}
    }

    # Error when no ssh object is found
    confess &log(ASSERT, "path type ${strPathType} does not have a defined ssh object");
}


####################################################################################################################################
# LINK_CREATE
####################################################################################################################################
sub link_create
{
    my $self = shift;
    my $strSourcePathType = shift;
    my $strSourceFile = shift;
    my $strDestinationPathType = shift;
    my $strDestinationFile = shift;
    my $bHard = shift;
    my $bRelative = shift;
    my $bPathCreate = shift;

    # if bHard is not defined default to false
    $bHard = defined($bHard) ? $bHard : false;

    # if bRelative is not defined or bHard is true, default to false
    $bRelative = !defined($bRelative) || $bHard ? false : $bRelative;

    # if bPathCreate is not defined, default to true
    $bPathCreate = defined($bPathCreate) ? $bPathCreate : true;
    
    # Source and destination path types must be the same (both PATH_DB or both PATH_BACKUP)
    if ($self->path_type_get($strSourcePathType) ne $self->path_type_get($strDestinationPathType))
    {
        confess &log(ASSERT, "path types must be equal in link create");
    }

    # Generate source and destination files
    my $strSource = $self->path_get($strSourcePathType, $strSourceFile);
    my $strDestination = $self->path_get($strDestinationPathType, $strDestinationFile);

    # If the destination path is backup and does not exist, create it
    if ($bPathCreate && $self->path_type_get($strDestinationPathType) eq PATH_BACKUP)
    {
        $self->path_create(PATH_BACKUP_ABSOLUTE, dirname($strDestination));
    }

    unless (-e $strSource)
    {
        if (-e $strSource . ".$self->{strCompressExtension}")
        {
            $strSource .= ".$self->{strCompressExtension}";
            $strDestination .= ".$self->{strCompressExtension}";
        }
        else
        {
            # Error when a hardlink will be created on a missing file
            if ($bHard)
            {
                confess &log(ASSERT, "unable to find ${strSource}(.$self->{strCompressExtension}) for link");
            }
        }
    }

    # Generate relative path if requested
    if ($bRelative)
    {
        my $iCommonLen = common_prefix($strSource, $strDestination);

        if ($iCommonLen != 0)
        {
            $strSource = ("../" x substr($strDestination, $iCommonLen) =~ tr/\///) . substr($strSource, $iCommonLen);
        }
    }

    # Create the command
    my $strCommand = "ln" . (!$bHard ? " -s" : "") . " ${strSource} ${strDestination}";

    # Run remotely
    if ($self->is_remote($strSourcePathType))
    {
        &log(TRACE, "link_create: remote ${strSourcePathType} '${strCommand}'");

        my $oSSH = $self->remote_get($strSourcePathType);
        $oSSH->system($strCommand) or confess &log("unable to create link from ${strSource} to ${strDestination}");
    }
    # Run locally
    else
    {
        &log(TRACE, "link_create: local '${strCommand}'");
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
    my $self = shift;
    my $strPathType = shift;
    my $strPath = shift;
    my $strPermission = shift;

    # Setup standard variables
    my $strErrorPrefix = "File->path_create";
    my $bRemote = $self->is_remote($strPathType);
    my $strPathOp = $self->path_get($strPathType, $strPath);

    &log(TRACE, "${strErrorPrefix}: " . ($bRemote ? "remote" : "local") . " ${strPathType}:${strPath}, " . 
                "permission " . (defined($strPermission) ? $strPermission : "[undef]"));

    if ($bRemote)
    {
        # Run remotely
        my $oSSH = $self->remote_get($strPathType);
        my $strOutput = $oSSH->capture($self->{strCommand} .
                        (defined($strPermission) ? " --permission=${strPermission}" : "") .
                        " path_create ${strPath}");

        # Capture any errors
        if ($oSSH->error)
        {
            confess &log(ERROR, "${strErrorPrefix} remote: " . (defined($strOutput) ? $strOutput : $oSSH->error));
        }
    }
    else
    {
        # Attempt the create the directory
        if (!mkdir($strPathOp, oct(defined($strPermission) ? $strPermission : $self->{strDefaultPathPermission})))
        {
            # Capture the error
            my $strError = "${strPath} could not be created: " . $!;

            # If running on command line the return directly
            if ($strPathType eq PATH_ABSOLUTE)
            {
                print $strError;
                exit COMMAND_ERR_PATH_CREATE;
            }

            # Error the normal way
            confess &log(ERROR, "${strErrorPrefix}: " . $strError);
        }
    }
}

####################################################################################################################################
# MOVE
#
# Moves a file locally or remotely.
####################################################################################################################################
sub move
{
    my $self = shift;
    my $strSourcePathType = shift;
    my $strSourceFile = shift;
    my $strDestinationPathType = shift;
    my $strDestinationFile = shift;
    my $bDestinationPathCreate = shift;

    # Get the root path for the file list
    my $strErrorPrefix = "File->move";
    my $bRemote = $self->is_remote($strSourcePathType);
    $bDestinationPathCreate = defined($bDestinationPathCreate) ? $bDestinationPathCreate : true;

    &log(TRACE, "${strErrorPrefix}: " . ($bRemote ? "remote" : "local") .
                " ${strSourcePathType}" . (defined($strSourceFile) ? ":${strSourceFile}" : "") .
                " to ${strDestinationPathType}" . (defined($strDestinationFile) ? ":${strDestinationFile}" : "") .
                ", dest_path_create = " . ($bDestinationPathCreate ? "true" : "false"));

    # Get source and desination files
    if ($self->path_type_get($strSourcePathType) ne $self->path_type_get($strSourcePathType))
    {
        confess &log(ASSERT, "source and destination path types must be equal");
    }

    my $strPathOpSource = $self->path_get($strSourcePathType, $strSourceFile);
    my $strPathOpDestination = $self->path_get($strDestinationPathType, $strDestinationFile);

    # Run remotely
    if ($bRemote)
    {
        my $strCommand = $self->{strCommand} .
                         ($bDestinationPathCreate ? " --dest-path-create" : "") .
                         " move ${strPathOpSource} ${strPathOpDestination}";

        # Run via SSH
        my $oSSH = $self->remote_get($strSourcePathType);
        my $strOutput = $oSSH->capture($strCommand);

        # Handle any errors
        if ($oSSH->error)
        {
            confess &log(ERROR, "${strErrorPrefix} remote (${strCommand}): " . (defined($strOutput) ? $strOutput : $oSSH->error));
        }
    }
    # Run locally
    else
    {
        # If the destination path does not exist, create it
        unless (-e dirname($strPathOpDestination))
        {
            if ($bDestinationPathCreate)
            {
                $self->path_create($strDestinationPathType, dirname($strDestinationFile));
            }
            else
            {
                my $strError = "destination " . dirname($strPathOpDestination) . " does not exist";

                if ($strSourcePathType eq PATH_ABSOLUTE)
                {
                    print $strError;
                    exit (COMMAND_ERR_PATH_MISSING);
                }

                confess &log(ERROR, "${strErrorPrefix}: " . $strError);
            }
        }
        
        if (!rename($strPathOpSource, $strPathOpDestination))
        {
            my $strError = "${strPathOpSource} could not be moved:" . $!;
            my $iErrorCode = COMMAND_ERR_FILE_MOVE;

            unless (-e $strPathOpSource)
            {
                $strError = "${strPathOpSource} does not exist";
                $iErrorCode = COMMAND_ERR_FILE_MISSING;
            }

            if ($strSourcePathType eq PATH_ABSOLUTE)
            {
                print $strError;
                exit ($iErrorCode);
            }

            confess &log(ERROR, "${strErrorPrefix}: " . $strError);
        }
    }
}

####################################################################################################################################
# FILE_COPY
####################################################################################################################################
sub file_copy
{
    my $self = shift;
    my $strSourcePathType = shift;
    my $strSourceFile = shift;
    my $strDestinationPathType = shift;
    my $strDestinationFile = shift;
    my $bNoCompressionOverride = shift;
    my $lModificationTime = shift;
    my $strPermission = shift;
    my $bPathCreate = shift;
    my $bConfessCopyError = shift;

    # if bPathCreate is not defined, default to true
    $bPathCreate = defined($bPathCreate) ? $bPathCreate : true;
    $bConfessCopyError = defined($bConfessCopyError) ? $bConfessCopyError : true;

    &log(TRACE, "file_copy: ${strSourcePathType}: " . (defined($strSourceFile) ? ":${strSourceFile}" : "") .
                " to ${strDestinationPathType}" . (defined($strDestinationFile) ? ":${strDestinationFile}" : ""));

    # Modification time and permissions cannot be set remotely
    if ((defined($lModificationTime) || defined($strPermission)) && $self->is_remote($strDestinationPathType))
    {
        confess &log(ASSERT, "modification time and permissions cannot be set on remote destination file");
    }

    # Generate source, destination and tmp filenames
    my $strSource = $self->path_get($strSourcePathType, $strSourceFile);
    my $strDestination = $self->path_get($strDestinationPathType, $strDestinationFile);
    my $strDestinationTmp = $self->path_get($strDestinationPathType, $strDestinationFile, true);

    # Is this already a compressed file?
    my $bAlreadyCompressed = $strSource =~ "^.*\.$self->{strCompressExtension}\$";

    if ($bAlreadyCompressed && $strDestination !~ "^.*\.$self->{strCompressExtension}\$")
    {
        $strDestination .= ".$self->{strCompressExtension}";
    }

    # Does the file need compression?
    my $bCompress = !((defined($bNoCompressionOverride) && $bNoCompressionOverride) ||
                      (!defined($bNoCompressionOverride) && $self->{bNoCompression}));

    # If the destination path is backup and does not exist, create it
    if ($bPathCreate && $self->path_type_get($strDestinationPathType) eq PATH_BACKUP)
    {
        $self->path_create(PATH_BACKUP_ABSOLUTE, dirname($strDestination));
    }

    # Generate the command string depending on compression/decompression/cat
    my $strCommand = $self->{strCommandCat};
    
    if (!$bAlreadyCompressed && $bCompress)
    {
        $strCommand = $self->{strCommandCompress};
        $strDestination .= ".gz";
    }
    elsif ($bAlreadyCompressed && !$bCompress)
    {
        $strCommand = $self->{strCommandDecompress};
        $strDestination = substr($strDestination, 0, length($strDestination) - length($self->{strCompressExtension}) - 1);
    }

    $strCommand =~ s/\%file\%/${strSource}/g;

    # If this command is remote on only one side
    if ($self->is_remote($strSourcePathType) && !$self->is_remote($strDestinationPathType) ||
        !$self->is_remote($strSourcePathType) && $self->is_remote($strDestinationPathType))
    {
        # Else if the source is remote
        if ($self->is_remote($strSourcePathType))
        {
            &log(TRACE, "file_copy: remote ${strSource} to local ${strDestination}");

            # Open the destination file for writing (will be streamed from the ssh session)
            my $hFile;
            open($hFile, ">", $strDestinationTmp) or confess &log(ERROR, "cannot open ${strDestination}");

            # Execute the command through ssh
            my $oSSH = $self->remote_get($strSourcePathType);
            
            unless ($oSSH->system({stderr_file => $self->path_get(PATH_LOCK_ERR, "file"), stdout_fh => $hFile}, $strCommand))
            {
                close($hFile) or confess &log(ERROR, "cannot close file ${strDestinationTmp}");
                
                my $strResult = "unable to execute ssh '${strCommand}': " . $self->error_get();
                $bConfessCopyError ? confess &log(ERROR, $strResult) : return false;
            }

            # Close the destination file handle
            close($hFile) or confess &log(ERROR, "cannot close file ${strDestinationTmp}");
        }
        # Else if the destination is remote
        elsif ($self->is_remote($strDestinationPathType))
        {
            &log(TRACE, "file_copy: local ${strSource} ($strCommand) to remote ${strDestination}");

            if (defined($self->path_get(PATH_LOCK_ERR, "file")))
            {
                $strCommand .= " 2> " . $self->path_get(PATH_LOCK_ERR, "file");
            }

            # Open the input command as a stream
            my $hOut;
            my $pId = open3(undef, $hOut, undef, $strCommand) or confess(ERROR, "unable to execute '${strCommand}'");

            # Execute the command though ssh
            my $oSSH = $self->remote_get($strDestinationPathType);
            $oSSH->system({stderr_file => $self->path_get(PATH_LOCK_ERR, "file"), stdin_fh => $hOut}, "cat > ${strDestinationTmp}") or confess &log(ERROR, "unable to execute ssh 'cat'");

            # Wait for the stream process to finish
            waitpid($pId, 0);
            my $iExitStatus = ${^CHILD_ERROR_NATIVE} >> 8;

            if ($iExitStatus != 0)
            {
                my $strResult = "command '${strCommand}' returned " . $iExitStatus . ": " . $self->error_get();
                $bConfessCopyError ? confess &log(ERROR, $strResult) : return false;
            }
        }
    }
    # If the source and destination are both remote but not the same remote
    elsif ($self->is_remote($strSourcePathType) && $self->is_remote($strDestinationPathType) &&
           $self->path_type_get($strSourcePathType) ne $self->path_type_get($strDestinationPathType))
    {
        &log(TRACE, "file_copy: remote ${strSource} to remote ${strDestination}");
        confess &log(ASSERT, "remote source and destination not supported");
    }
    # Else this is a local command or remote where both sides are the same remote
    else
    {
        # Complete the command by redirecting to the destination tmp file
        $strCommand .= " > ${strDestinationTmp}";

        if ($self->is_remote($strSourcePathType))
        {
            &log(TRACE, "file_copy: remote ${strSourcePathType} '${strCommand}'");

            my $oSSH = $self->remote_get($strSourcePathType);
            $oSSH->system({stderr_file => $self->path_get(PATH_LOCK_ERR, "file")}, $strCommand);

            if ($oSSH->error)
            {
                my $strResult = "unable to execute copy ${strCommand}: " . $oSSH->error;
                $bConfessCopyError ? confess &log(ERROR, $strResult) : return false;
            }
        }
        else
        {
            &log(TRACE, "file_copy: local '${strCommand}'");
            
            if (defined($self->path_get(PATH_LOCK_ERR, "file")))
            {
                $strCommand .= " 2> " . $self->path_get(PATH_LOCK_ERR, "file");
            }

            unless(system($strCommand) == 0)
            {
                my $strResult = "unable to copy local ${strSource} to local ${strDestinationTmp}";
                $bConfessCopyError ? confess &log(ERROR, $strResult) : return false;
            }
        }
    }

    # Set the file permission if required (this only works locally for now)
    if (defined($strPermission))
    {
        &log(TRACE, "file_copy: chmod ${strPermission}");

        system("chmod ${strPermission} ${strDestinationTmp}") == 0
            or confess &log(ERROR, "unable to set permissions for local ${strDestinationTmp}");
    }

    # Set the file modification time if required (this only works locally for now)
    if (defined($lModificationTime))
    {
        &log(TRACE, "file_copy: time ${lModificationTime}");

        utime($lModificationTime, $lModificationTime, $strDestinationTmp)
            or confess &log(ERROR, "unable to set time for local ${strDestinationTmp}");
    }

    # Move the file from tmp to final destination
    $self->file_move($self->path_type_get($strSourcePathType) . ":absolute", $strDestinationTmp,
                     $self->path_type_get($strDestinationPathType) . ":absolute",  $strDestination, $bPathCreate);
                     
    return true;
}

####################################################################################################################################
# HASH
####################################################################################################################################
sub hash
{
    my $self = shift;
    my $strPathType = shift;
    my $strFile = shift;
    my $strHashType = shift;

    # For now this operation is not supported remotely.  Not currently needed.
    my $strHash;
    my $strErrorPrefix = "File->hash";
    my $bRemote = $self->is_remote($strPathType);
    my $strPath = $self->path_get($strPathType, $strFile);

    &log(TRACE, "${strErrorPrefix}: " . ($bRemote ? "remote" : "local") . " ${strPathType}:${strPath}");

    if ($bRemote)
    {
        # Run remotely
        my $oSSH = $self->remote_get($strPathType);
        my $strOutput = $oSSH->capture($self->{strCommand} . " hash ${strPath}");

        # Capture any errors
        if ($oSSH->error)
        {
            confess &log(ERROR, "${strErrorPrefix} remote: " . (defined($strOutput) ? $strOutput : $oSSH->error));
        }

        $strHash = $strOutput;
    }
    else
    {
        my $hFile;

        if (!open($hFile, "<", $strPath))
        {
            my $strError = "${strPath} could not be read" . $!;
            my $iErrorCode = 2;

            unless (-e $strPath)
            {
                $strError = "${strPath} does not exist";
                $iErrorCode = 1;
            }

            if ($strPathType eq PATH_ABSOLUTE)
            {
                print $strError;
                exit ($iErrorCode);
            }

            confess &log(ERROR, "${strErrorPrefix}: " . $strError);
        }

        my $oSHA = Digest::SHA->new(defined($strHashType) ? $strHashType : 'sha1');

        $oSHA->addfile($hFile);

        close($hFile);

        $strHash = $oSHA->hexdigest();
    }

    return $strHash;
}

####################################################################################################################################
# COMPRESS
####################################################################################################################################
sub compress
{
    my $self = shift;
    my $strPathType = shift;
    my $strFile = shift;

    # Get the root path for the file list
    my $strErrorPrefix = "File->compress";
    my $bRemote = $self->is_remote($strPathType);
    my $strPathOp = $self->path_get($strPathType, $strFile);

    &log(TRACE, "${strErrorPrefix}: " . ($bRemote ? "remote" : "local") . " ${strPathType}:${strPathOp}");

    # Run remotely
    if ($bRemote)
    {
        my $strCommand = $self->{strCommand} .
                         " compress ${strPathOp}";

        # Run via SSH
        my $oSSH = $self->remote_get($strPathType);
        my $strOutput = $oSSH->capture($strCommand);

        # Handle any errors
        if ($oSSH->error)
        {
            confess &log(ERROR, "${strErrorPrefix} remote (${strCommand}): " . (defined($strOutput) ? $strOutput : $oSSH->error));
        }
    }
    # Run locally
    else
    {
        if (!gzip($strPathOp => "${strPathOp}.gz"))
        {
            my $strError = "${strPathOp} could not be compressed:" . $!;
            my $iErrorCode = 2;

            unless (-e $strPathOp)
            {
                $strError = "${strPathOp} does not exist";
                $iErrorCode = 1;
            }

            if ($strPathType eq PATH_ABSOLUTE)
            {
                print $strError;
                exit ($iErrorCode);
            }

            confess &log(ERROR, "${strErrorPrefix}: " . $strError);
        }
    }
}

####################################################################################################################################
# LIST
####################################################################################################################################
sub list
{
    my $self = shift;
    my $strPathType = shift;
    my $strPath = shift;
    my $strExpression = shift;
    my $strSortOrder = shift;

    # Get the root path for the file list
    my @stryFileList;
    my $strErrorPrefix = "File->list";
    my $bRemote = $self->is_remote($strPathType);
    my $strPathOp = $self->path_get($strPathType, $strPath);

    &log(TRACE, "${strErrorPrefix}: " . ($bRemote ? "remote" : "local") . " ${strPathType}:${strPathOp}" . 
                                        ", expression " . (defined($strExpression) ? $strExpression : "[UNDEF]") .
                                        ", sort " . (defined($strSortOrder) ? $strSortOrder : "[UNDEF]"));

    # Run remotely
    if ($self->is_remote($strPathType))
    {
        my $strCommand = $self->{strCommand} .
                         (defined($strSortOrder) && $strSortOrder eq "reverse" ? " --sort=reverse" : "") .
                         (defined($strExpression) ? " --expression=\"" . $strExpression . "\"" : "") .
                         " list ${strPathOp}";

        # Run via SSH
        my $oSSH = $self->remote_get($strPathType);
        my $strOutput = $oSSH->capture($strCommand);

        # Handle any errors
        if ($oSSH->error)
        {
            confess &log(ERROR, "${strErrorPrefix} remote (${strCommand}): " . (defined($strOutput) ? $strOutput : $oSSH->error));
        }

        @stryFileList = split(/\n/, $strOutput);
    }
    # Run locally
    else
    {
        my $hPath;

        if (!opendir($hPath, $strPathOp))
        {
            my $strError = "${strPathOp} could not be read:" . $!;
            my $iErrorCode = 2;

            unless (-e $strPath)
            {
                $strError = "${strPathOp} does not exist";
                $iErrorCode = 1;
            }

            if ($strPathType eq PATH_ABSOLUTE)
            {
                print $strError;
                exit ($iErrorCode);
            }

            confess &log(ERROR, "${strErrorPrefix}: " . $strError);
        }

        @stryFileList = grep(!/^(\.)|(\.\.)$/i, readdir($hPath));

        close($hPath);

        if (defined($strExpression))
        {
            @stryFileList = grep(/$strExpression/i, @stryFileList);
        }

        # Reverse sort
        if (defined($strSortOrder) && $strSortOrder eq "reverse")
        {
            @stryFileList = sort {$b cmp $a} @stryFileList;
        }
        # Normal sort
        else
        {
            @stryFileList = sort @stryFileList;
        }
    }

    # Return file list
    return @stryFileList;
}

####################################################################################################################################
# EXISTS - Checks for the existence of a file, but does not imply that the file is readable/writeable.
#
# Return: true if file exists, false otherwise
####################################################################################################################################
sub exists
{
    my $self = shift;
    my $strPathType = shift;
    my $strPath = shift;

    # Set error prefix, remote, and path
    my $bExists = false;
    my $strErrorPrefix = "File->exists";
    my $bRemote = $self->is_remote($strPathType);
    my $strPathOp = $self->path_get($strPathType, $strPath);

    &log(TRACE, "${strErrorPrefix}: " . ($bRemote ? "remote" : "local") . " ${strPathType}:${strPathOp}");

    # Run remotely
    if ($bRemote)
    {
        # Build the command
        my $strCommand = $self->{strCommand} . " exists ${strPathOp}";
        
        # Run it remotely
        my $oSSH = $self->remote_get($strPathType);
        my $strOutput = $oSSH->capture($strCommand);

        # Capture any errors
        if ($oSSH->error)
        {
            confess &log(ERROR, "${strErrorPrefix} remote (${strCommand}): " . (defined($strOutput) ? $strOutput : $oSSH->error));
        }

        $bExists = $strOutput eq "Y";
    }
    # Run locally
    else
    {
        if (-e $strPathOp)
        {
            $bExists = true;
        }
    }

    return $bExists;
}

####################################################################################################################################
# REMOVE
####################################################################################################################################
sub remove
{
    my $self = shift;
    my $strPathType = shift;
    my $strPath = shift;
    my $bTemp = shift;
    my $bIgnoreMissing = shift;
    
    if (!defined($bIgnoreMissing))
    {
        $bIgnoreMissing = true;
    }

    # Get the root path for the manifest
    my $bRemoved = true;
    my $strErrorPrefix = "File->remove";
    my $bRemote = $self->is_remote($strPathType);
    my $strPathOp = $self->path_get($strPathType, $strPath, $bTemp);

    &log(TRACE, "${strErrorPrefix}: " . ($bRemote ? "remote" : "local") . " ${strPathType}:${strPathOp}");

    # Run remotely
    if ($bRemote)
    {
        # Build the command
        my $strCommand = $self->{strCommand} . ($bIgnoreMissing ? " --ignore-missing" : "") . " remove ${strPathOp}";
        
        # Run it remotely
        my $oSSH = $self->remote_get($strPathType);
        my $strOutput = $oSSH->capture($strCommand);

        if ($oSSH->error)
        {
            confess &log(ERROR, "${strErrorPrefix} remote (${strCommand}): " . (defined($strOutput) ? $strOutput : $oSSH->error));
        }

        $bRemoved = $strOutput eq "Y";
    }
    # Run locally
    else
    {
        if (unlink($strPathOp) != 1)
        {
            $bRemoved = false;

            if (-e $strPathOp || !$bIgnoreMissing)
            {
                my $strError = "${strPathOp} could not be removed: " . $!;
                my $iErrorCode = 2;

                unless (-e $strPathOp)
                {
                    $strError = "${strPathOp} does not exist";
                    $iErrorCode = 1;
                }

                if ($strPathType eq PATH_ABSOLUTE)
                {
                    print $strError;
                    exit ($iErrorCode);
                }

                confess &log(ERROR, "${strErrorPrefix}: " . $strError);
            }
        }
    }
    
    return $bRemoved;
}

####################################################################################################################################
# MANIFEST
#
# Builds a path/file manifest starting with the base path and including all subpaths.  The manifest contains all the information
# needed to perform a backup or a delta with a previous backup.
####################################################################################################################################
sub manifest
{
    my $self = shift;
    my $strPathType = shift;
    my $strPath = shift;
    my $oManifestHashRef = shift;

    # Get the root path for the manifest
    my $strErrorPrefix = "File->manifest";
    my $bRemote = $self->is_remote($strPathType);
    my $strPathOp = $self->path_get($strPathType, $strPath);

    &log(TRACE, "${strErrorPrefix}: " . ($bRemote ? "remote" : "local") . " ${strPathType}:${strPathOp}");

    # Run remotely
    if ($bRemote)
    {
        # Build the command
        my $strCommand = $self->{strCommand} . " manifest ${strPathOp}";
        
        # Run it remotely
        my $oSSH = $self->remote_get($strPathType);
        my $strOutput = $oSSH->capture($strCommand);

        if ($oSSH->error)
        {
            confess &log(ERROR, "${strErrorPrefix} remote (${strCommand}): " . (defined($strOutput) ? $strOutput : $oSSH->error));
        }
        
        return data_hash_build($oManifestHashRef, $strOutput, "\t", ".");
    }
    # Run locally
    else
    {
        manifest_recurse($strPathType, $strPathOp, undef, 0, $oManifestHashRef);
    }
}

sub manifest_recurse
{
    my $strPathType = shift;
    my $strPathOp = shift;
    my $strPathFileOp = shift;
    my $iDepth = shift;
    my $oManifestHashRef = shift;

    my $strErrorPrefix = "File->manifest";
    my $strPathRead = $strPathOp . (defined($strPathFileOp) ? "/${strPathFileOp}" : "");
    my $hPath;

    if (!opendir($hPath, $strPathRead))
    {
        my $strError = "${strPathRead} could not be read:" . $!;
        my $iErrorCode = 2;

        unless (-e $strPathRead)
        {
            $strError = "${strPathRead} does not exist";
            $iErrorCode = 1;
        }

        if ($strPathType eq PATH_ABSOLUTE)
        {
            print $strError;
            exit ($iErrorCode);
        }

        confess &log(ERROR, "${strErrorPrefix}: " . $strError);
    }

    my @stryFileList = grep(!/^\..$/i, readdir($hPath));

    close($hPath);

    foreach my $strFile (@stryFileList)
    {
        my $strPathFile = "${strPathRead}/$strFile";
        my $bCurrentDir = $strFile eq ".";

        if ($iDepth != 0)
        {
            if ($bCurrentDir)
            {
                $strFile = $strPathFileOp;
                $strPathFile = $strPathRead;
            }
            else
            {
                $strFile = "${strPathFileOp}/${strFile}";
            }
        }

        my $oStat = lstat($strPathFile);

        if (!defined($oStat))
        {
            if (-e $strPathFile)
            {
                my $strError = "${strPathFile} could not be read: " . $!;

                if ($strPathType eq PATH_ABSOLUTE)
                {
                    print $strError;
                    exit COMMAND_ERR_FILE_READ;
                }

                confess &log(ERROR, "${strErrorPrefix}: " . $strError);
            }
            
            next;
        }

        # Check for regular file
        if (S_ISREG($oStat->mode))
        {
            ${$oManifestHashRef}{name}{"${strFile}"}{type} = "f";

            # Get inode
            ${$oManifestHashRef}{name}{"${strFile}"}{inode} = $oStat->ino;

            # Get size
            ${$oManifestHashRef}{name}{"${strFile}"}{size} = $oStat->size;

            # Get modification time
            ${$oManifestHashRef}{name}{"${strFile}"}{modification_time} = $oStat->mtime;
        }
        # Check for directory
        elsif (S_ISDIR($oStat->mode))
        {
            ${$oManifestHashRef}{name}{"${strFile}"}{type} = "d";
        }
        # Check for link
        elsif (S_ISLNK($oStat->mode))
        {
            ${$oManifestHashRef}{name}{"${strFile}"}{type} = "l";

            # Get link destination
            ${$oManifestHashRef}{name}{"${strFile}"}{link_destination} = readlink($strPathFile);
            
            if (!defined(${$oManifestHashRef}{name}{"${strFile}"}{link_destination}))
            {
                if (-e $strPathFile)
                {
                    my $strError = "${strPathFile} error reading link: " . $!;

                    if ($strPathType eq PATH_ABSOLUTE)
                    {
                        print $strError;
                        exit COMMAND_ERR_LINK_READ;
                    }

                    confess &log(ERROR, "${strErrorPrefix}: " . $strError);
                }
            }
        }
        else
        {
            my $strError = "${strPathFile} is not of type directory, file, or link";

            if ($strPathType eq PATH_ABSOLUTE)
            {
                print $strError;
                exit COMMAND_ERR_FILE_TYPE;
            }

            confess &log(ERROR, "${strErrorPrefix}: " . $strError);
        }

        # Get user name
        ${$oManifestHashRef}{name}{"${strFile}"}{user} = getpwuid($oStat->uid);

        # Get group name
        ${$oManifestHashRef}{name}{"${strFile}"}{group} = getgrgid($oStat->gid);

        # Get permissions
        if (${$oManifestHashRef}{name}{"${strFile}"}{type} ne "l")
        {
            ${$oManifestHashRef}{name}{"${strFile}"}{permission} = sprintf("%04o", S_IMODE($oStat->mode));
        }

        # Recurse into directories
        if (${$oManifestHashRef}{name}{"${strFile}"}{type} eq "d" && !$bCurrentDir)
        {
            manifest_recurse($strPathType, $strPathOp,
                             $strFile, 
                             $iDepth + 1, $oManifestHashRef);
        }
    }
}

no Moose;
__PACKAGE__->meta->make_immutable;
