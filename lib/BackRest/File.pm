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
use File::Basename;
use File::Copy qw(cp);
use File::Path qw(make_path remove_tree);
use Digest::SHA;
use File::stat;
use Fcntl ':mode';
use IO::Compress::Gzip qw(gzip $GzipError);
use IO::Uncompress::Gunzip qw(gunzip $GunzipError);
use IO::String;

use lib dirname($0) . '/../lib';
use BackRest::Exception;
use BackRest::Utility;
use BackRest::Remote;
use BackRest::ProcessAsync;

# Exports
use Exporter qw(import);
our @EXPORT = qw(PATH_ABSOLUTE PATH_DB PATH_DB_ABSOLUTE PATH_BACKUP PATH_BACKUP_ABSOLUTE
                 PATH_BACKUP_CLUSTER PATH_BACKUP_TMP PATH_BACKUP_ARCHIVE

                 COMMAND_ERR_FILE_MISSING COMMAND_ERR_FILE_READ COMMAND_ERR_FILE_MOVE COMMAND_ERR_FILE_TYPE
                 COMMAND_ERR_LINK_READ COMMAND_ERR_PATH_MISSING COMMAND_ERR_PATH_CREATE COMMAND_ERR_PARAM

                 PIPE_STDIN PIPE_STDOUT PIPE_STDERR

                 REMOTE_DB REMOTE_BACKUP REMOTE_NONE

                 OP_FILE_LIST OP_FILE_EXISTS OP_FILE_HASH OP_FILE_REMOVE OP_FILE_MANIFEST OP_FILE_COMPRESS
                 OP_FILE_MOVE OP_FILE_COPY OP_FILE_COPY_OUT OP_FILE_COPY_IN OP_FILE_PATH_CREATE);

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

# Process flags
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
    COMMAND_ERR_PATH_READ              => 8
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
# STD Pipe Constants
####################################################################################################################################
use constant
{
    PIPE_STDIN   => '<STDIN>',
    PIPE_STDOUT  => '<STDOUT>',
    PIPE_STDERR  => '<STDERR>'
};

####################################################################################################################################
# Remote Types
####################################################################################################################################
use constant
{
    REMOTE_DB     => PATH_DB,
    REMOTE_BACKUP => PATH_BACKUP,
    REMOTE_NONE   => 'none'
};

####################################################################################################################################
# Operation constants
####################################################################################################################################
use constant
{
    OP_FILE_LIST        => 'File->list',
    OP_FILE_EXISTS      => 'File->exists',
    OP_FILE_HASH        => 'File->hash',
    OP_FILE_REMOVE      => 'File->remove',
    OP_FILE_MANIFEST    => 'File->manifest',
    OP_FILE_COMPRESS    => 'File->compress',
    OP_FILE_MOVE        => 'File->move',
    OP_FILE_COPY        => 'File->copy',
    OP_FILE_COPY_OUT    => 'File->copy_out',
    OP_FILE_COPY_IN     => 'File->copy_in',
    OP_FILE_PATH_CREATE => 'File->path_create'
};

####################################################################################################################################
# CONSTRUCTOR
####################################################################################################################################
sub BUILD
{
    my $self = shift;

    # If remote is defined check parameters and open session
    if (defined($self->{strRemote}) && $self->{strRemote} ne REMOTE_NONE)
    {
        # Make sure remote is valid
        if ($self->{strRemote} ne REMOTE_DB && $self->{strRemote} ne REMOTE_BACKUP)
        {
            confess &log(ASSERT, 'strRemote must be "' . REMOTE_DB . '" or "' . REMOTE_BACKUP . '"');
        }

        # Remote object must be set
        if (!defined($self->{oRemote}))
        {
            confess &log(ASSERT, 'oRemote must be defined');
        }
    }
}

####################################################################################################################################
# DESTRUCTOR
####################################################################################################################################
sub DEMOLISH
{
    my $self = shift;

    if (defined($self->{oRemote}))
    {
        $self->{oRemote} = undef;
    }

    if (defined($self->{oProcessAsync}))
    {
        $self->{oProcessAsync} = undef;
    }
}

####################################################################################################################################
# CLONE
####################################################################################################################################
sub clone
{
    my $self = shift;
    my $iThreadIdx = shift;

    return BackRest::File->new
    (
        strCommand => $self->{strCommand},
        strRemote => $self->{strRemote},
        oRemote => defined($self->{oRemote}) ? $self->{oRemote}->clone($iThreadIdx) : undef,
        strBackupPath => $self->{strBackupPath},
        strStanza => $self->{strStanza},
        iThreadIdx => $iThreadIdx
    );
}

