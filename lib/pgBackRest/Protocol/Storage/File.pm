####################################################################################################################################
# Protocol file.
####################################################################################################################################
package pgBackRest::Protocol::Storage::File;
use parent 'pgBackRest::Common::Io::Base';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Exporter qw(import);
    our @EXPORT = qw();

use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;

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
        $oProtocol,                                         # Master or minion protocol
        $oFileIo,                                           # File whose results will be returned via protocol
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'oProtocol', trace => true},
            {name => 'oFileIo', required => false, trace => true},
        );

    # Create class
    my $self = $class->SUPER::new($oProtocol->io()->id() . ' file');
    bless $self, $class;

    # Set variables
    $self->{oProtocol} = $oProtocol;
    $self->{oFileIo} = $oFileIo;

    # Set read/write
    $self->{bWrite} = false;

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# read - read block from protocol
####################################################################################################################################
sub read
{
    my $self = shift;
    my $rtBuffer = shift;

    my $lBlockSize;

    # Read the block header and make sure it's valid
    my $strBlockHeader = $self->{oProtocol}->io()->readLine();

    if ($strBlockHeader !~ /^BRBLOCK[0-9]+$/)
    {
        confess &log(ERROR, "invalid block header '${strBlockHeader}'", ERROR_FILE_READ);
    }

    # Get block size from the header
    $lBlockSize = substr($strBlockHeader, 7);

    # Read block if size > 0
    if ($lBlockSize >= 0)
    {
        $self->{oProtocol}->io()->read($rtBuffer, $lBlockSize, true);
    }

    # Return the block size
    return $lBlockSize;
}

####################################################################################################################################
# write - write block to protocol
####################################################################################################################################
sub write
{
    my $self = shift;
    my $rtBuffer = shift;

    # Set write
    $self->{bWrite} = true;

    # Get the buffer size
    my $lBlockSize = defined($rtBuffer) ? length($$rtBuffer) : 0;

    # Write if size > 0 (0 ends the copy stream so it should only be done in close())
    if ($lBlockSize > 0)
    {
        # Write block header to the protocol stream
        $self->{oProtocol}->io()->writeLine("BRBLOCK${lBlockSize}");

        # Write block if size
        $self->{oProtocol}->io()->write($rtBuffer);
    }

    return length($$rtBuffer);
}

####################################################################################################################################
# close - set the result hash
####################################################################################################################################
sub close
{
    my $self = shift;

    # Close if protocol is defined (to prevent this from running more than once)
    if (defined($self->{oProtocol}))
    {
        # If writing output terminator
        if ($self->{bWrite})
        {
            $self->{oProtocol}->io()->writeLine("BRBLOCK0");
        }

        # On master read the results
        if ($self->{oProtocol}->master())
        {
            ($self->{rhResult}) = $self->{oProtocol}->outputRead();

            # Minion will send one more output message after file is closed which can be ignored
            $self->{oProtocol}->outputRead();
        }
        # On minion write the results
        else
        {
            $self->{oProtocol}->outputWrite($self->{oFileIo}->{rhResult});
        }

        # Delete protocol to prevent close from running again
        delete($self->{oProtocol});
    }
}

1;
