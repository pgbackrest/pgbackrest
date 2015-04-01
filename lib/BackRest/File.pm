####################################################################################################################################
# FILE MODULE
####################################################################################################################################
package BackRest::File;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Net::OpenSSH;
use File::Basename qw(dirname basename);
use File::Copy qw(cp);
use File::Path qw(make_path remove_tree);
use Digest::SHA;
use File::stat;
use Fcntl qw(:mode O_RDONLY O_WRONLY O_CREAT O_EXCL);
use Exporter qw(import);

use lib dirname($0) . '/../lib';
use BackRest::Exception;
use BackRest::Utility;
use BackRest::Config;
use BackRest::Remote;

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

our @EXPORT = qw(COMMAND_ERR_FILE_MISSING COMMAND_ERR_FILE_READ COMMAND_ERR_FILE_MOVE COMMAND_ERR_FILE_TYPE COMMAND_ERR_LINK_READ
                 COMMAND_ERR_PATH_MISSING COMMAND_ERR_PATH_CREATE COMMAND_ERR_PARAM);

####################################################################################################################################
# PATH_GET Constants
####################################################################################################################################
use constant
{
    PATH_ABSOLUTE           => 'absolute',
    PATH_DB                 => 'db',
    PATH_DB_ABSOLUTE        => 'db:absolute',
    PATH_BACKUP             => 'backup',
    PATH_BACKUP_ABSOLUTE    => 'backup:absolute',
    PATH_BACKUP_CLUSTER     => 'backup:cluster',
    PATH_BACKUP_TMP         => 'backup:tmp',
    PATH_BACKUP_ARCHIVE     => 'backup:archive',
    PATH_BACKUP_ARCHIVE_OUT => 'backup:archive:out'
};

push @EXPORT, qw(PATH_ABSOLUTE PATH_DB PATH_DB_ABSOLUTE PATH_BACKUP PATH_BACKUP_ABSOLUTE PATH_BACKUP_CLUSTER PATH_BACKUP_TMP
                 PATH_BACKUP_ARCHIVE PATH_BACKUP_ARCHIVE_OUT);

####################################################################################################################################
# STD Pipe Constants
####################################################################################################################################
use constant
{
    PIPE_STDIN   => '<STDIN>',
    PIPE_STDOUT  => '<STDOUT>',
    PIPE_STDERR  => '<STDERR>'
};

push @EXPORT, qw(PIPE_STDIN PIPE_STDOUT PIPE_STDERR);

####################################################################################################################################
# Operation constants
####################################################################################################################################
use constant
{
    OP_FILE_OWNER       => 'File->owner',
    OP_FILE_WAIT        => 'File->wait',
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
    OP_FILE_PATH_CREATE => 'File->path_create',
    OP_FILE_LINK_CREATE => 'File->link_create'
};

push @EXPORT, qw(OP_FILE_OWNER OP_FILE_WAIT OP_FILE_LIST OP_FILE_EXISTS OP_FILE_HASH OP_FILE_REMOVE OP_FILE_MANIFEST
                 OP_FILE_COMPRESS OP_FILE_MOVE OP_FILE_COPY OP_FILE_COPY_OUT OP_FILE_COPY_IN OP_FILE_PATH_CREATE);

