####################################################################################################################################
# Posix File Write
####################################################################################################################################
package pgBackRestTest::Common::StoragePosixWrite;
use parent 'pgBackRestTest::Common::Io::Handle';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Fcntl qw(O_RDONLY O_WRONLY O_CREAT O_TRUNC);
use File::Basename qw(dirname);

use BackRestDoc::Common::Exception;
use BackRestDoc::Common::Log;

use pgBackRestTest::Common::Io::Handle;
use pgBackRestTest::Common::StorageBase;

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
        # Create temp file name
        $self->{strNameTmp} = "$self->{strName}." . $self->{oDriver}->tempExtension();
    }

    # Open file on first write to avoid creating extraneous files on error
    $self->{bOpened} = false;

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self, trace => true}
    );
}

####################################################################################################################################
# open - open the file
####################################################################################################################################
sub open
{
    my $self = shift;

    # Get the file name
    my $strFile = $self->{bAtomic} ? $self->{strNameTmp} : $self->{strName};

    # Open the file
    if (!sysopen(
        $self->{fhFile}, $strFile, O_WRONLY | O_CREAT | O_TRUNC, oct(defined($self->{strMode}) ? $self->{strMode} : '0666')))
    {
        # If the path does not exist create it if requested
        if ($OS_ERROR{ENOENT} && $self->{bPathCreate})
        {
            $self->{oDriver}->pathCreate(dirname($strFile), {bIgnoreExists => true, bCreateParent => true});
            $self->{bPathCreate} = false;
            return $self->open();
        }

        logErrorResult($OS_ERROR{ENOENT} ? ERROR_PATH_MISSING : ERROR_FILE_OPEN, "unable to open '${strFile}'", $OS_ERROR);
    }

    # Set file mode to binary
    binmode($self->{fhFile});

    # Set the owner
    $self->{oDriver}->owner($strFile, {strUser => $self->{strUser}, strGroup => $self->{strGroup}});

    # Set handle
    $self->handleWriteSet($self->{fhFile});

    # Mark file as opened
    $self->{bOpened} = true;

    return true;
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
# Close the handle if it is open (in case close() was never called)
####################################################################################################################################
sub DESTROY
{
    my $self = shift;

    if (defined($self->handle()))
    {
        CORE::close($self->handle());
        undef($self->{fhFile});
    }
}

####################################################################################################################################
# Getters
####################################################################################################################################
sub handle {shift->{fhFile}}
sub opened {shift->{bOpened}}
sub name {shift->{strName}}

1;
