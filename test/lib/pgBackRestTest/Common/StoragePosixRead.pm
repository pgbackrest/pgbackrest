####################################################################################################################################
# Posix File Read
####################################################################################################################################
package pgBackRestTest::Common::StoragePosixRead;
use parent 'pgBackRestTest::Common::Io::Handle';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Fcntl qw(O_RDONLY);

use BackRestDoc::Common::Exception;
use BackRestDoc::Common::Log;

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
        $bIgnoreMissing,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'oDriver', trace => true},
            {name => 'strName', trace => true},
            {name => 'bIgnoreMissing', optional => true, default => false, trace => true},
        );

    # Open the file
    my $fhFile;

    if (!sysopen($fhFile, $strName, O_RDONLY))
    {
        if (!($OS_ERROR{ENOENT} && $bIgnoreMissing))
        {
            logErrorResult($OS_ERROR{ENOENT} ? ERROR_FILE_MISSING : ERROR_FILE_OPEN, "unable to open '${strName}'", $OS_ERROR);
        }

        undef($fhFile);
    }

    # Create IO object if open succeeded
    my $self;

    if (defined($fhFile))
    {
        # Set file mode to binary
        binmode($fhFile);

        # Create the class hash
        $self = $class->SUPER::new("'${strName}'", $fhFile);
        bless $self, $class;

        # Set variables
        $self->{oDriver} = $oDriver;
        $self->{strName} = $strName;
        $self->{fhFile} = $fhFile;
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self, trace => true}
    );
}

####################################################################################################################################
# close - close the file
####################################################################################################################################
sub close
{
    my $self = shift;

    if (defined($self->handle()))
    {
        # Close the file
        close($self->handle());
        undef($self->{fhFile});

        # Close parent
        $self->SUPER::close();
    }

    return true;
}

####################################################################################################################################
# Getters
####################################################################################################################################
sub handle {shift->{fhFile}}
sub name {shift->{strName}}

1;