####################################################################################################################################
# PROCESS_ASYNC_GET
####################################################################################################################################
sub process_async_get
{
    my $self = shift;

    if (!defined($self->{oProcessAsync}))
    {
        $self->{oProcessAsync} = new BackRest::ProcessAsync;
    }

    return $self->{oProcessAsync};
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
        confess &log(ASSERT, 'temp file not supported on path ' . $strType);
    }

    # Get absolute path
    if ($bAbsolute)
    {
        if (defined($bTemp) && $bTemp)
        {
            return $strFile . '.backrest.tmp';
        }

        return $strFile;
    }

    # Make sure the base backup path is defined (since all other path types are backup)
    if (!defined($self->{strBackupPath}))
    {
        confess &log(ASSERT, 'strBackupPath not defined');
    }

    # Get base backup path
    if ($strType eq PATH_BACKUP)
    {
        return $self->{strBackupPath} . (defined($strFile) ? "/${strFile}" : '');
    }

    # Make sure the cluster is defined
    if (!defined($self->{strStanza}))
    {
        confess &log(ASSERT, 'strStanza not defined');
    }

    # Get the backup tmp path
    if ($strType eq PATH_BACKUP_TMP)
    {
        my $strTempPath = "$self->{strBackupPath}/temp/$self->{strStanza}.tmp";

        if ($bTemp)
        {
            return "${strTempPath}/file.tmp" . (defined($self->{iThreadIdx}) ? ".$self->{iThreadIdx}" : '');
        }

        return "${strTempPath}" . (defined($strFile) ? "/${strFile}" : '');
    }

    # Get the backup archive path
    if ($strType eq PATH_BACKUP_ARCHIVE)
    {
        my $strArchivePath = "$self->{strBackupPath}/archive/$self->{strStanza}";
        my $strArchive;

        if ($bTemp)
        {
            return "${strArchivePath}/file.tmp" . (defined($self->{iThreadIdx}) ? ".$self->{iThreadIdx}" : '');
        }

        if (defined($strFile))
        {
            $strArchive = substr(basename($strFile), 0, 24);

            if ($strArchive !~ /^([0-F]){24}$/)
            {
                return "${strArchivePath}/${strFile}";
            }
        }

        return $strArchivePath . (defined($strArchive) ? '/' . substr($strArchive, 0, 16) : '') .
               (defined($strFile) ? '/' . $strFile : '');
    }

    if ($strType eq PATH_BACKUP_CLUSTER)
    {
        return $self->{strBackupPath} . "/backup/$self->{strStanza}" . (defined($strFile) ? "/${strFile}" : '');
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
        confess &log(ASSERT, 'path types must be equal in link create');
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
            $strSource = ('../' x substr($strDestination, $iCommonLen) =~ tr/\///) . substr($strSource, $iCommonLen);
        }
    }

    # Create the command
    my $strCommand = 'ln' . (!$bHard ? ' -s' : '') . " ${strSource} ${strDestination}";

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

    # Set defaults
    $bDestinationPathCreate = defined($bDestinationPathCreate) ? $bDestinationPathCreate : false;

    # Set operation variables
    my $strPathOpSource = $self->path_get($strSourcePathType, $strSourceFile);
    my $strPathOpDestination = $self->path_get($strDestinationPathType, $strDestinationFile);

    # Set operation and debug strings
    my $strOperation = OP_FILE_MOVE;

    my $strDebug = "${strSourcePathType}" . (defined($strSourceFile) ? ":${strSourceFile}" : '') .
                   " to ${strDestinationPathType}" . (defined($strDestinationFile) ? ":${strDestinationFile}" : '') .
                   ', destination_path_create = ' . ($bDestinationPathCreate ? 'true' : 'false');
    &log(DEBUG, "${strOperation}: ${strDebug}");

    # Source and destination path types must be the same
    if ($self->path_type_get($strSourcePathType) ne $self->path_type_get($strSourcePathType))
    {
        confess &log(ASSERT, "${strDebug}: source and destination path types must be equal");
    }

    # Run remotely
    if ($self->is_remote($strSourcePathType))
    {
        confess &log(ASSERT, "${strDebug}: remote operation not supported");
    }
    # Run locally
    else
    {
        if (!rename($strPathOpSource, $strPathOpDestination))
        {
            my $strError = "${strPathOpDestination} could not be moved: " . $!;
            my $iErrorCode = COMMAND_ERR_FILE_READ;

            if (!$self->exists(PATH_ABSOLUTE, dirname($strPathOpDestination)))
            {
                $strError = "${strPathOpDestination} does not exist";
                $iErrorCode = COMMAND_ERR_FILE_MISSING;
            }

            if (!($bDestinationPathCreate && $iErrorCode == COMMAND_ERR_FILE_MISSING))
            {
                if ($strSourcePathType eq PATH_ABSOLUTE)
                {
                    confess &log(ERROR, $strError, $iErrorCode);
                }

                confess &log(ERROR, "${strDebug}: " . $strError);
            }

            $self->path_create(PATH_ABSOLUTE, dirname($strPathOpDestination));

            if (!rename($strPathOpSource, $strPathOpDestination))
            {
                confess &log(ERROR, "unable to move file ${strPathOpSource}: " . $!);
            }
        }
    }
}

