####################################################################################################################################
# PROTOCOL COMMAND MINION MODULE
####################################################################################################################################
package pgBackRest::Protocol::Command::Minion;
use parent 'pgBackRest::Protocol::Base::Minion';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use JSON::PP;

use pgBackRest::Common::Exception;
use pgBackRest::Common::Ini;
use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::Protocol::Base::Minion;
use pgBackRest::Common::Io::Buffered;
use pgBackRest::Version;

####################################################################################################################################
# CONSTRUCTOR
####################################################################################################################################
sub new
{
    my $class = shift;                  # Class name

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strName,                                   # Name of the protocol
        $iBufferMax,                                # Maximum buffer size
        $iProtocolTimeout,                          # Protocol timeout
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'strName'},
            {name => 'iBufferMax'},
            {name => 'iProtocolTimeout'},
        );

    # Open buffered protocol io
    my $oIo =
        new pgBackRest::Common::Io::Buffered(
            new pgBackRest::Common::Io::Handle('stdio', *STDIN, *STDOUT), $iProtocolTimeout, $iBufferMax);

    # Create the class hash
    my $self = $class->SUPER::new($strName, $oIo);
    bless $self, $class;

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

1;