####################################################################################################################################
# CONSTRUCTOR
####################################################################################################################################
sub new
{
    my $class = shift;
    my $strStanza = shift;
    my $strBackupPath = shift;
    my $strRemote = shift;
    my $oRemote = shift;
    my $strDefaultPathMode = shift;
    my $strDefaultFileMode = shift;
    my $iThreadIdx = shift;

    # Create the class hash
    my $self = {};
    bless $self, $class;

    # Default compression extension to gz
    $self->{strCompressExtension} = 'gz';

    # Default file and path mode
    $self->{strDefaultPathMode} = defined($strDefaultPathMode) ? $strDefaultPathMode : '0750';
    $self->{strDefaultFileMode} = defined($strDefaultFileMode) ? $strDefaultFileMode : '0640';

    # Initialize other variables
    $self->{strStanza} = $strStanza;
    $self->{strBackupPath} = $strBackupPath;
    $self->{strRemote} = $strRemote;
    $self->{oRemote} = $oRemote;
    $self->{iThreadIdx} = $iThreadIdx;

    # Remote object must be set
    if (!defined($self->{oRemote}))
    {
        confess &log(ASSERT, 'oRemote must be defined');
    }

    # If remote is defined check parameters and open session
    if (defined($self->{strRemote}) && $self->{strRemote} ne NONE)
    {
        # Make sure remote is valid
        if ($self->{strRemote} ne DB && $self->{strRemote} ne BACKUP)
        {
            confess &log(ASSERT, 'strRemote must be "' . DB . '" or "' . BACKUP .
                                 "\", $self->{strRemote} was passed");
        }
    }

    return $self;
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
        $self->{strStanza},
        $self->{strBackupPath},
        $self->{strRemote},
        defined($self->{oRemote}) ? $self->{oRemote}->clone() : undef,
        $self->{strDefaultPathMode},
        $self->{strDefaultFileMode},
        $iThreadIdx
    );
}