####################################################################################################################################
# COMPRESS
####################################################################################################################################
sub compress
{
    my $self = shift;
    my $strPathType = shift;
    my $strFile = shift;

    # Set operation variables
    my $strPathOp = $self->path_get($strPathType, $strFile);

    # Set operation and debug strings
    my $strOperation = OP_FILE_COMPRESS;

    my $strDebug = "${strPathType}:${strPathOp}";
    &log(DEBUG, "${strOperation}: ${strDebug}");

    # Run remotely
    if ($self->is_remote($strPathType))
    {
        confess &log(ASSERT, "${strDebug}: remote operation not supported");
    }
    # Run locally
    else
    {
        # Compress the file
        if (!gzip($strPathOp => "${strPathOp}.gz"))
        {
            my $strError = "${strPathOp} could not be compressed:" . $!;
            my $iErrorCode = COMMAND_ERR_FILE_READ;

            if (!$self->exists($strPathType, $strFile))
            {
                $strError = "${strPathOp} does not exist";
                $iErrorCode = COMMAND_ERR_FILE_MISSING;
            }

            if ($strPathType eq PATH_ABSOLUTE)
            {
                confess &log(ERROR, $strError, $iErrorCode);
            }

            confess &log(ERROR, "${strDebug}: " . $strError);
        }

        # Remove the old file
        unlink($strPathOp)
            or die &log(ERROR, "${strDebug}: unable to remove ${strPathOp}");
    }
}

