####################################################################################################################################
# PROTOCOL COMMAND MINION MODULE
####################################################################################################################################
package pgBackRest::Protocol::CommandMinion;
use parent 'pgBackRest::Protocol::CommonMinion';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use JSON::PP;

use pgBackRest::Common::Exception;
use pgBackRest::Common::Ini;
use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::Protocol::CommonMinion;
use pgBackRest::Protocol::IO::ProcessIO;
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
        $strCommand,                                # Command the master process is running
        $iBufferMax,                                # Maximum buffer size
        $iCompressLevel,                            # Set compression level
        $iCompressLevelNetwork,                     # Set compression level for network only compression
        $iProtocolTimeout,                          # Protocol timeout
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'strName'},
            {name => 'strCommand'},
            {name => 'iBufferMax'},
            {name => 'iCompressLevel'},
            {name => 'iCompressLevelNetwork'},
            {name => 'iProtocolTimeout'},
        );

    # Create the class hash
    my $self = $class->SUPER::new(
        $strName, $strCommand,
        new pgBackRest::Protocol::IO::ProcessIO(*STDIN, *STDOUT, *STDERR, undef, undef, $iProtocolTimeout, $iBufferMax),
        $iBufferMax, $iCompressLevel, $iCompressLevelNetwork, $iProtocolTimeout);
    bless $self, $class;

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

1;
