####################################################################################################################################
# FILE MODULE
####################################################################################################################################
package pgBackRest::File;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Exporter qw(import);
    our @EXPORT = qw();
use Fcntl qw(:mode :flock O_RDONLY O_WRONLY O_CREAT);
use File::Basename qw(dirname basename);
use File::Copy qw(cp);
use File::Path qw(make_path remove_tree);
use File::stat;
use IO::Handle;

use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::Common::Wait;
use pgBackRest::FileCommon;
use pgBackRest::Protocol::Common;
use pgBackRest::Version;

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
# RegEx constants
####################################################################################################################################
use constant REGEX_DB_VERSION                                       => '^[0-9]+\.[0-9]+';
    push @EXPORT, qw(REGEX_DB_VERSION);
use constant REGEX_ARCHIVE_DIR                                      => '^[0-F]{16}$';
    push @EXPORT, qw(REGEX_ARCHIVE_DIR);


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
        $self->{oProtocol},
        $self->{strDefaultPathMode},
        $self->{strDefaultFileMode},
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'strStanza', required => false},
            {name => 'strBackupPath'},
            {name => 'oProtocol'},
            {name => 'strDefaultPathMode', default => '0750'},
            {name => 'strDefaultFileMode', default => '0640'},
        );

    # Default compression extension to gz
    $self->{strCompressExtension} = 'gz';

    # Remote object must be set
    if (!defined($self->{oProtocol}))
    {
        confess &log(ASSERT, 'oProtocol must be defined');
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
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->DESTROY');

    if (defined($self->{oProtocol}))
    {
        $self->{oProtocol} = undef;
    }

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# stanza
####################################################################################################################################
sub stanza
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->stanza');

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
        __PACKAGE__ . '->pathTypeGet', \@_,
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
# ??? Need to tackle the return paths in this function (i.e. there are to many ways to return)
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
        __PACKAGE__ . '->pathGet', \@_,
        {name => 'strType', trace => true},
        {name => 'strFile', required => false, trace => true},
        {name => 'bTemp', default => false, trace => true}
    );

    # Is this an absolute path type?
    my $bAbsolute = $strType =~ /.*absolute.*/;

    # Make sure a temp file is valid for this type and file
    if ($bTemp)
    {
        # Only allow temp files for PATH_BACKUP_ARCHIVE, PATH_BACKUP_ARCHIVE_OUT, PATH_BACKUP_TMP and any absolute path
        if (!($strType eq PATH_BACKUP_ARCHIVE || $strType eq PATH_BACKUP_ARCHIVE_OUT || $strType eq PATH_BACKUP_TMP || $bAbsolute))
        {
            confess &log(ASSERT, 'temp file not supported for path type ' . $strType);
        }

        # The file must be defined
        if (!defined($strFile))
        {
            confess &log(ASSERT, 'strFile must be defined when temp file requested');
        }
    }

    # Get absolute path
    if ($bAbsolute)
    {
        # Make sure that any absolute path starts with /, otherwise it will actually be relative
        if ($strFile !~ /^\/.*/)
        {
            confess &log(ASSERT, "absolute path ${strType}:${strFile} must start with /");
        }

        if ($bTemp)
        {
            return
                ($strFile =~ "\.$self->{strCompressExtension}\$" ?
                    substr($strFile, 0, length($strFile) - (length($self->{strCompressExtension}) + 1)) : $strFile) .
                    '.' . BACKREST_EXE . '.tmp';
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
        # return
        #     "$self->{strBackupPath}/temp/$self->{strStanza}.tmp" . (defined($strFile) ? "/${strFile}" : '') .
        #     ($bTemp ? '.' . BACKREST_EXE . '.tmp' : '');
        return
            "$self->{strBackupPath}/temp/$self->{strStanza}.tmp" .
            # (defined($strFile) ?
            #     '/' . ($strFile =~ "\.$self->{strCompressExtension}\$" ?
            #     substr($strFile, 0, length($strFile) - (length($self->{strCompressExtension}) + 1)) : $strFile) : '') .
            ($bTemp ?
                '/' . ($strFile =~ "\.$self->{strCompressExtension}\$" ?
                substr($strFile, 0, length($strFile) - (length($self->{strCompressExtension}) + 1)) : $strFile) .
                '.' . BACKREST_EXE . '.tmp' :
                (defined($strFile) ? "/${strFile}" : ''));
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

            $strArchivePath =
                "${strArchivePath}/${strArchiveId}/" . substr($strArchive, 0, 16) .
                ($bTemp ? "/${strArchive}." . BACKREST_EXE . '.tmp' : "/${strArchiveFile}");
        }
        else
        {
            $strArchivePath =
                "${strArchivePath}/out" . (defined($strFile) ? '/' . $strFile : '') .
                ($bTemp ? '.' . BACKREST_EXE . '.tmp' : '');
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
        __PACKAGE__ . '->isRemote', \@_,
        {name => 'strPathType', trace => true}
    );

    my $bRemote = $self->{oProtocol}->isRemote() && $self->{oProtocol}->remoteTypeTest($self->pathTypeGet($strPathType));

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
            __PACKAGE__ . '->linkCreate', \@_,
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
        confess &log(ASSERT, "${strOperation}: remote operation not supported");
    }
    # Run locally
    else
    {
        # If the destination path is backup and does not exist, create it
        # ??? This should only happen when the link create errors
        if ($bPathCreate && $self->pathTypeGet($strDestinationPathType) eq PATH_BACKUP)
        {
            filePathCreate(dirname($strDestination));
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
    return logDebugReturn($strOperation);
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
            __PACKAGE__ . '->move', \@_,
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
        confess &log(ASSERT, "${strOperation}: remote operation not supported");
    }
    # Run locally
    else
    {
        fileMove($strPathOpSource, $strPathOpDestination, $bDestinationPathCreate);
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
        $strFile,
        $bRemoveSource
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->compress', \@_,
            {name => 'strPathType'},
            {name => 'strFile'},
            {name => 'bRemoveSource', default => true}
        );

    # Set operation variables
    my $strPathOp = $self->pathGet($strPathType, $strFile);

    # Run remotely
    if ($self->isRemote($strPathType))
    {
        confess &log(ASSERT, "${strOperation}: remote operation not supported");
    }
    # Run locally
    else
    {
        # Use copy to compress the file
        $self->copy($strPathType, $strFile, $strPathType, "${strFile}.gz", false, true);

        # Remove the old file
        if ($bRemoveSource)
        {
            fileRemove($strPathOp);
        }
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
        $bIgnoreExists,
        $bCreateParents
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->pathCreate', \@_,
            {name => 'strPathType'},
            {name => 'strPath', required => false},
            {name => 'strMode', default => '0750'},
            {name => 'bIgnoreExists', default => false},
            {name => 'bCreateParents', default => false}
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
        $self->{oProtocol}->cmdExecute(OP_FILE_PATH_CREATE, \%oParamHash);
    }
    else
    {
        filePathCreate($strPathOp, $strMode, $bIgnoreExists, $bCreateParents);
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
            __PACKAGE__ . '->exists', \@_,
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
        $bExists = $self->{oProtocol}->cmdExecute(OP_FILE_EXISTS, \%oParamHash, true) eq 'Y' ? true : false;
    }
    # Run locally
    else
    {
        $bExists = fileExists($strPathOp);
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
            __PACKAGE__ . '->remove', \@_,
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
        confess &log(ASSERT, "${strOperation}: remote operation not supported");
    }
    # Run locally
    else
    {
        $bRemoved = fileRemove($strPathOp, $bIgnoreMissing);
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
            __PACKAGE__ . '->hash', \@_,
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
            __PACKAGE__ . '->hashSize', \@_,
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
        confess &log(ASSERT, "${strOperation}: remote operation not supported");
    }
    else
    {
        ($strHash, $iSize) = fileHashSize($strFileOp, $bCompressed, $strHashType, $self->{oProtocol});
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
            __PACKAGE__ . '->owner', \@_,
            {name => 'strPathType'},
            {name => 'strFile'},
            {name => 'strUser'},
            {name => 'strGroup'}
        );

    # Set operation variables
    my $strFileOp = $self->pathGet($strPathType, $strFile);

    if ($self->isRemote($strPathType))
    {
        confess &log(ASSERT, "${strOperation}: remote operation not supported");
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
            __PACKAGE__ . '->list', \@_,
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
        my $strOutput = $self->{oProtocol}->cmdExecute(OP_FILE_LIST, \%oParamHash);

        if (defined($strOutput))
        {
            @stryFileList = split(/\n/, $strOutput);
        }
    }
    # Run locally
    else
    {
        @stryFileList = fileList($strPathOp, $strExpression, $strSortOrder, $bIgnoreMissing);
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
        $strPathType,
        $bWait
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->wait', \@_,
            {name => 'strPathType'},
            {name => 'bWait', default => true}
        );

    # Second when the function was called
    my $lTimeBegin;

    # Run remotely
    if ($self->isRemote($strPathType))
    {
        # Build param hash
        my %oParamHash;
        $oParamHash{wait} = $bWait;

        # Execute the command
        $lTimeBegin = $self->{oProtocol}->cmdExecute(OP_FILE_WAIT, \%oParamHash, true);
    }
    # Run locally
    else
    {
        # Wait the remainder of the current second
        $lTimeBegin = waitRemainder($bWait);
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
            __PACKAGE__ . '->manifest', \@_,
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
            __PACKAGE__ . '->manifestRecurse', \@_,
            {name => 'strPathType'},
            {name => 'strPathOp'},
            {name => 'strPathFileOp', required => false},
            {name => 'iDepth'},
            {name => 'oManifestHashRef', required => false}
        );

    # Set operation and debug strings
    my $strPathRead = $strPathOp . (defined($strPathFileOp) ? "/${strPathFileOp}" : '');
    my $hPath;
    my $strFilter;

    # If this is the top level stat the path to discover if it is actually a file
    if ($iDepth == 0 && !S_ISDIR((fileStat($strPathRead))->mode))
    {
        $strFilter = basename($strPathRead);
        $strPathRead = dirname($strPathRead);
    }

    # Open the path
    if (!opendir($hPath, $strPathRead))
    {
        my $strError = "${strPathRead} could not be read: " . $!;
        my $iErrorCode = ERROR_PATH_OPEN;

        # If the path does not exist and is not the root path requested then return, else error
        # It's OK for paths to go away during execution (databases are a dynamic thing!)
        if (!$self->exists(PATH_ABSOLUTE, $strPathRead))
        {
            if ($iDepth != 0)
            {
                return;
            }

            $strError = "${strPathRead} does not exist";
            $iErrorCode = ERROR_PATH_MISSING;
        }

        confess &log(ERROR, $strError, $iErrorCode);
    }

    # Get a list of all files in the path (except ..)
    my @stryFileList = grep(!/^\..$/i, readdir($hPath));

    close($hPath);

    # Loop through all subpaths/files in the path
    foreach my $strFile (sort(@stryFileList))
    {
        # Skip this file if it does not match the filter
        if (defined($strFilter) && $strFile ne $strFilter)
        {
            next;
        }

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
            my $iErrorCode = ERROR_FILE_READ;

            # If the file does not exist then go to the next file, else error
            # It's OK for files to go away during execution (databases are a dynamic thing!)
            if (!$self->exists(PATH_ABSOLUTE, $strPathFile))
            {
                next;
            }

            confess &log(ERROR, $strError, $iErrorCode);
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
                        exit ERROR_LINK_OPEN;
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
                exit ERROR_FILE_INVALID;
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
    return logDebugReturn($strOperation);
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
            __PACKAGE__ . '->copy', \@_,
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
            my $iErrorCode = ERROR_FILE_READ;

            if ($!{ENOENT})
            {
                # $strError = 'file is missing';
                $iErrorCode = ERROR_FILE_MISSING;

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
            }

            confess &log(ERROR, $strError, $iErrorCode);
        }
    }

    if (!$bDestinationRemote)
    {
        my $iCreateFlag = O_WRONLY | O_CREAT;

        # Open the destination temp file
        if (!sysopen($hDestinationFile, $strDestinationTmpOp, $iCreateFlag))
        {
            if ($bDestinationPathCreate)
            {
                filePathCreate(dirname($strDestinationTmpOp), undef, true, true);
            }

            if (!$bDestinationPathCreate || !sysopen($hDestinationFile, $strDestinationTmpOp, $iCreateFlag))
            {
                my $strError = "unable to open ${strDestinationTmpOp}: " . $!;
                my $iErrorCode = ERROR_FILE_READ;

                if (!fileExists(dirname($strDestinationTmpOp)))
                {
                    $strError = dirname($strDestinationTmpOp) . ' destination path does not exist';
                    $iErrorCode = ERROR_FILE_MISSING;
                }

                if (!($bDestinationPathCreate && $iErrorCode == ERROR_FILE_MISSING))
                {
                    confess &log(ERROR, $strError, $iErrorCode);
                }
            }
        }

        # Now lock the file to be sure nobody else is operating on it
        if (!flock($hDestinationFile, LOCK_EX | LOCK_NB))
        {
            confess &log(ERROR, "unable to acquire exclusive lock on ${strDestinationTmpOp}", ERROR_LOCK_ACQUIRE);
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
        my $strRemoteOp;

        # If source is remote and destination is local
        if ($bSourceRemote && !$bDestinationRemote)
        {
            $hOut = $hDestinationFile;
            $strRemoteOp = OP_FILE_COPY_OUT;
            $strRemote = 'in';

            if ($strSourcePathType ne PIPE_STDIN)
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
            $strRemoteOp = OP_FILE_COPY_IN;
            $strRemote = 'out';

            if ($strDestinationPathType ne PIPE_STDOUT)
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
            $strRemoteOp = OP_FILE_COPY;

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
            $self->{oProtocol}->cmdWrite($strRemoteOp, \%oParamHash);
        }

        # Transfer the file (skip this for copies where both sides are remote)
        if ($strRemoteOp ne OP_FILE_COPY)
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
                $strOutput = $self->{oProtocol}->outputRead(true, $bIgnoreMissingSource);

                # Check the result of the remote call
                if (substr($strOutput, 0, 1) eq 'Y')
                {
                    # If the operation was purely remote, get checksum/size
                    if ($strRemoteOp eq OP_FILE_COPY ||
                        $strRemoteOp eq OP_FILE_COPY_IN && $bSourceCompressed && !$bDestinationCompress)
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

                return true;
            }
            # If there is an error then evaluate
            or do
            {
                my $oException = $EVAL_ERROR;

                # Ignore error if source file was missing and missing file exception was returned and bIgnoreMissingSource is set
                if ($bIgnoreMissingSource && $strRemote eq 'in' && isException($oException) &&
                    $oException->code() == ERROR_FILE_MISSING)
                {
                    close($hDestinationFile)
                        or confess &log(ERROR, "cannot close file ${strDestinationTmpOp}");
                    fileRemove($strDestinationTmpOp);

                    return false, undef, undef;
                }

                confess $oException;
            };
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
        fileMove($strDestinationTmpOp, $strDestinationOp, $bDestinationPathCreate);
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