####################################################################################################################################
# PATH_CREATE
#
# Creates a path locally or remotely.
####################################################################################################################################
sub path_create
{
    my $self = shift;
    my $strPathType = shift;
    my $strPath = shift;
    my $strPermission = shift;
    my $bIgnoreExists = shift;

    # Set operation variables
    my $strPathOp = $self->path_get($strPathType, $strPath);

    # Set operation and debug strings
    my $strOperation = OP_FILE_PATH_CREATE;
    my $strDebug = " ${strPathType}:${strPathOp}, permission " . (defined($strPermission) ? $strPermission : '[undef]');
    &log(DEBUG, "${strOperation}: ${strDebug}");

    if ($self->is_remote($strPathType))
    {
        # Build param hash
        my %oParamHash;

        $oParamHash{path} = ${strPathOp};

        if (defined($strPermission))
        {
            $oParamHash{permission} = ${strPermission};
        }

        # Add remote info to debug string
        my $strRemote = 'remote (' . $self->{oRemote}->command_param_string(\%oParamHash) . ')';
        $strDebug = "${strOperation}: ${strRemote}: ${strDebug}";
        &log(TRACE, "${strOperation}: ${strRemote}");

        # Execute the command
        $self->{oRemote}->command_execute($strOperation, \%oParamHash, false, $strDebug);
    }
    else
    {
        if (!($bIgnoreExists && $self->exists($strPathType, $strPath)))
        {
            # Attempt the create the directory
            my $stryError;

            if (defined($strPermission))
            {
                make_path($strPathOp, {mode => oct($strPermission), error => \$stryError});
            }
            else
            {
                make_path($strPathOp, {error => \$stryError});
            }

            if (@$stryError)
            {
                # Capture the error
                my $strError = "${strPathOp} could not be created: " . $!;

                # If running on command line the return directly
                if ($strPathType eq PATH_ABSOLUTE)
                {
                    confess &log(ERROR, $strError, COMMAND_ERR_PATH_CREATE);
                }

                # Error the normal way
                confess &log(ERROR, "${strDebug}: " . $strError); #, COMMAND_ERR_PATH_CREATE);
            }
        }
    }
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

    # Set operation variables
    my $strPathOp = $self->path_get($strPathType, $strPath);

    # Set operation and debug strings
    my $strOperation = OP_FILE_EXISTS;
    my $strDebug = "${strPathType}:${strPathOp}";
    &log(DEBUG, "${strOperation}: ${strDebug}");

    # Run remotely
    if ($self->is_remote($strPathType))
    {
        # Build param hash
        my %oParamHash;

        $oParamHash{path} = $strPathOp;

        # Add remote info to debug string
        my $strRemote = 'remote (' . $self->{oRemote}->command_param_string(\%oParamHash) . ')';
        $strDebug = "${strOperation}: ${strRemote}: ${strDebug}";
        &log(TRACE, "${strOperation}: ${strRemote}");

        # Execute the command
        return $self->{oRemote}->command_execute($strOperation, \%oParamHash, true, $strDebug) eq 'Y';
    }
    # Run locally
    else
    {
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
                    confess &log(ERROR, "${strDebug}: " . $!); #, COMMAND_ERR_FILE_READ);
                }
            }

            return false;
        }
    }

    return true;
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

    # Set defaults
    $bIgnoreMissing = defined($bIgnoreMissing) ? $bIgnoreMissing : true;

    # Set operation variables
    my $strPathOp = $self->path_get($strPathType, $strPath, $bTemp);
    my $bRemoved = true;

    # Set operation and debug strings
    my $strOperation = OP_FILE_EXISTS;
    my $strDebug = "${strPathType}:${strPathOp}";
    &log(DEBUG, "${strOperation}: ${strDebug}");

    # Run remotely
    if ($self->is_remote($strPathType))
    {
        confess &log(ASSERT, "${strDebug}: remote operation not supported");
    }
    # Run locally
    else
    {
        if (unlink($strPathOp) != 1)
        {
            $bRemoved = false;

            my $strError = "${strPathOp} could not be removed: " . $!;
            my $iErrorCode = COMMAND_ERR_PATH_READ;

            if (!$self->exists($strPathType, $strPath))
            {
                $strError = "${strPathOp} does not exist";
                $iErrorCode = COMMAND_ERR_PATH_MISSING;
            }

            if (!($iErrorCode == COMMAND_ERR_PATH_MISSING && $bIgnoreMissing))
            {
                if ($strPathType eq PATH_ABSOLUTE)
                {
                    confess &log(ERROR, $strError, $iErrorCode);
                }

                confess &log(ERROR, "${strDebug}: " . $strError);
            }
        }
    }

    return $bRemoved;
}