####################################################################################################################################
# stanza
####################################################################################################################################
sub stanza
{
    my $self = shift;

    return $self->{strStanza};
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

    # Only allow temp files for PATH_BACKUP_ARCHIVE, PATH_BACKUP_ARCHIVE_OUT, PATH_BACKUP_TMP and any absolute path
    $bTemp = defined($bTemp) ? $bTemp : false;

    if ($bTemp && !($strType eq PATH_BACKUP_ARCHIVE || $strType eq PATH_BACKUP_ARCHIVE_OUT || $strType eq PATH_BACKUP_TMP ||
        $bAbsolute))
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
    if ($strType eq PATH_BACKUP_ARCHIVE_OUT || $strType eq PATH_BACKUP_ARCHIVE)
    {
        my $strArchivePath = "$self->{strBackupPath}/archive/$self->{strStanza}";

        if ($strType eq PATH_BACKUP_ARCHIVE)
        {
            my $strArchive;

            if (defined($strFile))
            {
                $strArchive = substr(basename($strFile), 0, 24);

                if ($strArchive !~ /^([0-F]){24}$/)
                {
                    return "${strArchivePath}/${strFile}";
                }
            }

            $strArchivePath = $strArchivePath . (defined($strArchive) ? '/' . substr($strArchive, 0, 16) : '') .
                              (defined($strFile) ? '/' . $strFile : '');
        }
        else
        {
            $strArchivePath = "${strArchivePath}/out" . (defined($strFile) ? '/' . $strFile : '');
        }

        if ($bTemp)
        {
            if (!defined($strFile))
            {
                confess &log(ASSERT, 'archive temp must have strFile defined');
            }

            return "${strArchivePath}.tmp";
        }

        return $strArchivePath;
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

    # Source and destination path types must be the same (e.g. both PATH_DB or both PATH_BACKUP, etc.)
    if ($self->path_type_get($strSourcePathType) ne $self->path_type_get($strDestinationPathType))
    {
        confess &log(ASSERT, 'path types must be equal in link create');
    }

    # Generate source and destination files
    my $strSource = $self->path_get($strSourcePathType, $strSourceFile);
    my $strDestination = $self->path_get($strDestinationPathType, $strDestinationFile);

    # Set operation and debug strings
    my $strOperation = OP_FILE_LINK_CREATE;

    my $strDebug = "${strSourcePathType}" . (defined($strSource) ? ":${strSource}" : '') .
                   " to ${strDestinationPathType}" . (defined($strDestination) ? ":${strDestination}" : '') .
                   ', hard = ' . ($bHard ? 'true' : 'false') . ", relative = " . ($bRelative ? 'true' : 'false') .
                   ', destination_path_create = ' . ($bPathCreate ? 'true' : 'false');
    &log(DEBUG, "${strOperation}: ${strDebug}");

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

    # Run remotely
    if ($self->is_remote($strSourcePathType))
    {
        confess &log(ASSERT, "${strDebug}: remote operation not supported");
    }
    # Run locally
    else
    {
        if ($bHard)
        {
            link($strSource, $strDestination)
                or confess &log(ERROR, "unable to create hardlink from ${strSource} to ${strDestination}");
        }
        else
        {
            symlink($strSource, $strDestination)
                or confess &log(ERROR, "unable to create symlink from ${strSource} to ${strDestination}");
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
        # Use copy to compress the file
        $self->copy($strPathType, $strFile, $strPathType, "${strFile}.gz", false, true);

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
    my $strMode = shift;
    my $bIgnoreExists = shift;

    # Set operation variables
    my $strPathOp = $self->path_get($strPathType, $strPath);

    # Set operation and debug strings
    my $strOperation = OP_FILE_PATH_CREATE;
    my $strDebug = " ${strPathType}:${strPathOp}, mode " . (defined($strMode) ? $strMode : '[undef]');
    &log(DEBUG, "${strOperation}: ${strDebug}");

    if ($self->is_remote($strPathType))
    {
        # Build param hash
        my %oParamHash;

        $oParamHash{path} = ${strPathOp};

        if (defined($strMode))
        {
            $oParamHash{mode} = ${strMode};
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

            if (defined($strMode))
            {
                make_path($strPathOp, {mode => oct($strMode), error => \$stryError});
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
    my $strOperation = OP_FILE_REMOVE;
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

    my ($strHash, $iSize) = $self->hash_size($strPathType, $strFile, $bCompressed, $strHashType);

    return $strHash;
}

####################################################################################################################################
# HASH_SIZE
####################################################################################################################################
sub hash_size
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
    my $iSize = 0;

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

        if (!sysopen($hFile, $strFileOp, O_RDONLY))
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
            ($strHash, $iSize) =
                $self->{oRemote}->binary_xfer($hFile, undef, 'in', true, false, false);
        }
        else
        {
            my $iBlockSize;
            my $tBuffer;

            do
            {
                # Read a block from the file
                $iBlockSize = sysread($hFile, $tBuffer, 4194304);

                if (!defined($iBlockSize))
                {
                    confess &log(ERROR, "${strFileOp} could not be read: " . $!);
                }

                $iSize += $iBlockSize;
                $oSHA->add($tBuffer);
            }
            while ($iBlockSize > 0);

            $strHash = $oSHA->hexdigest();
        }

        close($hFile);
    }

    return $strHash, $iSize;
}

####################################################################################################################################
# OWNER
####################################################################################################################################
sub owner
{
    my $self = shift;
    my $strPathType = shift;
    my $strFile = shift;
    my $strUser = shift;
    my $strGroup = shift;

    # Set operation variables
    my $strFileOp = $self->path_get($strPathType, $strFile);

    # Set operation and debug strings
    my $strOperation = OP_FILE_OWNER;
    my $strDebug = "${strPathType}:${strFileOp}, " .
                   'user = ' . (defined($strUser) ? $strUser : '[undef]') .
                   'group = ' . (defined($strGroup) ? $strGroup : '[undef]');
    &log(DEBUG, "${strOperation}: ${strDebug}");

    if ($self->is_remote($strPathType))
    {
        confess &log(ASSERT, "${strDebug}: remote operation not supported");
    }
    else
    {
        my $iUserId;
        my $iGroupId;
        my $oStat;

        if (!defined($strUser) || !defined($strGroup))
        {
            $oStat = stat($strFileOp);

            if (!defined($oStat))
            {
                confess &log(ERROR, 'unable to stat ${strFileOp}');
            }
        }

        if (defined($strUser))
        {
            $iUserId = getpwnam($strUser);
        }
        else
        {
            $iUserId = $oStat->uid;
        }

        if (defined($strGroup))
        {
            $iGroupId = getgrnam($strGroup);
        }
        else
        {
            $iGroupId = $oStat->gid;
        }

        chown($iUserId, $iGroupId, $strFileOp)
            or confess &log(ERROR, "unable to set ownership for ${strFileOp}");
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
# WAIT
#
# Wait until the next second.  This is done in the file object because it must be performed on whichever side the db is on, local or
# remote.  This function is used to make sure that no files are copied in the same second as the manifest is created.  The reason is
# that the db might modify they file again in the same second as the copy and that change will not be visible to a subsequent
# incremental backup using timestamp/size to determine deltas.
####################################################################################################################################
sub wait
{
    my $self = shift;
    my $strPathType = shift;

    # Set operation and debug strings
    my $strOperation = OP_FILE_WAIT;
    my $strDebug = "${strPathType}";
    &log(DEBUG, "${strOperation}: ${strDebug}");

    # Second when the function was called
    my $lTimeBegin;

    # Run remotely
    if ($self->is_remote($strPathType))
    {
        # Add remote info to debug string
        $strDebug = "${strOperation}: remote: ${strDebug}";
        &log(TRACE, "${strOperation}: remote");

        # Execute the command
        $lTimeBegin = $self->{oRemote}->command_execute($strOperation, undef, true, $strDebug);
    }
    # Run locally
    else
    {
        # Wait the remainder of the current second
        $lTimeBegin = wait_remainder();
    }

    return $lTimeBegin;
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

        # Get mode
        if (${$oManifestHashRef}{name}{"${strFile}"}{type} ne 'l')
        {
            ${$oManifestHashRef}{name}{"${strFile}"}{mode} = sprintf('%04o', S_IMODE($oStat->mode));
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
# * modification time, mode, and ownership can be set on destination file
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
    my $strMode = shift;
    my $bDestinationPathCreate = shift;
    my $strUser = shift;
    my $strGroup = shift;
    my $bAppendChecksum = shift;

    # Set defaults
    $bSourceCompressed = defined($bSourceCompressed) ? $bSourceCompressed : false;
    $bDestinationCompress = defined($bDestinationCompress) ? $bDestinationCompress : false;
    $bIgnoreMissingSource = defined($bIgnoreMissingSource) ? $bIgnoreMissingSource : false;
    $bDestinationPathCreate = defined($bDestinationPathCreate) ? $bDestinationPathCreate : false;
    $bAppendChecksum = defined($bAppendChecksum) ? $bAppendChecksum : false;

    # Set working variables
    my $bSourceRemote = $self->is_remote($strSourcePathType) || $strSourcePathType eq PIPE_STDIN;
    my $bDestinationRemote = $self->is_remote($strDestinationPathType) || $strDestinationPathType eq PIPE_STDOUT;
    my $strSourceOp = $strSourcePathType eq PIPE_STDIN ?
        $strSourcePathType : $self->path_get($strSourcePathType, $strSourceFile);
    my $strDestinationOp = $strDestinationPathType eq PIPE_STDOUT ?
        $strDestinationPathType : $self->path_get($strDestinationPathType, $strDestinationFile);
    my $strDestinationTmpOp = $strDestinationPathType eq PIPE_STDOUT ?
        undef : $self->path_get($strDestinationPathType, $strDestinationFile, true);

    # Checksum and size variables
    my $strChecksum = undef;
    my $iFileSize = undef;
    my $bResult = true;

    # Set debug string and log
    my $strDebug = ($bSourceRemote ? ' remote' : ' local') . " ${strSourcePathType}" .
                   (defined($strSourceFile) ? ":${strSourceOp}" : '') .
                   ' to' . ($bDestinationRemote ? ' remote' : ' local') . " ${strDestinationPathType}" .
                   (defined($strDestinationFile) ? ":${strDestinationOp}" : '') .
                   ', source_compressed = ' . ($bSourceCompressed ? 'true' : 'false') .
                   ', destination_compress = ' . ($bDestinationCompress ? 'true' : 'false') .
                   ', ignore_missing_source = ' . ($bIgnoreMissingSource ? 'true' : 'false') .
                   ', destination_path_create = ' . ($bDestinationPathCreate ? 'true' : 'false') .
                   ', modification_time = ' . (defined($lModificationTime) ? $lModificationTime : '[undef]') .
                   ', mode = ' . (defined($strMode) ? $strMode : '[undef]') .
                   ', user = ' . (defined($strUser) ? $strUser : '[undef]') .
                   ', group = ' . (defined($strGroup) ? $strGroup : '[undef]');
    &log(DEBUG, OP_FILE_COPY . ": ${strDebug}");

    # Open the source and destination files (if needed)
    my $hSourceFile;
    my $hDestinationFile;

    if (!$bSourceRemote)
    {
        if (!sysopen($hSourceFile, $strSourceOp, O_RDONLY))
        {
            my $strError = $!;
            my $iErrorCode = COMMAND_ERR_FILE_READ;

            if ($!{ENOENT})
            {
                # $strError = 'file is missing';
                $iErrorCode = COMMAND_ERR_FILE_MISSING;

                if ($bIgnoreMissingSource && $strDestinationPathType ne PIPE_STDOUT)
                {
                    return false, undef, undef;
                }
            }

            $strError = "cannot open source file ${strSourceOp}: " . $strError;

            if ($strSourcePathType eq PATH_ABSOLUTE)
            {
                if ($strDestinationPathType eq PIPE_STDOUT)
                {
                    $self->{oRemote}->write_line(*STDOUT, 'block -1');
                }

                confess &log(ERROR, $strError, $iErrorCode);
            }

            confess &log(ERROR, "${strDebug}: " . $strError, $iErrorCode);
        }
    }

    if (!$bDestinationRemote)
    {
        # Open the destination temp file
        if (!sysopen($hDestinationFile, $strDestinationTmpOp, O_WRONLY | O_CREAT))
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

            if (!sysopen($hDestinationFile, $strDestinationTmpOp, O_WRONLY | O_CREAT | O_EXCL))
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
                $oParamHash{destination_compress} = $bDestinationCompress;

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
                $oParamHash{source_compressed} = $bSourceCompressed;
                $oParamHash{destination_compress} = $bDestinationCompress;
                $oParamHash{destination_path_create} = $bDestinationPathCreate;

                if (defined($strMode))
                {
                    $oParamHash{mode} = $strMode;
                }

                if (defined($strUser))
                {
                    $oParamHash{user} = $strUser;
                }

                if (defined($strGroup))
                {
                    $oParamHash{group} = $strGroup;
                }

                if ($bAppendChecksum)
                {
                    $oParamHash{append_checksum} = true;
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

            if (defined($strMode))
            {
                $oParamHash{mode} = $strMode;
            }

            if (defined($strUser))
            {
                $oParamHash{user} = $strUser;
            }

            if (defined($strGroup))
            {
                $oParamHash{group} = $strGroup;
            }

            if ($bIgnoreMissingSource)
            {
                $oParamHash{ignore_missing_source} = $bIgnoreMissingSource;
            }

            if ($bAppendChecksum)
            {
                $oParamHash{append_checksum} = true;
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
            ($strChecksum, $iFileSize) =
                $self->{oRemote}->binary_xfer($hIn, $hOut, $strRemote, $bSourceCompressed, $bDestinationCompress);
        }

        # If this is the controlling process then wait for OK from remote
        if (%oParamHash)
        {
            # Test for an error when reading output
            my $strOutput;

            eval
            {
                $strOutput = $self->{oRemote}->output_read(true, $strDebug, true);

                # Check the result of the remote call
                if (substr($strOutput, 0, 1) eq 'Y')
                {
                    # If the operation was purely remote, get checksum/size
                    if ($strOperation eq OP_FILE_COPY ||
                        $strOperation eq OP_FILE_COPY_IN && $bSourceCompressed && !$bDestinationCompress)
                    {
                        # Checksum shouldn't already be set
                        if (defined($strChecksum) || defined($iFileSize))
                        {
                            confess &log(ASSERT, "checksum and size are already defined, but shouldn't be");
                        }

                        # Parse output and check to make sure tokens are defined
                        my @stryToken = split(/ /, $strOutput);

                        if (!defined($stryToken[1]) || !defined($stryToken[2]) ||
                            $stryToken[1] eq '?' && $stryToken[2] eq '?')
                        {
                            confess &log(ERROR, "invalid return from copy" . (defined($strOutput) ? ": ${strOutput}" : ''));
                        }

                        # Read the checksum and size
                        if ($stryToken[1] ne '?')
                        {
                            $strChecksum = $stryToken[1];
                        }

                        if ($stryToken[2] ne '?')
                        {
                            $iFileSize = $stryToken[2];
                        }
                    }
                }
                # Remote called returned false
                else
                {
                    $bResult = false;
                }
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

                    return false, undef, undef;
                }

                confess $oMessage;
            }
        }
    }
    # Else this is a local operation
    else
    {
        # If the source is not compressed and the destination is then compress
        if (!$bSourceCompressed && $bDestinationCompress)
        {
            ($strChecksum, $iFileSize) =
                $self->{oRemote}->binary_xfer($hSourceFile, $hDestinationFile, 'out', false, true, false);
        }
        # If the source is compressed and the destination is not then decompress
        elsif ($bSourceCompressed && !$bDestinationCompress)
        {
            ($strChecksum, $iFileSize) =
                $self->{oRemote}->binary_xfer($hSourceFile, $hDestinationFile, 'in', true, false, false);
        }
        # Else both side are compressed, so copy capturing checksum
        elsif ($bSourceCompressed)
        {
            ($strChecksum, $iFileSize) =
                $self->{oRemote}->binary_xfer($hSourceFile, $hDestinationFile, 'out', true, true, false);
        }
        else
        {
            ($strChecksum, $iFileSize) =
                $self->{oRemote}->binary_xfer($hSourceFile, $hDestinationFile, 'in', false, true, false);
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

    # Checksum and file size should be set if the destination is not remote
    if ($bResult &&
        !(!$bSourceRemote && $bDestinationRemote && $bSourceCompressed) &&
        (!defined($strChecksum) || !defined($iFileSize)))
    {
        confess &log(ASSERT, "${strDebug}: checksum or file size not set");
    }

    # Where the destination is local, set mode, modification time, and perform move to final location
    if ($bResult && !$bDestinationRemote)
    {
        # Set the file Mode if required
        if (defined($strMode))
        {
            chmod(oct($strMode), $strDestinationTmpOp)
                or confess &log(ERROR, "unable to set mode for local ${strDestinationTmpOp}");
        }

        # Set the file modification time if required
        if (defined($lModificationTime))
        {
            utime($lModificationTime, $lModificationTime, $strDestinationTmpOp)
                or confess &log(ERROR, "unable to set time for local ${strDestinationTmpOp}");
        }

        # set user and/or group if required
        if (defined($strUser) || defined($strGroup))
        {
            $self->owner(PATH_ABSOLUTE, $strDestinationTmpOp, $strUser, $strGroup);
        }

        # Replace checksum in destination filename (if exists)
        if ($bAppendChecksum)
        {
            # Replace destination filename
            if ($bDestinationCompress)
            {
                $strDestinationOp =
                    substr($strDestinationOp, 0, length($strDestinationOp) - length($self->{strCompressExtension}) - 1) .
                    '-' . $strChecksum . '.' . $self->{strCompressExtension};
            }
            else
            {
                $strDestinationOp .= '-' . $strChecksum;
            }
        }

        # Move the file from tmp to final destination
        $self->move(PATH_ABSOLUTE, $strDestinationTmpOp, PATH_ABSOLUTE, $strDestinationOp, true);
    }

    return $bResult, $strChecksum, $iFileSize;
}

1;
