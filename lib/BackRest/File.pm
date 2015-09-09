####################################################################################################################################
# FILE MODULE
####################################################################################################################################
package BackRest::File;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();
use Digest::SHA;
use Fcntl qw(:mode :flock O_RDONLY O_WRONLY O_CREAT O_EXCL);
use File::Basename qw(dirname basename);
use File::Copy qw(cp);
use File::Path qw(make_path remove_tree);
use File::stat;
use IO::Handle;
use Scalar::Util qw(blessed);

use lib dirname($0) . '/../lib';
use BackRest::Common::Exception;
use BackRest::Common::Log;
use BackRest::Common::String;
use BackRest::Common::Wait;
use BackRest::Config::Config;
use BackRest::FileCommon;

####################################################################################################################################
# Operation constants
####################################################################################################################################
use constant OP_FILE                                                => 'File';

use constant OP_FILE_CLONE                                          => OP_FILE . '->clone';
use constant OP_FILE_COMPRESS                                       => OP_FILE . '->compress';
use constant OP_FILE_COPY                                           => OP_FILE . '->copy';
    push @EXPORT, qw(OP_FILE_COPY);
use constant OP_FILE_COPY_IN                                        => OP_FILE . '->copyIn';
    push @EXPORT, qw(OP_FILE_COPY_IN);
use constant OP_FILE_COPY_OUT                                       => OP_FILE . '->copyOut';
    push @EXPORT, qw(OP_FILE_COPY_OUT);
use constant OP_FILE_DESTROY                                        => OP_FILE . '->DESTROY';
use constant OP_FILE_EXISTS                                         => OP_FILE . '->exists';
    push @EXPORT, qw(OP_FILE_EXISTS);
use constant OP_FILE_HASH                                           => OP_FILE . '->hash';
use constant OP_FILE_HASH_SIZE                                      => OP_FILE . '->hashSize';
use constant OP_FILE_LINK_CREATE                                    => OP_FILE . '->linkCreate';
use constant OP_FILE_LIST                                           => OP_FILE . '->list';
    push @EXPORT, qw(OP_FILE_LIST);
use constant OP_FILE_MANIFEST                                       => OP_FILE . '->manifest';
    push @EXPORT, qw(OP_FILE_MANIFEST);
use constant OP_FILE_MANIFEST_RECURSE                               => OP_FILE . '->manifestRecurse';
use constant OP_FILE_MOVE                                           => OP_FILE . '->move';
use constant OP_FILE_NEW                                            => OP_FILE . '->new';
use constant OP_FILE_OWNER                                          => OP_FILE . '->owner';
use constant OP_FILE_PATH_CREATE                                    => OP_FILE . '->pathCreate';
    push @EXPORT, qw(OP_FILE_PATH_CREATE);
use constant OP_FILE_PATH_GET                                       => OP_FILE . '->pathGet';
use constant OP_FILE_PATH_SYNC                                      => OP_FILE . '->pathSync';
use constant OP_FILE_PATH_SYNC_STATIC                               => OP_FILE . '::filePathSync';
use constant OP_FILE_PATH_TYPE_GET                                  => OP_FILE . '->pathTypeGet';
use constant OP_FILE_REMOVE                                         => OP_FILE . '->remove';
use constant OP_FILE_STANZA                                         => OP_FILE . '->stanza';
use constant OP_FILE_WAIT                                           => OP_FILE . '->wait';
    push @EXPORT, qw(OP_FILE_WAIT);

####################################################################################################################################
# COMMAND error constants [DEPRECATED - TO BE REPLACED BY CONSTANTS IN EXCEPTION.PM]
####################################################################################################################################
use constant COMMAND_ERR_FILE_MISSING           => 1;
use constant COMMAND_ERR_FILE_READ              => 2;
use constant COMMAND_ERR_FILE_MOVE              => 3;
use constant COMMAND_ERR_FILE_TYPE              => 4;
use constant COMMAND_ERR_LINK_READ              => 5;
use constant COMMAND_ERR_PATH_MISSING           => 6;
use constant COMMAND_ERR_PATH_CREATE            => 7;
use constant COMMAND_ERR_PATH_READ              => 8;