####################################################################################################################################
# HASH
####################################################################################################################################
sub hash
{
    my $self = shift;
    my $strPathType = shift;
    my $strFile = shift;
    my $bCompressed = shift;
    my $strHashType = shift;

    # Set defaults
    $bCompressed = defined($bCompressed) ? $bCompressed : false;
    $strHashType = defined($strHashType) ? $strHashType : 'sha1';

    # Set operation variables
    my $strFileOp = $self->path_get($strPathType, $strFile);
    my $strHash;

    # Set operation and debug strings
    my $strOperation = OP_FILE_HASH;
    my $strDebug = "${strPathType}:${strFileOp}, " .
                   'compressed = ' . ($bCompressed ? 'true' : 'false') . ', ' .
                   "hash_type = ${strHashType}";
    &log(DEBUG, "${strOperation}: ${strDebug}");

    if ($self->is_remote($strPathType))
    {
        confess &log(ASSERT, "${strDebug}: remote operation not supported");
    }
    else
    {
        my $hFile;

        if (!open($hFile, '<', $strFileOp))
        {
            my $strError = "${strFileOp} could not be read: " . $!;
            my $iErrorCode = 2;

            if (!$self->exists($strPathType, $strFile))
            {
                $strError = "${strFileOp} does not exist";
                $iErrorCode = 1;
            }

            if ($strPathType eq PATH_ABSOLUTE)
            {
                confess &log(ERROR, $strError, $iErrorCode);
            }

            confess &log(ERROR, "${strDebug}: " . $strError);
        }

        my $oSHA = Digest::SHA->new($strHashType);

        if ($bCompressed)
        {
            $oSHA->addfile($self->process_async_get()->process_begin('decompress', $hFile));
            $self->process_async_get()->process_end();
        }
        else
        {
            $oSHA->addfile($hFile);
        }

        close($hFile);

        $strHash = $oSHA->hexdigest();
    }

    return $strHash;
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
    my $bIgnoreMissing = shift;

    # Set defaults
    $strSortOrder = defined($strSortOrder) ? $strSortOrder : 'forward';
    $bIgnoreMissing = defined($bIgnoreMissing) ? $bIgnoreMissing : false;

    # Set operation variables
    my $strPathOp = $self->path_get($strPathType, $strPath);
    my @stryFileList;

    # Get the root path for the file list
    my $strOperation = OP_FILE_LIST;
    my $strDebug = "${strPathType}:${strPathOp}" .
                   ', expression ' . (defined($strExpression) ? $strExpression : '[UNDEF]') .
                   ", sort ${strSortOrder}";
    &log(DEBUG, "${strOperation}: ${strDebug}");

    # Run remotely
    if ($self->is_remote($strPathType))
    {
        # Build param hash
        my %oParamHash;

        $oParamHash{path} = $strPathOp;
        $oParamHash{sort_order} = $strSortOrder;
        $oParamHash{ignore_missing} = ${bIgnoreMissing};

        if (defined($strExpression))
        {
            $oParamHash{expression} = $strExpression;
        }

        # Add remote info to debug string
        my $strRemote = 'remote (' . $self->{oRemote}->command_param_string(\%oParamHash) . ')';
        $strDebug = "${strOperation}: ${strRemote}: ${strDebug}";
        &log(TRACE, "${strOperation}: ${strRemote}");

        # Execute the command
        my $strOutput = $self->{oRemote}->command_execute($strOperation, \%oParamHash, false, $strDebug);

        if (defined($strOutput))
        {
            @stryFileList = split(/\n/, $strOutput);
        }
    }
    # Run locally
    else
    {
        my $hPath;

        if (!opendir($hPath, $strPathOp))
        {
            my $strError = "${strPathOp} could not be read: " . $!;
            my $iErrorCode = COMMAND_ERR_PATH_READ;

            if (!$self->exists($strPathType, $strPath))
            {
                # If ignore missing is set then return an empty array
                if ($bIgnoreMissing)
                {
                    return @stryFileList;
                }

                $strError = "${strPathOp} does not exist";
                $iErrorCode = COMMAND_ERR_PATH_MISSING;
            }

            if ($strPathType eq PATH_ABSOLUTE)
            {
                confess &log(ERROR, $strError, $iErrorCode);
            }

            confess &log(ERROR, "${strDebug}: " . $strError);
        }

        @stryFileList = grep(!/^(\.)|(\.\.)$/i, readdir($hPath));

        close($hPath);

        if (defined($strExpression))
        {
            @stryFileList = grep(/$strExpression/i, @stryFileList);
        }

        # Reverse sort
        if ($strSortOrder eq 'reverse')
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

    # Set operation variables
    my $strPathOp = $self->path_get($strPathType, $strPath);

    # Set operation and debug strings
    my $strOperation = OP_FILE_MANIFEST;
    my $strDebug = "${strPathType}:${strPathOp}";
    &log(DEBUG, "${strOperation}: ${strDebug}");

    # Run remotely
    if ($self->is_remote($strPathType))
    {
        # Build param hash
        my %oParamHash;

        $oParamHash{path} = $strPathOp;

        # Add remote info to debug string
        my $strRemote = 'remote (' . $self->{oRemote}->command_param_string(\%oParamHash) . ')';
        $strDebug = "${strOperation}: ${strRemote}: ${strDebug}";
        &log(TRACE, "${strOperation}: ${strRemote}");

        # Execute the command
        data_hash_build($oManifestHashRef, $self->{oRemote}->command_execute($strOperation, \%oParamHash, true, $strDebug), "\t");
    }
    # Run locally
    else
    {
        $self->manifest_recurse($strPathType, $strPathOp, undef, 0, $oManifestHashRef, $strDebug);
    }
}

sub manifest_recurse
{
    my $self = shift;
    my $strPathType = shift;
    my $strPathOp = shift;
    my $strPathFileOp = shift;
    my $iDepth = shift;
    my $oManifestHashRef = shift;
    my $strDebug = shift;

    # Set operation and debug strings
    $strDebug = $strDebug . (defined($strPathFileOp) ? " => ${strPathFileOp}" : '');
    my $strPathRead = $strPathOp . (defined($strPathFileOp) ? "/${strPathFileOp}" : '');
    my $hPath;

    # Open the path
    if (!opendir($hPath, $strPathRead))
    {
        my $strError = "${strPathRead} could not be read: " . $!;
        my $iErrorCode = COMMAND_ERR_PATH_READ;

        # If the path does not exist and is not the root path requested then return, else error
        # It's OK for paths to go away during execution (databases are a dynamic thing!)
        if (!$self->exists(PATH_ABSOLUTE, $strPathRead))
        {
            if ($iDepth != 0)
            {
                return;
            }

            $strError = "${strPathRead} does not exist";
            $iErrorCode = COMMAND_ERR_PATH_MISSING;
        }

        if ($strPathType eq PATH_ABSOLUTE)
        {
            confess &log(ERROR, $strError, $iErrorCode);
        }

        confess &log(ERROR, "${strDebug}: " . $strError);
    }

    # Get a list of all files in the path (except ..)
    my @stryFileList = grep(!/^\..$/i, readdir($hPath));

    close($hPath);

    # Loop through all subpaths/files in the path
    foreach my $strFile (@stryFileList)
    {
        my $strPathFile = "${strPathRead}/$strFile";
        my $bCurrentDir = $strFile eq '.';

        # Create the file and path names
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

        # Stat the path/file
        my $oStat = lstat($strPathFile);

        # Check for errors in stat
        if (!defined($oStat))
        {
            my $strError = "${strPathFile} could not be read: " . $!;
            my $iErrorCode = COMMAND_ERR_FILE_READ;

            # If the file does not exist then go to the next file, else error
            # It's OK for files to go away during execution (databases are a dynamic thing!)
            if (!$self->exists(PATH_ABSOLUTE, $strPathFile))
            {
                next;
            }

            if ($strPathType eq PATH_ABSOLUTE)
            {
                confess &log(ERROR, $strError, $iErrorCode);
            }

            confess &log(ERROR, "${strDebug}: " . $strError);
        }

        # Check for regular file
        if (S_ISREG($oStat->mode))
        {
            ${$oManifestHashRef}{name}{"${strFile}"}{type} = 'f';

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
            ${$oManifestHashRef}{name}{"${strFile}"}{type} = 'd';
        }
        # Check for link
        elsif (S_ISLNK($oStat->mode))
        {
            ${$oManifestHashRef}{name}{"${strFile}"}{type} = 'l';

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

                    confess &log(ERROR, "${strDebug}: " . $strError);
                }
            }
        }
        # Not a recognized type
        else
        {
            my $strError = "${strPathFile} is not of type directory, file, or link";

            if ($strPathType eq PATH_ABSOLUTE)
            {
                print $strError;
                exit COMMAND_ERR_FILE_TYPE;
            }

            confess &log(ERROR, "${strDebug}: " . $strError);
        }

        # Get user name
        ${$oManifestHashRef}{name}{"${strFile}"}{user} = getpwuid($oStat->uid);

        # Get group name
        ${$oManifestHashRef}{name}{"${strFile}"}{group} = getgrgid($oStat->gid);

        # Get permissions
        if (${$oManifestHashRef}{name}{"${strFile}"}{type} ne 'l')
        {
            ${$oManifestHashRef}{name}{"${strFile}"}{permission} = sprintf('%04o', S_IMODE($oStat->mode));
        }

        # Recurse into directories
        if (${$oManifestHashRef}{name}{"${strFile}"}{type} eq 'd' && !$bCurrentDir)
        {
            $self->manifest_recurse($strPathType, $strPathOp, $strFile, $iDepth + 1, $oManifestHashRef, $strDebug);
        }
    }
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
    my $bSourceCompressed = shift;
    my $bDestinationCompress = shift;
    my $bIgnoreMissingSource = shift;
    my $lModificationTime = shift;
    my $strPermission = shift;
    my $bDestinationPathCreate = shift;

    # Set defaults
    $bSourceCompressed = defined($bSourceCompressed) ? $bSourceCompressed : false;
    $bDestinationCompress = defined($bDestinationCompress) ? $bDestinationCompress : false;
    $bIgnoreMissingSource = defined($bIgnoreMissingSource) ? $bIgnoreMissingSource : false;
    $bDestinationPathCreate = defined($bDestinationPathCreate) ? $bDestinationPathCreate : false;

    # Set working variables
    my $bSourceRemote = $self->is_remote($strSourcePathType) || $strSourcePathType eq PIPE_STDIN;
    my $bDestinationRemote = $self->is_remote($strDestinationPathType) || $strDestinationPathType eq PIPE_STDOUT;
    my $strSourceOp = $strSourcePathType eq PIPE_STDIN ?
        $strSourcePathType : $self->path_get($strSourcePathType, $strSourceFile);
    my $strDestinationOp = $strDestinationPathType eq PIPE_STDOUT ?
        $strDestinationPathType : $self->path_get($strDestinationPathType, $strDestinationFile);
    my $strDestinationTmpOp = $strDestinationPathType eq PIPE_STDOUT ?
        undef : $self->path_get($strDestinationPathType, $strDestinationFile, true);

    # Set debug string and log
    my $strDebug = ($bSourceRemote ? ' remote' : ' local') . " ${strSourcePathType}" .
                   (defined($strSourceFile) ? ":${strSourceOp}" : '') .
                   ' to' . ($bDestinationRemote ? ' remote' : ' local') . " ${strDestinationPathType}" .
                   (defined($strDestinationFile) ? ":${strDestinationOp}" : '') .
                   ', source_compressed = ' . ($bSourceCompressed ? 'true' : 'false') .
                   ', destination_compress = ' . ($bDestinationCompress ? 'true' : 'false') .
                   ', ignore_missing_source = ' . ($bIgnoreMissingSource ? 'true' : 'false') .
                   ', destination_path_create = ' . ($bDestinationPathCreate ? 'true' : 'false');
    &log(DEBUG, OP_FILE_COPY . ": ${strDebug}");

    # Open the source and destination files (if needed)
    my $hSourceFile;
    my $hDestinationFile;

    if (!$bSourceRemote)
    {
        if (!open($hSourceFile, '<', $strSourceOp))
        {
            my $strError = $!;
            my $iErrorCode = COMMAND_ERR_FILE_READ;

            if ($!{ENOENT})
            {
                # $strError = 'file is missing';
                $iErrorCode = COMMAND_ERR_FILE_MISSING;

                if ($bIgnoreMissingSource && $strDestinationPathType ne PIPE_STDOUT)
                {
                    return false;
                }
            }

            $strError = "cannot open source file ${strSourceOp}: " . $strError;

            if ($strSourcePathType eq PATH_ABSOLUTE)
            {
                if ($strDestinationPathType eq PIPE_STDOUT)
                {
                    $self->{oRemote}->write_line(*STDOUT, 'block 0');
                }

                confess &log(ERROR, $strError, $iErrorCode);
            }

            confess &log(ERROR, "${strDebug}: " . $strError, $iErrorCode);
        }
    }

    if (!$bDestinationRemote)
    {
        # Open the destination temp file
        if (!open($hDestinationFile, '>', $strDestinationTmpOp))
        {
            my $strError = "${strDestinationTmpOp} could not be opened: " . $!;
            my $iErrorCode = COMMAND_ERR_FILE_READ;

            if (!$self->exists(PATH_ABSOLUTE, dirname($strDestinationTmpOp)))
            {
                $strError = dirname($strDestinationTmpOp) . ' does not exist';
                $iErrorCode = COMMAND_ERR_FILE_MISSING;
            }

            if (!($bDestinationPathCreate && $iErrorCode == COMMAND_ERR_FILE_MISSING))
            {
                if ($strSourcePathType eq PATH_ABSOLUTE)
                {
                    confess &log(ERROR, $strError, $iErrorCode);
                }

                confess &log(ERROR, "${strDebug}: " . $strError);
            }

            $self->path_create(PATH_ABSOLUTE, dirname($strDestinationTmpOp));

            if (!open($hDestinationFile, '>', $strDestinationTmpOp))
            {
                confess &log(ERROR, "unable to open destination file ${strDestinationOp}: " . $!);
            }
        }
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
        my $strOperation;

        # If source is remote and destination is local
        if ($bSourceRemote && !$bDestinationRemote)
        {
            $hOut = $hDestinationFile;
            $strOperation = OP_FILE_COPY_OUT;
            $strRemote = 'in';

            if ($strSourcePathType eq PIPE_STDIN)
            {
                $hIn = *STDIN;
            }
            else
            {
                $oParamHash{source_file} = $strSourceOp;
                $oParamHash{source_compressed} = $bSourceCompressed;

                $hIn = $self->{oRemote}->{hOut};
            }
        }
        # Else if source is local and destination is remote
        elsif (!$bSourceRemote && $bDestinationRemote)
        {
            $hIn = $hSourceFile;
            $strOperation = OP_FILE_COPY_IN;
            $strRemote = 'out';

            if ($strDestinationPathType eq PIPE_STDOUT)
            {
                $hOut = *STDOUT;
            }
            else
            {
                $oParamHash{destination_file} = $strDestinationOp;
                $oParamHash{destination_compress} = $bDestinationCompress;
                $oParamHash{destination_path_create} = $bDestinationPathCreate;

                if (defined($strPermission))
                {
                    $oParamHash{permission} = $strPermission;
                }

                $hOut = $self->{oRemote}->{hIn};
            }
        }
        # Else source and destination are remote
        else
        {
            $strOperation = OP_FILE_COPY;

            $oParamHash{source_file} = $strSourceOp;
            $oParamHash{source_compressed} = $bSourceCompressed;
            $oParamHash{destination_file} = $strDestinationOp;
            $oParamHash{destination_compress} = $bDestinationCompress;
            $oParamHash{destination_path_create} = $bDestinationPathCreate;

            if (defined($strPermission))
            {
                $oParamHash{permission} = $strPermission;
            }

            if ($bIgnoreMissingSource)
            {
                $oParamHash{ignore_missing_source} = $bIgnoreMissingSource;
            }
        }

        # Build debug string
        if (%oParamHash)
        {
            my $strRemote = 'remote (' . $self->{oRemote}->command_param_string(\%oParamHash) . ')';
            $strDebug = "${strOperation}: ${strRemote}: ${strDebug}";

            &log(TRACE, "${strOperation}: ${strRemote}");
        }

        # If an operation is defined then write it
        if (%oParamHash)
        {
            $self->{oRemote}->command_write($strOperation, \%oParamHash);
        }

        # Transfer the file (skip this for copies where both sides are remote)
        if ($strOperation ne OP_FILE_COPY)
        {
            $self->{oRemote}->binary_xfer($hIn, $hOut, $strRemote, $bSourceCompressed, $bDestinationCompress);
        }

        # If this is the controlling process then wait for OK from remote
        if (%oParamHash)
        {
            # Test for an error when reading output
            my $strOutput;

            eval
            {
                $strOutput = $self->{oRemote}->output_read($strOperation eq OP_FILE_COPY, $strDebug, true);
            };

            # If there is an error then evaluate
            if ($@)
            {
                my $oMessage = $@;

                # We'll ignore this error if the source file was missing and missing file exception was returned
                # and bIgnoreMissingSource is set
                if ($bIgnoreMissingSource && $strRemote eq 'in' && $oMessage->isa('BackRest::Exception') &&
                    $oMessage->code() == COMMAND_ERR_FILE_MISSING)
                {
                    close($hDestinationFile) or confess &log(ERROR, "cannot close file ${strDestinationTmpOp}");
                    unlink($strDestinationTmpOp) or confess &log(ERROR, "cannot remove file ${strDestinationTmpOp}");

                    return false;
                }

                # Otherwise report the error
                confess $oMessage;
            }

            # If this was a remote copy, then return the result
            if ($strOperation eq OP_FILE_COPY)
            {
                return false; #$strOutput eq 'N' ? true : false;
            }
        }
    }
    # Else this is a local operation
    else
    {
        # If the source is compressed and the destination is not then decompress
        if ($bSourceCompressed && !$bDestinationCompress)
        {
            gunzip($hSourceFile => $hDestinationFile)
                or die confess &log(ERROR, "${strDebug}: unable to uncompress: " . $GunzipError);
        }
        elsif (!$bSourceCompressed && $bDestinationCompress)
        {
            gzip($hSourceFile => $hDestinationFile)
                or die confess &log(ERROR, "${strDebug}: unable to compress: " . $GzipError);
        }
        else
        {
           cp($hSourceFile, $hDestinationFile)
               or die confess &log(ERROR, "${strDebug}: unable to copy: " . $!);
        }
    }

    # Close the source file (if open)
    if (defined($hSourceFile))
    {
        close($hSourceFile) or confess &log(ERROR, "cannot close file ${strSourceOp}");
    }

    # Close the destination file (if open)
    if (defined($hDestinationFile))
    {
        close($hDestinationFile) or confess &log(ERROR, "cannot close file ${strDestinationTmpOp}");
    }

    # Where the destination is local, set permissions, modification time, and perform move to final location
    if (!$bDestinationRemote)
    {
        # Set the file permission if required
        if (defined($strPermission))
        {
            chmod(oct($strPermission), $strDestinationTmpOp)
                or confess &log(ERROR, "unable to set permissions for local ${strDestinationTmpOp}");
        }

        # Set the file modification time if required
        if (defined($lModificationTime))
        {
            utime($lModificationTime, $lModificationTime, $strDestinationTmpOp)
                or confess &log(ERROR, "unable to set time for local ${strDestinationTmpOp}");
        }

        # Move the file from tmp to final destination
        $self->move(PATH_ABSOLUTE, $strDestinationTmpOp, PATH_ABSOLUTE, $strDestinationOp, true);
    }

    return true;
}

no Moose;
__PACKAGE__->meta->make_immutable;
