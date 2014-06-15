####################################################################################################################################
# FILE MODULE
####################################################################################################################################
package BackRest::File;

use threads;
use strict;
use warnings;
use Carp;

use Moose;
use Net::OpenSSH;
use IPC::Open3;
use File::Basename;
use Digest::SHA;
use File::stat;
use Fcntl ':mode';
use IO::Compress::Gzip qw(gzip $GzipError);
use IO::Uncompress::Gunzip qw(gunzip $GunzipError);
use IO::String;

use lib dirname($0) . "/../lib";
use BackRest::Exception;
use BackRest::Utility;
use BackRest::Remote;

use Exporter qw(import);
our @EXPORT = qw(PATH_ABSOLUTE PATH_DB PATH_DB_ABSOLUTE PATH_BACKUP PATH_BACKUP_ABSOLUTE
                 PATH_BACKUP_CLUSTERPATH_BACKUP_TMP PATH_BACKUP_ARCHIVE

                 COMMAND_ERR_FILE_MISSING COMMAND_ERR_FILE_READ COMMAND_ERR_FILE_MOVE COMMAND_ERR_FILE_TYPE
                 COMMAND_ERR_LINK_READ COMMAND_ERR_PATH_MISSING COMMAND_ERR_PATH_CREATE COMMAND_ERR_PARAM

                 PIPE_STDIN PIPE_STDOUT PIPE_STDERR

                 OP_FILE_LIST OP_FILE_EXISTS OP_FILE_HASH OP_FILE_REMOVE OP_FILE_MANIFEST OP_FILE_COMPRESS
                 OP_FILE_MOVE OP_FILE_COPY_OUT OP_FILE_COPY_IN OP_FILE_PATH_CREATE);


# Extension and permissions
has strCompressExtension => (is => 'ro', default => 'gz');
has strDefaultPathPermission => (is => 'bare', default => '0750');
has strDefaultFilePermission => (is => 'ro', default => '0640');

# Command strings
has strCommand => (is => 'bare');

# Module variables
has strRemote => (is => 'bare');                # Remote type (db or backup)
has oRemote   => (is => 'bare');                # Remote object

has strBackupPath => (is => 'bare');            # Backup base path
has strBackupClusterPath => (is => 'bare');     # Backup cluster path

# Process flags
has bCompress => (is => 'bare');
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
    COMMAND_ERR_PATH_CREATE            => 7,
    COMMAND_ERR_PARAM                  => 8
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
    PATH_BACKUP_ARCHIVE  => 'backup:archive'
};

####################################################################################################################################
# File copy block size constant
####################################################################################################################################
use constant
{
    BLOCK_SIZE  => 8192
};

####################################################################################################################################
# STD Pipe Constants
####################################################################################################################################
use constant
{
    PIPE_STDIN   => "<STDIN>",
    PIPE_STDOUT  => "<STDOUT>",
    PIPE_STDERR  => "<STDERR>"
};

####################################################################################################################################
# Remote Types
####################################################################################################################################
use constant
{
    REMOTE_DB     => PATH_DB,
    REMOTE_BACKUP => PATH_BACKUP
};