####################################################################################################################################
# PATH_GET constants
####################################################################################################################################
use constant PATH_ABSOLUTE                                          => 'absolute';
    push @EXPORT, qw(PATH_ABSOLUTE);
use constant PATH_DB                                                => 'db';
    push @EXPORT, qw(PATH_DB);
use constant PATH_DB_ABSOLUTE                                       => 'db:absolute';
    push @EXPORT, qw(PATH_DB_ABSOLUTE);
use constant PATH_BACKUP                                            => 'backup';
    push @EXPORT, qw(PATH_BACKUP);
use constant PATH_BACKUP_ABSOLUTE                                   => 'backup:absolute';
    push @EXPORT, qw(PATH_BACKUP_ABSOLUTE);
use constant PATH_BACKUP_CLUSTER                                    => 'backup:cluster';
    push @EXPORT, qw(PATH_BACKUP_CLUSTER);
use constant PATH_BACKUP_TMP                                        => 'backup:tmp';
    push @EXPORT, qw(PATH_BACKUP_TMP);
use constant PATH_BACKUP_ARCHIVE                                    => 'backup:archive';
    push @EXPORT, qw(PATH_BACKUP_ARCHIVE);
use constant PATH_BACKUP_ARCHIVE_OUT                                => 'backup:archive:out';
    push @EXPORT, qw(PATH_BACKUP_ARCHIVE_OUT);

####################################################################################################################################
# STD pipe constants
####################################################################################################################################
use constant PIPE_STDIN                                             => '<STDIN>';
    push @EXPORT, qw(PIPE_STDIN);
use constant PIPE_STDOUT                                            => '<STDOUT>';
    push @EXPORT, qw(PIPE_STDOUT);
use constant PIPE_STDERR                                            => '<STDERR>';
    push @EXPORT, qw(PIPE_STDERR);

####################################################################################################################################
# new
####################################################################################################################################
sub new
{
    my $class = shift;

    # Create the class hash
    my $self = {};
    bless $self, $class;

    # Assign function parameters, defaults, and log debug info
    (
        my $strOperation,
        $self->{strStanza},
        $self->{strBackupPath},
        $self->{strRemote},
        $self->{oProtocol},
        $self->{strDefaultPathMode},
        $self->{strDefaultFileMode},
        $self->{iThreadIdx}
    ) =
        logDebugParam
        (
            OP_FILE_NEW, \@_,
            {name => 'strStanza', required => false},
            {name => 'strBackupPath'},
            {name => 'strRemote', required => false},
            {name => 'oProtocol'},
            {name => 'strDefaultPathMode', default => '0750'},
            {name => 'strDefaultFileMode', default => '0640'},
            {name => 'iThreadIdx', required => false}
        );

    # Default compression extension to gz
    $self->{strCompressExtension} = 'gz';

    # Remote object must be set
    if (!defined($self->{oProtocol}))
    {
        confess &log(ASSERT, 'oProtocol must be defined');
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

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# DESTROY
####################################################################################################################################
sub DESTROY
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation
    ) =
        logDebugParam
    (
        OP_FILE_DESTROY
    );

    if (defined($self->{oProtocol}))
    {
        $self->{oProtocol} = undef;
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation
    );
}

####################################################################################################################################
# clone
####################################################################################################################################
sub clone
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $iThreadIdx
    ) =
        logDebugParam
    (
        OP_FILE_CLONE, \@_,
        {name => 'iThreadidx', required => false}
    );

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => BackRest::File->new
                                  (
                                      $self->{strStanza},
                                      $self->{strBackupPath},
                                      $self->{strRemote},
                                      $self->{oProtocol},
                                      $self->{strDefaultPathMode},
                                      $self->{strDefaultFileMode},
                                      $iThreadIdx
                                  )}
    );
}

####################################################################################################################################
# stanza
####################################################################################################################################
sub stanza
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation
    ) =
        logDebugParam
    (
        OP_FILE_STANZA
    );

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strStanza', $self->{strStanza}, trace => true}
    );
}

