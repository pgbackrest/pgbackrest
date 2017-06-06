####################################################################################################################################
# Posix file.
####################################################################################################################################
package pgBackRest::Storage::Posix::File;
use parent 'pgBackRest::Common::Io::Handle';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Fcntl qw(O_RDONLY O_WRONLY O_CREAT O_TRUNC);
use File::Basename qw(dirname);

use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;

use pgBackRest::Common::Io::Handle;
use pgBackRest::Storage::Base;

####################################################################################################################################
# CONSTRUCTOR
####################################################################################################################################
sub new
{
    my $class = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oDriver,
        $strName,
        $strType,
        $strMode,
        $strUser,
        $strGroup,
        $lTimestamp,
        $bPathCreate,
        $bAtomic,
        $bSync,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'oDriver', trace => true},
            {name => 'strName', trace => true},
            {name => 'strType', trace => true},
            {name => 'strMode', optional => true, trace => true},
            {name => 'strUser', optional => true, trace => true},
            {name => 'strGroup', optional => true, trace => true},
            {name => 'lTimestamp', optional => true, trace => true},
            {name => 'bPathCreate', optional => true, default => false, trace => true},
            {name => 'bAtomic', optional => true, default => false, trace => true},
            {name => 'bSync', optional => true, default => true, trace => true},
        );

    # Create the class hash
    my $self = $class->SUPER::new("'${strName}'");
    bless $self, $class;

    # Set variables
    $self->{oDriver} = $oDriver;
    $self->{strName} = $strName;
    $self->{strType} = $strType;
    $self->{strMode} = $strMode;
    $self->{strUser} = $strUser;
    $self->{strGroup} = $strGroup;
    $self->{lTimestamp} = $lTimestamp;
    $self->{bPathCreate} = $bPathCreate;
    $self->{bAtomic} = $bAtomic;
    $self->{bSync} = $bSync;

    # If atomic create temp filename
    if ($self->{bAtomic})
    {
        # Don't allow atomic when reading
        confess &log(ASSERT, 'atomic valid only for write') if ($self->type() eq STORAGE_FILE_READ);

        # Create temp file name
        $self->{strNameTmp} = "$self->{strName}." . $self->{oDriver}->tempExtension();
    }

    # Only open the file immediately if reading.  Write opens the file on first write to avoid creating extraneous files on error.
    $self->{bOpened} = false;

    if ($self->type() eq STORAGE_FILE_READ)
    {
        $self->open();
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self, trace => true}
    );
}

####################################################################################################################################
# open - open the file.
####################################################################################################################################
sub open
{
    my $self = shift;

    # Get the file name
    my $strFile = $self->{bAtomic} ? $self->{strNameTmp} : $self->{strName};

    # Open the file
    if (!sysopen(
        $self->{fhFile}, $strFile, $self->type() eq STORAGE_FILE_READ ? O_RDONLY : O_WRONLY | O_CREAT | O_TRUNC,
        oct(defined($self->{strMode}) ? $self->{strMode} : '0666')))
    {
        # If the path does not exist create it if requested
        if ($OS_ERROR{ENOENT} && $self->{bPathCreate})
        {
            $self->{oDriver}->pathCreate(dirname($strFile), {bIgnoreExists => true, bCreateParent => true});
            $self->{bPathCreate} = false;
            return $self->open();
        }

        logErrorResult(
            $OS_ERROR{ENOENT} ? ERROR_FILE_MISSING : ERROR_FILE_OPEN, "unable to open '$self->{strName}'", $OS_ERROR);
    }

    # Set file mode to binary
    binmode($self->{fhFile});

    # Set the owner
    $self->{oDriver}->owner($strFile, {strUser => $self->{strUser}, strGroup => $self->{strGroup}});

    # Set handle
    $self->type() eq STORAGE_FILE_READ ? $self->handleReadSet($self->{fhFile}) : $self->handleWriteSet($self->{fhFile});

    # Mark file as opened
    $self->{bOpened} = true;
}

####################################################################################################################################
# write - write data to a file
####################################################################################################################################
sub write
{
    my $self = shift;
    my $rtBuffer = shift;

    # Open file if it is not open already
    $self->open() if !$self->opened();

    return $self->SUPER::write($rtBuffer);
}

####################################################################################################################################
# close - close the file
####################################################################################################################################
sub close
{
    my $self = shift;

    if (defined($self->handle()))
    {
        # Sync the file
        if ($self->{bSync})
        {
            $self->handle()->sync();
        }

        # Close the file
        close($self->handle());
        undef($self->{fhFile});

        # Get current filename
        my $strCurrentName = $self->{bAtomic} ? $self->{strNameTmp} : $self->{strName};

        # Set the modification time
        if (defined($self->{lTimestamp}))
        {
            utime(time(), $self->{lTimestamp}, $strCurrentName)
                or logErrorResult(ERROR_FILE_WRITE, "unable to set time for '${strCurrentName}'", $OS_ERROR);
        }

        # Move the file from temp to final if atomic
        if ($self->{bAtomic})
        {
            $self->{oDriver}->move($strCurrentName, $self->{strName});
        }

        # Set result
        $self->resultSet(COMMON_IO_HANDLE, $self->{lSize});

        # Close parent
        $self->SUPER::close();
    }

    return true;
}

####################################################################################################################################
# Getters
####################################################################################################################################
sub opened {shift->{bOpened}}
sub handle {shift->{fhFile}}
sub type {shift->{strType}}

1;
