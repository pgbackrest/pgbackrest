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
        $self->{strRepoPath},
        $self->{oProtocol},
        $self->{strDefaultPathMode},
        $self->{strDefaultFileMode},
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'strStanza', required => false},
            {name => 'strRepoPath'},
            {name => 'oProtocol'},
            {name => 'strDefaultPathMode', default => '0750'},
            {name => 'strDefaultFileMode', default => '0640'},
        );

    # Default compression extension to gz
    $self->{strCompressExtension} = 'gz';

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
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
    # else error when path type not recognized
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

    # Path to be returned
    my $strPath;

    # Is this an absolute path type?
    my $bAbsolute = $strType =~ /.*absolute.*/;

    # Make sure a temp file is valid for this type and file
    if ($bTemp)
    {
        # Only allow temp files for PATH_BACKUP_ARCHIVE, PATH_BACKUP_ARCHIVE_OUT, PATH_BACKUP_TMP and any absolute path
        if (!($strType eq PATH_BACKUP_ARCHIVE || $strType eq PATH_BACKUP_ARCHIVE_OUT || $strType eq PATH_BACKUP_TMP || $bAbsolute))
        {
            confess &log(ASSERT, "temp file not supported for path type '${strType}'");
        }

        # The file must be defined
        if (!defined($strFile))
        {
            confess &log(ASSERT, 'strFile must be defined when temp file specified');
        }
    }

    # Get absolute path
    if ($bAbsolute)
    {
        # File must defined when the path is absolute since in effect there is no path
        if (!defined($strFile))
        {
            confess &log(ASSERT, 'strFile must be defined for absolute path');
        }

        # Make sure that the file starts with /, otherwise it will actually be relative
        if ($strFile !~ /^\/.*/)
        {
            confess &log(ASSERT, "absolute path ${strType}:${strFile} must start with /");
        }
    }
    # Else get backup path
    elsif ($strType eq PATH_BACKUP)
    {
        $strPath = $self->{strRepoPath};
    }
    # Else process path types that require a stanza
    else
    {
        # All paths in this section will in the repo path
        $strPath = $self->{strRepoPath};

        # Make sure the stanza is defined since remaining path types require it
        if (!defined($self->{strStanza}))
        {
            confess &log(ASSERT, 'strStanza not defined');
        }

        # Get the backup tmp path
        if ($strType eq PATH_BACKUP_TMP)
        {
            $strPath .= "/temp/$self->{strStanza}.tmp";
        }
        # Else get archive paths
        elsif ($strType eq PATH_BACKUP_ARCHIVE_OUT || $strType eq PATH_BACKUP_ARCHIVE)
        {
            $strPath .= "/archive/$self->{strStanza}";

            # Get archive path
            if ($strType eq PATH_BACKUP_ARCHIVE)
            {
                # If file is not defined nothing further to do
                if (defined($strFile))
                {
                    my $strArchiveId = (split('/', $strFile))[0];

                    # If file is defined after archive id path is split out
                    if (defined((split('/', $strFile))[1]))
                    {
                        $strPath .= "/${strArchiveId}";
                        $strFile = (split('/', $strFile))[1];

                        # If this is a WAL segment then put it into a subdirectory
                        if (substr(basename($strFile), 0, 24) =~ /^([0-F]){24}$/)
                        {
                            $strPath .= '/' . substr($strFile, 0, 16);
                        }
                    }
                }
            }
            # Else get archive out path
            else
            {
                $strPath .= '/out';
            }
        }
        # Else get backup cluster
        elsif ($strType eq PATH_BACKUP_CLUSTER)
        {
            $strPath .= "/backup/$self->{strStanza}";
        }
        # Else error when path type not recognized
        else
        {
            confess &log(ASSERT, "no known path types in '${strType}'");
        }
    }

    # Combine path and file
    $strPath .= (defined($strFile) ? (defined($strPath) ? '/' : '') . $strFile : '');

    # Add temp extension
    $strPath .= $bTemp ? '.' . BACKREST_EXE . '.tmp' : '';

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strPath', value => $strPath, trace => true}
    );
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
            filePathCreate(dirname($strDestination), undef, true);
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
            # Determine how much of the paths are common
            my @strySource = split('/', $strSource);
            my @stryDestination = split('/', $strDestination);

            while (defined($strySource[0]) && defined($stryDestination[0]) && $strySource[0] eq $stryDestination[0])
            {
                shift(@strySource);
                shift(@stryDestination);
            }

            # Add relative path sections
            $strSource = '';

            for (my $iIndex = 0; $iIndex < @stryDestination - 1; $iIndex++)
            {
                $strSource .= '../';
            }

            # Add path to source
            $strSource .= join('/', @strySource);

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
        $bDestinationPathCreate,
        $bPathSync,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->move', \@_,
            {name => 'strSourcePathType'},
            {name => 'strSourceFile', required => false},
            {name => 'strDestinationPathType'},
            {name => 'strDestinationFile'},
            {name => 'bDestinationPathCreate', default => false},
            {name => 'bPathSync', default => false},
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
        fileMove($strPathOpSource, $strPathOpDestination, $bDestinationPathCreate, $bPathSync);
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

    # Run remotely
    if ($self->isRemote($strPathType))
    {
        $self->{oProtocol}->cmdExecute(OP_FILE_PATH_CREATE, [$strPathOp, $strMode, $bIgnoreExists, $bCreateParents]);
    }
    # Run locally
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
# pathSync
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
        $bRecursive,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->pathSync', \@_,
            {name => 'strPathType'},
            {name => 'strPath', required => false},
            {name => 'bRecursive', default => false},
        );

    # Remote not implemented
    if ($self->isRemote($strPathType))
    {
        confess &log(ASSERT, "${strOperation}: remote operation not supported");
    }

    # Sync all paths in the tree
    if ($bRecursive)
    {
        my $oManifest = $self->manifest($strPathType, $strPath);

        # Iterate all files in the manifest
        foreach my $strFile (sort(keys(%{$oManifest})))
        {
            # Only sync if this is a directory
            if ($oManifest->{$strFile}{type} eq 'd')
            {
                # If current directory
                if ($strFile eq '.')
                {
                    $self->pathSync($strPathType, $strPath);
                }
                # Else a subdirectory
                else
                {
                    $self->pathSync($strPathType, (defined($strPath) ? "${strPath}/" : '') . $strFile);
                }
            }
        }
    }
    # Only sync the specified path
    else
    {
        filePathSync($self->pathGet($strPathType, $strPath));
    }

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
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
    my $bExists;

    # Run remotely
    if ($self->isRemote($strPathType))
    {
        $bExists = $self->{oProtocol}->cmdExecute(OP_FILE_EXISTS, [$strPathOp], true);
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
        $bIgnoreMissing,
        $bPathSync,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->remove', \@_,
            {name => 'strPathType'},
            {name => 'strPath'},
            {name => 'bTemp', required => false},
            {name => 'bIgnoreMissing', default => true},
            {name => 'bPathSync', default => false},
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
        $bRemoved = fileRemove($strPathOp, $bIgnoreMissing, $bPathSync);
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
            {name => 'strUser', required => false},
            {name => 'strGroup', required => false}
        );

    # Set operation variables
    my $strFileOp = $self->pathGet($strPathType, $strFile);

    # Run remotely
    if ($self->isRemote($strPathType))
    {
        confess &log(ASSERT, "${strOperation}: remote operation not supported");
    }
    # Run locally
    else
    {
        my $iUserId;
        my $iGroupId;

        # If the user or group is not defined then get it by stat'ing the file.  This is because the chown function requires that
        # both user and group be set.
        if (!(defined($strUser) && defined($strGroup)))
        {
            my $oStat = fileStat($strFileOp);

            if (!defined($strUser))
            {
                $iUserId = $oStat->uid;
            }

            if (!defined($strGroup))
            {
                $iGroupId = $oStat->gid;
            }
        }

        # Lookup user if specified
        if (defined($strUser))
        {
            $iUserId = getpwnam($strUser);

            if (!defined($iUserId))
            {
                confess &log(ERROR, "user '${strUser}' does not exist", ERROR_USER_MISSING);
            }
        }

        # Lookup group if specified
        if (defined($strGroup))
        {
            $iGroupId = getgrnam($strGroup);

            if (!defined($iGroupId))
            {
                confess &log(ERROR, "group '${strGroup}' does not exist", ERROR_GROUP_MISSING);
            }
        }

        # Set ownership on the file
        if (!chown($iUserId, $iGroupId, $strFileOp))
        {
            my $strError = $!;

            if (fileExists($strFileOp))
            {
                confess &log(ERROR,
                    "unable to set ownership for '${strFileOp}'" . (defined($strError) ? ": $strError" : ''), ERROR_FILE_OWNER);
            }

            confess &log(ERROR, "${strFile} does not exist", ERROR_FILE_MISSING);
        }
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
        @stryFileList = $self->{oProtocol}->cmdExecute(
            OP_FILE_LIST, [$strPathOp, $strExpression, $strSortOrder, $bIgnoreMissing]);
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
        $lTimeBegin = $self->{oProtocol}->cmdExecute(OP_FILE_WAIT, [$bWait], true);
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
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->manifest', \@_,
            {name => 'strPathType'},
            {name => 'strPath', required => false},
        );

    # Set operation variables
    my $strPathOp = $self->pathGet($strPathType, $strPath);
    my $hManifest;

    # Run remotely
    if ($self->isRemote($strPathType))
    {
        $hManifest = $self->{oProtocol}->cmdExecute(OP_FILE_MANIFEST, [$strPathOp], true);
    }
    # Run locally
    else
    {
        $hManifest = fileManifest($strPathOp);
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'hManifest', value => $hManifest, trace => true}
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
        $bAppendChecksum,
        $bPathSync,
        $strExtraFunction,
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
            {name => 'bAppendChecksum', default => false},
            {name => 'bPathSync', default => false},
            {name => 'strExtraFunction', required => false},
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
    my $fnExtra = defined($strExtraFunction) ?
        eval("\\&${strExtraFunction}") : undef;                     ## no critic (BuiltinFunctions::ProhibitStringyEval)

    # Checksum and size variables
    my $strChecksum = undef;
    my $iFileSize = undef;
    my $rExtra = undef;
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
        my $hIn,
        my $hOut;
        my $strRemote;
        my $strRemoteOp;
        my $bController = false;

        # If source is remote and destination is local
        if ($bSourceRemote && !$bDestinationRemote)
        {
            $hOut = $hDestinationFile;
            $strRemoteOp = OP_FILE_COPY_OUT;
            $strRemote = 'in';

            if ($strSourcePathType ne PIPE_STDIN)
            {
                $self->{oProtocol}->cmdWrite($strRemoteOp,
                    [$strSourceOp, undef, $bSourceCompressed, $bDestinationCompress, undef, undef, undef, undef, undef, undef,
                        undef, undef, $strExtraFunction]);

                $bController = true;
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
                $self->{oProtocol}->cmdWrite(
                    $strRemoteOp,
                    [undef, $strDestinationOp, $bSourceCompressed, $bDestinationCompress, undef, undef, $strMode,
                        $bDestinationPathCreate, $strUser, $strGroup, $bAppendChecksum, $bPathSync, $strExtraFunction]);

                $bController = true;
            }
        }
        # Else source and destination are remote
        else
        {
            $strRemoteOp = OP_FILE_COPY;

            $self->{oProtocol}->cmdWrite(
                $strRemoteOp,
                [$strSourceOp, $strDestinationOp, $bSourceCompressed, $bDestinationCompress, $bIgnoreMissingSource, undef,
                    $strMode, $bDestinationPathCreate, $strUser, $strGroup, $bAppendChecksum, $bPathSync, $strExtraFunction]);

            $bController = true;
        }

        # Transfer the file (skip this for copies where both sides are remote)
        if ($strRemoteOp ne OP_FILE_COPY)
        {
            ($strChecksum, $iFileSize, $rExtra) =
                $self->{oProtocol}->binaryXfer($hIn, $hOut, $strRemote, $bSourceCompressed, $bDestinationCompress, undef, $fnExtra);
        }

        # If this is the controlling process then wait for OK from remote
        if ($bController)
        {
            # Test for an error when reading output
            my $strOutput;

            eval
            {
                ($bResult, my $strResultChecksum, my $iResultFileSize, my $rResultExtra) =
                    $self->{oProtocol}->outputRead(true, $bIgnoreMissingSource);

                # Check the result of the remote call
                if ($bResult)
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

                        $strChecksum = $strResultChecksum;
                        $iFileSize = $iResultFileSize;
                        $rExtra = $rResultExtra;
                    }
                }

                return true;
            }
            # If there is an error then evaluate
            or do
            {
                my $oException = $EVAL_ERROR;

                # Ignore error if source file was missing and missing file exception was returned and bIgnoreMissingSource is set
                if ($bIgnoreMissingSource && $strRemote eq 'in' && exceptionCode($oException) == ERROR_FILE_MISSING)
                {
                    close($hDestinationFile)
                        or confess &log(ERROR, "cannot close file ${strDestinationTmpOp}");
                    fileRemove($strDestinationTmpOp);

                    $bResult = false;
                }
                else
                {
                    confess $oException;
                }
            };
        }
    }
    # Else this is a local operation
    else
    {
        # If the source is not compressed and the destination is then compress
        if (!$bSourceCompressed && $bDestinationCompress)
        {
            ($strChecksum, $iFileSize, $rExtra) =
                $self->{oProtocol}->binaryXfer($hSourceFile, $hDestinationFile, 'out', false, true, false, $fnExtra);
        }
        # If the source is compressed and the destination is not then decompress
        elsif ($bSourceCompressed && !$bDestinationCompress)
        {
            ($strChecksum, $iFileSize, $rExtra) =
                $self->{oProtocol}->binaryXfer($hSourceFile, $hDestinationFile, 'in', true, false, false, $fnExtra);
        }
        # Else both sides are compressed, so copy capturing checksum
        elsif ($bSourceCompressed)
        {
            ($strChecksum, $iFileSize, $rExtra) =
                $self->{oProtocol}->binaryXfer($hSourceFile, $hDestinationFile, 'out', true, true, false, $fnExtra);
        }
        else
        {
            ($strChecksum, $iFileSize, $rExtra) =
                $self->{oProtocol}->binaryXfer($hSourceFile, $hDestinationFile, 'in', false, true, false, $fnExtra);
        }
    }

    if ($bResult)
    {
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
        if (!(!$bSourceRemote && $bDestinationRemote && $bSourceCompressed) &&
            (!defined($strChecksum) || !defined($iFileSize)))
        {
            confess &log(ASSERT, 'checksum or file size not set');
        }

        # Where the destination is local, set mode, modification time, and perform move to final location
        if (!$bDestinationRemote)
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
            fileMove($strDestinationTmpOp, $strDestinationOp, $bDestinationPathCreate, $bPathSync);
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'bResult', value => $bResult, trace => true},
        {name => 'strChecksum', value => $strChecksum, trace => true},
        {name => 'iFileSize', value => $iFileSize, trace => true},
        {name => '$rExtra', value => $rExtra, trace => true},
    );
}

1;