####################################################################################################################################
# pathTypeGet
####################################################################################################################################
sub pathTypeGet
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strType
    ) =
        logDebugParam
    (
        OP_FILE_PATH_TYPE_GET, \@_,
        {name => 'strType', trace => true}
    );

    my $strPath;

    # If absolute type
    if ($strType eq PATH_ABSOLUTE)
    {
        $strPath = PATH_ABSOLUTE;
    }
    # If db type
    elsif ($strType =~ /^db(\:.*){0,1}/)
    {
        $strPath = PATH_DB;
    }
    # Else if backup type
    elsif ($strType =~ /^backup(\:.*){0,1}/)
    {
        $strPath = PATH_BACKUP;
    }
    # Else error when path type not recognized
    else
    {
        confess &log(ASSERT, "no known path types in '${strType}'");
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strPath', value => $strPath, trace => true}
    );
}

####################################################################################################################################
# pathGet
# !!! Need ot tackle the return paths in this function
####################################################################################################################################
sub pathGet
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strType,                                   # Base type of the path to get (PATH_DB_ABSOLUTE, PATH_BACKUP_TMP, etc)
        $strFile,                                   # File to append to the base path (can include a path as well)
        $bTemp                                      # Return the temp file for this path type - only some types have temp files
    ) =
        logDebugParam
    (
        OP_FILE_PATH_GET, \@_,
        {name => 'strType', trace => true},
        {name => 'strFile', required => false, trace => true},
        {name => 'bTemp', default => false, trace => true}
    );

    # Make sure that any absolute path starts with /, otherwise it will actually be relative
    my $bAbsolute = $strType =~ /.*absolute.*/;

    if ($bAbsolute && $strFile !~ /^\/.*/)
    {
        confess &log(ASSERT, "absolute path ${strType}:${strFile} must start with /");
    }

    # Only allow temp files for PATH_BACKUP_ARCHIVE, PATH_BACKUP_ARCHIVE_OUT, PATH_BACKUP_TMP and any absolute path
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

        if (!defined($strFile))
        {
            return $strArchivePath;
        }

        if ($strType eq PATH_BACKUP_ARCHIVE)
        {
            my $strArchiveId = (split('/', $strFile))[0];
            my $strArchiveFile = (split('/', $strFile))[1];

            if (!defined($strArchiveFile))
            {
                return "${strArchivePath}/${strFile}";
            }

            my $strArchive = substr(basename($strArchiveFile), 0, 24);

            if ($strArchive !~ /^([0-F]){24}$/)
            {
                return "${strArchivePath}/${strFile}";
            }

            $strArchivePath = "${strArchivePath}/${strArchiveId}/" . substr($strArchive, 0, 16) . "/${strArchiveFile}";
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

            $strArchivePath = "${strArchivePath}.tmp";
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
# isRemote
#
# Determine whether the path type is remote
####################################################################################################################################
sub isRemote
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPathType
    ) =
        logDebugParam
    (
        OP_FILE_CLONE, \@_,
        {name => 'strPathType', trace => true}
    );

    my $bRemote = defined($self->{strRemote}) && $self->pathTypeGet($strPathType) eq $self->{strRemote};

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'bRemote', value => $bRemote, trace => true}
    );
}

####################################################################################################################################
# linkCreate
####################################################################################################################################
sub linkCreate
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strSourcePathType,
        $strSourceFile,
        $strDestinationPathType,
        $strDestinationFile,
        $bHard,
        $bRelative,
        $bPathCreate
    ) =
        logDebugParam
        (
            OP_FILE_LINK_CREATE, \@_,
            {name => 'strSourcePathType'},
            {name => 'strSourceFile'},
            {name => 'strDestinationPathType'},
            {name => 'strDestinationFile'},
            {name => 'bHard', default => false},
            {name => 'bRelative', default => false},
            {name => 'bPathCreate', default => true}
        );

    # Source and destination path types must be the same (e.g. both PATH_DB or both PATH_BACKUP, etc.)
    if ($self->pathTypeGet($strSourcePathType) ne $self->pathTypeGet($strDestinationPathType))
    {
        confess &log(ASSERT, 'path types must be equal in link create');
    }

    # Generate source and destination files
    my $strSource = $self->pathGet($strSourcePathType, $strSourceFile);
    my $strDestination = $self->pathGet($strDestinationPathType, $strDestinationFile);

    # Run remotely
    if ($self->isRemote($strSourcePathType))
    {
        confess &log(ASSERT, 'remote operation not supported');
    }
    # Run locally
    else
    {
        # If the destination path is backup and does not exist, create it
        # !!! This should only happen when the link create errors
        if ($bPathCreate && $self->pathTypeGet($strDestinationPathType) eq PATH_BACKUP)
        {
            $self->pathCreate(PATH_BACKUP_ABSOLUTE, dirname($strDestination));
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
            my $iCommonLen = commonPrefix($strSource, $strDestination);

            if ($iCommonLen != 0)
            {
                $strSource = ('../' x substr($strDestination, $iCommonLen) =~ tr/\///) . substr($strSource, $iCommonLen);
            }

            logDebugMisc
            (
                $strOperation, 'apply relative path',
                {name => 'strSource', value => $strSource, trace => true}
            );
        }

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

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation
    );
}