####################################################################################################################################
# Operation constants
####################################################################################################################################
use constant
{
    OP_FILE_LIST        => "File->list",
    OP_FILE_EXISTS      => "File->exists",
    OP_FILE_HASH        => "File->hash",
    OP_FILE_REMOVE      => "File->remove",
    OP_FILE_MANIFEST    => "File->manifest",
    OP_FILE_COMPRESS    => "File->compress",
    OP_FILE_MOVE        => "File->move",
    OP_FILE_COPY_OUT    => "File->copy_out",
    OP_FILE_COPY_IN     => "File->copy_in",
    OP_FILE_PATH_CREATE => "File->path_create"
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

    # If remote is defined check parameters and open session
    if (defined($self->{strRemote}))
    {
        # Make sure remote is valid
        if ($self->{strRemote} ne REMOTE_DB && $self->{strRemote} ne REMOTE_BACKUP)
        {
            confess &log(ASSERT, "strRemote must be \"" . REMOTE_DB . "\" or \"" . REMOTE_BACKUP . "\"");
        }

        # Remote object must be set
        if (!defined($self->{oRemote}))
        {
            confess &log(ASSERT, "oRemote must be defined");
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
        strDbUser => $self->{strDbUser},
        strDbHost => $self->{strDbHost},
        strBackupUser => $self->{strBackupUser},
        strBackupHost => $self->{strBackupHost},
        strBackupPath => $self->{strBackupPath},
        strBackupClusterPath => $self->{strBackupClusterPath},
        bCompress => $self->{bCompress},
        strStanza => $self->{strStanza},
        iThreadIdx => $iThreadIdx
    );
}

####################################################################################################################################
# PATH_TYPE_GET
####################################################################################################################################
sub path_type_get
{
    my $self = shift;
    my $strType = shift;

    # If absolute type
    if ($strType eq PATH_ABSOLUTE)
    {
        return PATH_ABSOLUTE;
    }
    # If db type
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
# Determine whether the path type is remote
####################################################################################################################################
sub is_remote
{
    my $self = shift;
    my $strPathType = shift;

    return defined($self->{strRemote}) && $self->path_type_get($strPathType) eq $self->{strRemote};
}

####################################################################################################################################
# REMOTE_GET
#
# Get remote SSH object depending on the path type.
####################################################################################################################################
# sub remote_get
# {
#     my $self = shift;
#
#     # Get the db SSH object
#     if ($self->path_type_get($strPathType) eq PATH_DB && defined($self->{oDbSSH}))
#     {
#         return $self->{oDbSSH};
#     }
#
#     # Get the backup SSH object
#     if ($self->path_type_get($strPathType) eq PATH_BACKUP && defined($self->{oBackupSSH}))
#     {
#         return $self->{oBackupSSH}
#     }
#
#     # Error when no ssh object is found
#     confess &log(ASSERT, "path type ${strPathType} does not have a defined ssh object");
# }

####################################################################################################################################
# LINK_CREATE !!! NEEDS TO BE CONVERTED
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
# PIPE_TO_STRING Function
#
# Copies data from a file handle into a string.
####################################################################################################################################
# sub pipe_to_string
# {
#     my $self = shift;
#     my $hOut = shift;
#
#     my $strBuffer;
#     my $hString = IO::String->new($strBuffer);
#     $self->pipe($hOut, $hString);
#
#     return $strBuffer;
# }

####################################################################################################################################
# PIPE Function
#
# Copies data from one file handle to another, optionally compressing or decompressing the data in stream.
####################################################################################################################################
# sub pipe
# {
#     my $self = shift;
#     my $hIn = shift;
#     my $hOut = shift;
#     my $bCompress = shift;
#     my $bUncompress = shift;
#
#     # If compression is requested and the file is not already compressed
#     if (defined($bCompress) && $bCompress)
#     {
#         if (!gzip($hIn => $hOut))
#         {
#             confess $GzipError;
#         }
#     }
#     # If no compression is requested and the file is already compressed
#     elsif (defined($bUncompress) && $bUncompress)
#     {
#         if (!gunzip($hIn => $hOut))
#         {
#             confess $GunzipError;
#         }
#     }
#     # Else it's a straight copy
#     else
#     {
#         my $strBuffer;
#         my $iResultRead;
#         my $iResultWrite;
#
#         # Read from the input handle
#         while (($iResultRead = sysread($hIn, $strBuffer, BLOCK_SIZE)) != 0)
#         {
#             if (!defined($iResultRead))
#             {
#                 confess $!;
#                 last;
#             }
#             else
#             {
#                 # Write to the output handle
#                 $iResultWrite = syswrite($hOut, $strBuffer, $iResultRead);
#
#                 if (!defined($iResultWrite) || $iResultWrite != $iResultRead)
#                 {
#                     confess $!;
#                     last;
#                 }
#             }
#         }
#     }
# }

####################################################################################################################################
# WAIT_PID
####################################################################################################################################
# sub wait_pid
# {
#     my $self = shift;
#     my $pId = shift;
#     my $strCommand = shift;
#     my $hIn = shift;
#     my $hOut = shift;
#     my $hErr = shift;
#
#     # Close hIn
#     close($hIn);
#
#     # Read STDERR into a string
#     my $strError = defined($hErr) ? $self->pipe_to_string($hErr) : "[unknown]";
#
#     # Wait for the process to finish and report any errors
#     waitpid($pId, 0);
#     my $iExitStatus = ${^CHILD_ERROR_NATIVE} >> 8;
#
#     if ($iExitStatus != 0)
#     {
#         confess &log(ERROR, "command '${strCommand}' returned " . $iExitStatus . ": " .
#                             (defined($strError) ? $strError : "[unknown]"));
#     }
# }

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

    # Set operation variables
    my $strPathOp = $self->path_get($strPathType, $strPath);

    # Set operation and debug strings
    my $strOperation = OP_FILE_EXISTS;
    my $strDebug = "${strPathType}:${strPathOp}";

    # Run remotely
    if ($self->is_remote($strPathType))
    {
        # Build param hash
        my %oParamHash;

        $oParamHash{path} = ${strPathOp};

        # Build debug string
        $strDebug = "${strOperation}: remote (" . $self->{oRemote}->command_param_string(\%oParamHash) . "): " . $strDebug;
        &log(DEBUG, $strDebug);

        # Execute the command
        return $self->{oRemote}->command_execute($strOperation, \%oParamHash, true, $strDebug) eq "Y";
    }
    # Run locally
    else
    {
        # Build debug string
        $strDebug = "${strOperation}: local: " . $strDebug;
        &log(DEBUG, ${strDebug});

        # Stat the file/path to determine if it exists
        my $oStat = lstat($strPathOp);

        # Evaluate error
        if (!defined($oStat))
        {
            # If the error is not entry missing, then throw error
            if (!$!{ENOENT})
            {
                if ($strPathType eq PATH_ABSOLUTE)
                {
                    confess &log(ERROR, $!, COMMAND_ERR_FILE_READ);
                }
                else
                {
                    confess &log(ERROR, "${strDebug}: " . $!, COMMAND_ERR_FILE_READ);
                }
            }

            return false;
        }
    }

    return true;
}

####################################################################################################################################
# COPY
#
# Copies a file from one location to another:
#
# * source and destination can be local or remote
# * wire and output compression/decompression are supported
# * intermediate temp files are used to prevent partial copies
# * modification time and permissions can be set on destination file
# * destination path can optionally be created
####################################################################################################################################
sub copy
{
    my $self = shift;
    my $strSourcePathType = shift;
    my $strSourceFile = shift;
    my $strDestinationPathType = shift;
    my $strDestinationFile = shift;
    my $bIgnoreMissingSource = shift;
    my $bCompress = shift;
    my $bPathCreate = shift;
    my $lModificationTime = shift;
    my $strPermission = shift;

    # Set defaults
    $bCompress = defined($bCompress) ? $bCompress : defined($self->{bCompress}) ? $self->{bCompress} : false;
    $bIgnoreMissingSource = defined($bIgnoreMissingSource) ? $bIgnoreMissingSource : false;
    $bPathCreate = defined($bPathCreate) ? $bPathCreate : false;

    # Set working variables
    my $bSourceRemote = $self->is_remote($strSourcePathType) || $strSourcePathType eq PIPE_STDIN;
    my $bDestinationRemote = $self->is_remote($strDestinationPathType) || $strDestinationPathType eq PIPE_STDOUT;
    my $strSourceOp = $strSourcePathType eq PIPE_STDIN ?
        $strSourcePathType : $self->path_get($strSourcePathType, $strSourceFile);
    my $strDestinationOp = $strDestinationPathType eq PIPE_STDOUT ?
        $strDestinationPathType : $self->path_get($strDestinationPathType, $strDestinationFile);
    my $strDestinationTmpOp = $strDestinationPathType eq PIPE_STDOUT ?
        undef : $self->path_get($strDestinationPathType, $strDestinationFile, true);

    # Set operation and debug string
    my $strOperation = "File->unknown";
    my $strDebug = ($bSourceRemote ? " remote" : " local") . " ${strSourcePathType}" .
                   (defined($strSourceFile) ? ":${strSourceFile}" : "") .
                   " to" . ($bDestinationRemote ? " remote" : " local") . " ${strDestinationPathType}" .
                   (defined($strDestinationFile) ? ":${strDestinationFile}" : "") .
                   ", compress = " . ($bCompress ? "true" : "false");

    # Open the source file
    my $hSourceFile;

    if (!$bSourceRemote)
    {
        # Determine if the file is compressed
#        $bIsCompressed = $strSourceOp !~ "^.*\.$self->{strCompressExtension}\$";

        open($hSourceFile, "<", $strSourceOp)
            or confess &log(ERROR, "cannot open ${strSourceOp}: " . $!);
    }

    # Open the destination file
    my $hDestinationFile;

    # Determine if the file needs compression extension
    if (!$bDestinationRemote)
    {
        if ($bCompress)
        {
            $strDestinationOp .= "." . $self->{strCompressExtension};
        }

        open($hDestinationFile, ">", $strDestinationTmpOp)
            or confess &log(ERROR, "cannot open ${strDestinationTmpOp}: " . $!);
    }

    # If source or destination are remote
    if ($bSourceRemote || $bDestinationRemote)
    {
        # Build the command and open the local file
        my $hFile;
        my %oParamHash;
        my $hIn,
        my $hOut;
        my $strRemote;

        # If source is remote and destination is local
        if ($bSourceRemote && !$bDestinationRemote)
        {
            $hOut = $hDestinationFile;
            $strOperation = OP_FILE_COPY_OUT;

            if ($strSourcePathType eq PIPE_STDIN)
            {
                $strRemote = 'in';
                $hIn = *STDIN;
            }
            else
            {
                $strRemote = 'in';
                $oParamHash{source_file} = ${strSourceOp};
                $oParamHash{compress} = $bCompress;

                $hIn = $self->{oRemote}->{hOut};
            }
        }
        # Else if source is local and destination is remote
        elsif (!$bSourceRemote && $bDestinationRemote)
        {
            $hIn = $hSourceFile;
            $strOperation = OP_FILE_COPY_IN;

            if ($strDestinationPathType eq PIPE_STDOUT)
            {
                $strRemote = 'out';
                $hOut = *STDOUT;
            }
            else
            {
                $strRemote = 'out';
                $oParamHash{destination_file} = ${strDestinationOp};
                $oParamHash{compress} = $bCompress;

                $hOut = $self->{oRemote}->{hIn};
            }
        }
        # Else source and destination are remote
        else
        {
            if ($self->path_type_get($strSourcePathType) ne $self->path_type_get($strDestinationPathType))
            {
                confess &log(ASSERT, "remote source and destination not supported");
            }

            # !!! MULTIPLE REMOTE COPY NOT YET IMPLEMENTED
            return false;
        }

        # Build debug string
        $strDebug = "${strOperation}: " . (%oParamHash ? "remote (" .
                    $self->{oRemote}->command_param_string(\%oParamHash) . ") :" : "") . $strDebug;
        &log(DEBUG, $strDebug);

        # If an operation is defined then write it
        if (%oParamHash)
        {
            $self->{oRemote}->command_write($strOperation, \%oParamHash);
        }

        # Transfer the file
        $self->{oRemote}->binary_xfer($hIn, $hOut, $strRemote, $bCompress);

        # If this is the controlling process then wait for OK from remote
        if (%oParamHash)
        {
            $self->{oRemote}->output_read(false, $strDebug);
        }
    }
    else
    {
        # !!! Implement this with pipes from above (refactor copy_in and and copy_out)
        # !!! LOCAL COPY NOT YET IMPLEMENTED
        return false;
    }

    # Close the source file
    if (defined($hSourceFile))
    {
        close($hSourceFile) or confess &log(ERROR, "cannot close file ${strSourceOp}");
    }

    # Close the destination file
    if (defined($hDestinationFile))
    {
        close($hDestinationFile) or confess &log(ERROR, "cannot close file ${strDestinationTmpOp}");
    }

    if (!$bDestinationRemote)
    {
        # Set the file permission if required
        if (defined($strPermission))
        {
            system("chmod ${strPermission} ${strDestinationTmpOp}") == 0
                or confess &log(ERROR, "unable to set permissions for local ${strDestinationTmpOp}");
        }

        # Set the file modification time if required (this only works locally for now)
        if (defined($lModificationTime))
        {
            utime($lModificationTime, $lModificationTime, $strDestinationTmpOp)
                or confess &log(ERROR, "unable to set time for local ${strDestinationTmpOp}");
        }

        # Move the file from tmp to final destination
        $self->move(PATH_ABSOLUTE, $strDestinationTmpOp, PATH_ABSOLUTE, $strDestinationOp, $bPathCreate);
    }

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