####################################################################################################################################
# pathSync
#
# Sync a directory.
####################################################################################################################################
sub pathSync
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPathType,
        $strPath,
    ) =
        logDebugParam
        (
            OP_FILE_PATH_SYNC, \@_,
            {name => 'strPathType', trace => true},
            {name => 'strPath', trace => true}
        );

    filePathSync($self->pathGet($strPathType, $strPath eq '.' ? undef : $strPath));

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation
    );
}

####################################################################################################################################
# move
#
# Moves a file locally or remotely.
####################################################################################################################################
sub move
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strSourcePathType,
        $strSourceFile,
        $strDestinationPathType,
        $strDestinationFile,
        $bDestinationPathCreate
    ) =
        logDebugParam
        (
            OP_FILE_MOVE, \@_,
            {name => 'strSourcePathType'},
            {name => 'strSourceFile', required => false},
            {name => 'strDestinationPathType'},
            {name => 'strDestinationFile'},
            {name => 'bDestinationPathCreate', default => false}
        );

    # Source and destination path types must be the same
    if ($self->pathTypeGet($strSourcePathType) ne $self->pathTypeGet($strSourcePathType))
    {
        confess &log(ASSERT, 'source and destination path types must be equal');
    }

    # Set operation variables
    my $strPathOpSource = $self->pathGet($strSourcePathType, $strSourceFile);
    my $strPathOpDestination = $self->pathGet($strDestinationPathType, $strDestinationFile);

    # Run remotely
    if ($self->isRemote($strSourcePathType))
    {
        confess &log(ASSERT, 'remote operation not supported');
    }
    # Run locally
    else
    {
        if (!rename($strPathOpSource, $strPathOpDestination))
        {
            if ($bDestinationPathCreate)
            {
                $self->pathCreate(PATH_ABSOLUTE, dirname($strPathOpDestination), undef, true);
            }

            if (!$bDestinationPathCreate || !rename($strPathOpSource, $strPathOpDestination))
            {
                my $strError = "unable to move file ${strPathOpSource} to ${strPathOpDestination}: " . $!;
                my $iErrorCode = COMMAND_ERR_FILE_READ;

                if (!$self->exists(PATH_ABSOLUTE, dirname($strPathOpDestination)))
                {
                    $strError = dirname($strPathOpDestination) . " destination path does not exist";
                    $iErrorCode = COMMAND_ERR_FILE_MISSING;
                }

                if (!($bDestinationPathCreate && $iErrorCode == COMMAND_ERR_FILE_MISSING))
                {
                    if ($strSourcePathType eq PATH_ABSOLUTE)
                    {
                        confess &log(ERROR, $strError, $iErrorCode);
                    }

                    confess &log(ERROR, $strError);
                }
            }
        }

        $self->pathSync($strDestinationPathType, dirname($strDestinationFile));
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation
    );
}

####################################################################################################################################
# compress
####################################################################################################################################
sub compress
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPathType,
        $strFile
    ) =
        logDebugParam
        (
            OP_FILE_COMPRESS, \@_,
            {name => 'strPathType'},
            {name => 'strFile'}
        );

    # Set operation variables
    my $strPathOp = $self->pathGet($strPathType, $strFile);

    # Run remotely
    if ($self->isRemote($strPathType))
    {
        confess &log(ASSERT, 'remote operation not supported');
    }
    # Run locally
    else
    {
        # Use copy to compress the file
        $self->copy($strPathType, $strFile, $strPathType, "${strFile}.gz", false, true);

        # Remove the old file
        unlink($strPathOp)
            or die &log(ERROR, "unable to remove ${strPathOp}");
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation
    );
}

####################################################################################################################################
# pathCreate
#
# Creates a path locally or remotely.
####################################################################################################################################
sub pathCreate
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPathType,
        $strPath,
        $strMode,
        $bIgnoreExists
    ) =
        logDebugParam
        (
            OP_FILE_PATH_CREATE, \@_,
            {name => 'strPathType'},
            {name => 'strPath', required => false},
            {name => 'strMode', default => '0750'},
            {name => 'bIgnoreExists', default => false}
        );

    # Set operation variables
    my $strPathOp = $self->pathGet($strPathType, $strPath);

    if ($self->isRemote($strPathType))
    {
        # Build param hash
        my %oParamHash;

        $oParamHash{path} = ${strPathOp};

        if (defined($strMode))
        {
            $oParamHash{mode} = ${strMode};
        }

        # Execute the command
        $self->{oProtocol}->cmdExecute(OP_FILE_PATH_CREATE, \%oParamHash, false);
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
                confess &log(ERROR, $strError); #, COMMAND_ERR_PATH_CREATE);
            }
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation
    );
}

####################################################################################################################################
# exists
#
# Checks for the existence of a file, but does not imply that the file is readable/writeable.
#
# Return: true if file exists, false otherwise
####################################################################################################################################
sub exists
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPathType,
        $strPath
    ) =
        logDebugParam
        (
            OP_FILE_EXISTS, \@_,
            {name => 'strPathType'},
            {name => 'strPath', required => false}
        );

    # Set operation variables
    my $strPathOp = $self->pathGet($strPathType, $strPath);
    my $bExists = true;

    # Run remotely
    if ($self->isRemote($strPathType))
    {
        # Build param hash
        my %oParamHash;

        $oParamHash{path} = $strPathOp;

        # Execute the command
        $bExists = $self->{oProtocol}->cmdExecute($strOperation, \%oParamHash, true) eq 'Y' ? true : false;
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
                    confess &log(ERROR, $!); #, COMMAND_ERR_FILE_READ);
                }
            }

            $bExists = false;
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'bExists', value => $bExists}
    );
}

####################################################################################################################################
# remove
####################################################################################################################################
sub remove
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPathType,
        $strPath,
        $bTemp,
        $bIgnoreMissing
    ) =
        logDebugParam
        (
            OP_FILE_REMOVE, \@_,
            {name => 'strPathType'},
            {name => 'strPath'},
            {name => 'bTemp', required => false},
            {name => 'bIgnoreMissing', default => true}
        );

    # Set operation variables
    my $strPathOp = $self->pathGet($strPathType, $strPath, $bTemp);
    my $bRemoved = true;

    # Run remotely
    if ($self->isRemote($strPathType))
    {
        confess &log(ASSERT, OP_FILE_REMOVE . ": remote operation not supported");
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

                confess &log(ERROR, $strError);
            }
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'bRemoved', value => $bRemoved}
    );
}

####################################################################################################################################
# hash
####################################################################################################################################
sub hash
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPathType,
        $strFile,
        $bCompressed,
        $strHashType
    ) =
        logDebugParam
        (
            OP_FILE_HASH, \@_,
            {name => 'strPathType'},
            {name => 'strFile'},
            {name => 'bCompressed', required => false},
            {name => 'strHashType', required => false}
        );

    my ($strHash) = $self->hashSize($strPathType, $strFile, $bCompressed, $strHashType);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strHash', value => $strHash, trace => true}
    );
}

####################################################################################################################################
# hashSize
####################################################################################################################################
sub hashSize
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPathType,
        $strFile,
        $bCompressed,
        $strHashType
    ) =
        logDebugParam
        (
            OP_FILE_HASH_SIZE, \@_,
            {name => 'strPathType'},
            {name => 'strFile'},
            {name => 'bCompressed', default => false},
            {name => 'strHashType', default => 'sha1'}
        );

    # Set operation variables
    my $strFileOp = $self->pathGet($strPathType, $strFile);
    my $strHash;
    my $iSize = 0;

    if ($self->isRemote($strPathType))
    {
        confess &log(ASSERT, OP_FILE_HASH_SIZE . ": remote operation not supported");
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

            confess &log(ERROR, $strError);
        }

        my $oSHA = Digest::SHA->new($strHashType);

        if ($bCompressed)
        {
            ($strHash, $iSize) =
                $self->{oProtocol}->binaryXfer($hFile, 'none', 'in', true, false, false);
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

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strHash', value => $strHash},
        {name => 'iSize', value => $iSize}
    );
}

####################################################################################################################################
# owner
####################################################################################################################################
sub owner
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPathType,
        $strFile,
        $strUser,
        $strGroup
    ) =
        logDebugParam
        (
            OP_FILE_OWNER, \@_,
            {name => 'strPathType'},
            {name => 'strFile'},
            {name => 'strUser'},
            {name => 'strGroup'}
        );

    # Set operation variables
    my $strFileOp = $self->pathGet($strPathType, $strFile);

    if ($self->isRemote($strPathType))
    {
        confess &log(ASSERT, OP_FILE_OWNER . ": remote operation not supported");
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

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation
    );
}

####################################################################################################################################
# list
####################################################################################################################################
sub list
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPathType,
        $strPath,
        $strExpression,
        $strSortOrder,
        $bIgnoreMissing
    ) =
        logDebugParam
        (
            OP_FILE_LIST, \@_,
            {name => 'strPathType'},
            {name => 'strPath', required => false},
            {name => 'strExpression', required => false},
            {name => 'strSortOrder', default => 'forward'},
            {name => 'bIgnoreMissing', default => false}
        );

    # Set operation variables
    my $strPathOp = $self->pathGet($strPathType, $strPath);
    my @stryFileList;

    # Run remotely
    if ($self->isRemote($strPathType))
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

        # Execute the command
        my $strOutput = $self->{oProtocol}->cmdExecute(OP_FILE_LIST, \%oParamHash, false);

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

            confess &log(ERROR, $strError);
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

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'stryFileList', value => \@stryFileList}
    );
}

####################################################################################################################################
# wait
#
# Wait until the next second.  This is done in the file object because it must be performed on whichever side the db is on, local or
# remote.  This function is used to make sure that no files are copied in the same second as the manifest is created.  The reason is
# that the db might modify they file again in the same second as the copy and that change will not be visible to a subsequent
# incremental backup using timestamp/size to determine deltas.
####################################################################################################################################
sub wait
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPathType
    ) =
        logDebugParam
        (
            OP_FILE_WAIT, \@_,
            {name => 'strPathType'}
        );

    # Second when the function was called
    my $lTimeBegin;

    # Run remotely
    if ($self->isRemote($strPathType))
    {
        # Execute the command
        $lTimeBegin = $self->{oProtocol}->cmdExecute(OP_FILE_WAIT, undef, true);
    }
    # Run locally
    else
    {
        # Wait the remainder of the current second
        $lTimeBegin = waitRemainder();
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'lTimeBegin', value => $lTimeBegin, trace => true}
    );
}

####################################################################################################################################
# manifest
#
# Builds a path/file manifest starting with the base path and including all subpaths.  The manifest contains all the information
# needed to perform a backup or a delta with a previous backup.
####################################################################################################################################
sub manifest
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPathType,
        $strPath,
        $oManifestHashRef
    ) =
        logDebugParam
        (
            OP_FILE_MANIFEST, \@_,
            {name => 'strPathType'},
            {name => 'strPath', required => false},
            {name => 'oManifestHashRef'}
        );

    # Set operation variables
    my $strPathOp = $self->pathGet($strPathType, $strPath);

    # Run remotely
    if ($self->isRemote($strPathType))
    {
        # Build param hash
        my %oParamHash;

        $oParamHash{path} = $strPathOp;

        # Execute the command
        dataHashBuild($oManifestHashRef, $self->{oProtocol}->cmdExecute(OP_FILE_MANIFEST, \%oParamHash, true), "\t");
    }
    # Run locally
    else
    {
        $self->manifestRecurse($strPathType, $strPathOp, undef, 0, $oManifestHashRef);
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation
    );
}

sub manifestRecurse
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPathType,
        $strPathOp,
        $strPathFileOp,
        $iDepth,
        $oManifestHashRef
    ) =
        logDebugParam
        (
            OP_FILE_MANIFEST_RECURSE, \@_,
            {name => 'strPathType'},
            {name => 'strPathOp'},
            {name => 'strPathFileOp', required => false},
            {name => 'iDepth'},
            {name => 'oManifestHashRef', required => false}
        );

    # Set operation and debug strings
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

        confess &log(ERROR, $strError);
    }

    # Get a list of all files in the path (except ..)
    my @stryFileList = grep(!/^\..$/i, readdir($hPath));

    close($hPath);

    # Loop through all subpaths/files in the path
    foreach my $strFile (sort(@stryFileList))
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

            confess &log(ERROR, $strError);
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

                    confess &log(ERROR, $strError);
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

            confess &log(ERROR, $strError);
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
            $self->manifestRecurse($strPathType, $strPathOp, $strFile, $iDepth + 1, $oManifestHashRef);
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation
    );
}

####################################################################################################################################
# copy
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

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strSourcePathType,
        $strSourceFile,
        $strDestinationPathType,
        $strDestinationFile,
        $bSourceCompressed,
        $bDestinationCompress,
        $bIgnoreMissingSource,
        $lModificationTime,
        $strMode,
        $bDestinationPathCreate,
        $strUser,
        $strGroup,
        $bAppendChecksum
    ) =
        logDebugParam
        (
            OP_FILE_COPY, \@_,
            {name => 'strSourcePathType'},
            {name => 'strSourceFile', required => false},
            {name => 'strDestinationPathType'},
            {name => 'strDestinationFile', required => false},
            {name => 'bSourceCompressed', default => false},
            {name => 'bDestinationCompress', default => false},
            {name => 'bIgnoreMissingSource', default => false},
            {name => 'lModificationTime', required => false},
            {name => 'strMode', default => '0640'},
            {name => 'bDestinationPathCreate', default => false},
            {name => 'strUser', required => false},
            {name => 'strGroup', required => false},
            {name => 'bAppendChecksum', default => false}
        );

    # Set working variables
    my $bSourceRemote = $self->isRemote($strSourcePathType) || $strSourcePathType eq PIPE_STDIN;
    my $bDestinationRemote = $self->isRemote($strDestinationPathType) || $strDestinationPathType eq PIPE_STDOUT;
    my $strSourceOp = $strSourcePathType eq PIPE_STDIN ?
        $strSourcePathType : $self->pathGet($strSourcePathType, $strSourceFile);
    my $strDestinationOp = $strDestinationPathType eq PIPE_STDOUT ?
        $strDestinationPathType : $self->pathGet($strDestinationPathType, $strDestinationFile);
    my $strDestinationTmpOp = $strDestinationPathType eq PIPE_STDOUT ?
        undef : $self->pathGet($strDestinationPathType, $strDestinationFile, true);

    # Checksum and size variables
    my $strChecksum = undef;
    my $iFileSize = undef;
    my $bResult = true;

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
                    $self->{oProtocol}->binaryXferAbort();
                }

                confess &log(ERROR, $strError, $iErrorCode);
            }

            confess &log(ERROR, $strError, $iErrorCode);
        }
    }

    if (!$bDestinationRemote)
    {
        my $iCreateFlag = O_WRONLY | O_CREAT | O_EXCL;

        # Open the destination temp file
        if (!sysopen($hDestinationFile, $strDestinationTmpOp, $iCreateFlag))
        {
            if ($bDestinationPathCreate)
            {
                $self->pathCreate(PATH_ABSOLUTE, dirname($strDestinationTmpOp), undef, true);
            }

            if (!$bDestinationPathCreate || !sysopen($hDestinationFile, $strDestinationTmpOp, $iCreateFlag))
            {
                my $strError = "unable to open ${strDestinationTmpOp}: " . $!;
                my $iErrorCode = COMMAND_ERR_FILE_READ;

                if (!$self->exists(PATH_ABSOLUTE, dirname($strDestinationTmpOp)))
                {
                    $strError = dirname($strDestinationTmpOp) . ' destination path does not exist';
                    $iErrorCode = COMMAND_ERR_FILE_MISSING;
                }

                if (!($bDestinationPathCreate && $iErrorCode == COMMAND_ERR_FILE_MISSING))
                {
                    if ($strSourcePathType eq PATH_ABSOLUTE)
                    {
                        confess &log(ERROR, $strError, $iErrorCode);
                    }

                    confess &log(ERROR, $strError);
                }
            }
        }

        # Now lock the file to be sure nobody else is operating on it
        if (!flock($hDestinationFile, LOCK_EX | LOCK_NB))
        {
            confess &log(ERROR, "unable to acquire exclusive lock on lock ${strDestinationTmpOp}", ERROR_LOCK_ACQUIRE);
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

        # If an operation is defined then write it
        if (%oParamHash)
        {
            $self->{oProtocol}->cmdWrite($strOperation, \%oParamHash);
        }

        # Transfer the file (skip this for copies where both sides are remote)
        if ($strOperation ne OP_FILE_COPY)
        {
            ($strChecksum, $iFileSize) =
                $self->{oProtocol}->binaryXfer($hIn, $hOut, $strRemote, $bSourceCompressed, $bDestinationCompress);
        }

        # If this is the controlling process then wait for OK from remote
        if (%oParamHash)
        {
            # Test for an error when reading output
            my $strOutput;

            eval
            {
                $strOutput = $self->{oProtocol}->outputRead(true, undef, true);

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
                if ($bIgnoreMissingSource && $strRemote eq 'in' && blessed($oMessage) && $oMessage->isa('BackRest::Common::Exception') &&
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
                $self->{oProtocol}->binaryXfer($hSourceFile, $hDestinationFile, 'out', false, true, false);
        }
        # If the source is compressed and the destination is not then decompress
        elsif ($bSourceCompressed && !$bDestinationCompress)
        {
            ($strChecksum, $iFileSize) =
                $self->{oProtocol}->binaryXfer($hSourceFile, $hDestinationFile, 'in', true, false, false);
        }
        # Else both side are compressed, so copy capturing checksum
        elsif ($bSourceCompressed)
        {
            ($strChecksum, $iFileSize) =
                $self->{oProtocol}->binaryXfer($hSourceFile, $hDestinationFile, 'out', true, true, false);
        }
        else
        {
            ($strChecksum, $iFileSize) =
                $self->{oProtocol}->binaryXfer($hSourceFile, $hDestinationFile, 'in', false, true, false);
        }
    }

    # Close the source file (if local)
    if (defined($hSourceFile))
    {
        close($hSourceFile) or confess &log(ERROR, "cannot close file ${strSourceOp}");
    }

    # Sync and close the destination file (if local)
    if (defined($hDestinationFile))
    {
        $hDestinationFile->sync()
            or confess &log(ERROR, "unable to sync ${strDestinationTmpOp}", ERROR_FILE_SYNC);

        close($hDestinationFile)
            or confess &log(ERROR, "cannot close file ${strDestinationTmpOp}");
    }

    # Checksum and file size should be set if the destination is not remote
    if ($bResult &&
        !(!$bSourceRemote && $bDestinationRemote && $bSourceCompressed) &&
        (!defined($strChecksum) || !defined($iFileSize)))
    {
        confess &log(ASSERT, 'checksum or file size not set');
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
        $self->move(PATH_ABSOLUTE, $strDestinationTmpOp, PATH_ABSOLUTE, $strDestinationOp, $bDestinationPathCreate);
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'bResult', value => $bResult, trace => true},
        {name => 'strChecksum', value => $strChecksum, trace => true},
        {name => 'iFileSize', value => $iFileSize, trace => true}
    );
}

1;
